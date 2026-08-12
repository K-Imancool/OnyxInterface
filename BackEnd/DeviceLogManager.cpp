#include "DeviceLogManager.h"

#include "jsonstorage.h"
#include "socketmodel.h"
#include "apppaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonValue>
#include <QMutexLocker>
#include <QHash>
#include <QTextStream>

#include <algorithm>

namespace {
const char *kTotalRuntimeMsKey = "totalRuntimeMs";
const char *kTotalActivationMsKey = "totalActivationMs";
const char *kLegacyLogFileName = "logFile.txt";
const char *kLogFilePrefix = "log-";
const char *kLogFileSuffix = ".txt";
constexpr qint64 kMaxDailyLogBytes = 2 * 1024 * 1024;
constexpr int kWarningFlushCheckMs = 1000;
constexpr qint64 kWarningSummaryIntervalMs = 10000;
constexpr qint64 kWarningQuietPeriodMs = 3000;

QString decodeLogValue(const QString &value)
{
    return QString::fromUtf8(QByteArray::fromBase64(value.toLatin1()));
}
}

DeviceLogManager::DeviceLogManager(JsonStorage *jsonStorage,
                                   SocketModel *socketModel,
                                   QObject *parent)
    : QObject(parent),
      m_jsonStorage(jsonStorage),
      m_socketModel(socketModel)
{
    m_persistTimer.setInterval(60000);
    connect(&m_persistTimer, &QTimer::timeout,
            this, &DeviceLogManager::persistCounters);

    m_warningFlushTimer.setInterval(kWarningFlushCheckMs);
    connect(&m_warningFlushTimer, &QTimer::timeout, this, [this]() {
        flushWarningSummaries(false);
    });
}

QStringList DeviceLogManager::readLogLines(const QString &filter, int maxLines) const
{
    return readLogLines(filter, todayLogDate(), maxLines);
}

QStringList DeviceLogManager::readLogLines(const QString &filter, const QString &date, int maxLines) const
{
    if (maxLines <= 0) {
        maxLines = 1000;
    }

    const QDate targetDate = normalizedLogDate(date);
    const QString filePath = logFilePathForDate(targetDate);

    QMutexLocker locker(&m_mutex);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!lineMatchesFilter(line, filter)) {
            continue;
        }
        lines.append(expandedLogLine(line, targetDate));
        if (lines.size() > maxLines) {
            lines.removeFirst();
        }
    }

    return lines;
}

QStringList DeviceLogManager::availableLogDates() const
{
    return sortedLogDates();
}

QString DeviceLogManager::todayLogDate() const
{
    return formatLogDate(QDate::currentDate());
}

QString DeviceLogManager::adjacentLogDate(const QString &date, int direction) const
{
    if (direction == 0) {
        return normalizedLogDate(date).isValid()
                ? formatLogDate(normalizedLogDate(date))
                : todayLogDate();
    }

    const QStringList dates = sortedLogDates();
    if (dates.isEmpty()) {
        return todayLogDate();
    }

    const QDate current = normalizedLogDate(date);
    int index = dates.indexOf(formatLogDate(current));
    if (index < 0) {
        index = 0;
    }

    index -= direction;
    if (index < 0) {
        index = 0;
    } else if (index >= dates.size()) {
        index = dates.size() - 1;
    }

    return dates.at(index);
}

int DeviceLogManager::logLineCount(const QString &filter, const QString &date) const
{
    const QDate targetDate = normalizedLogDate(date);
    const QString filePath = logFilePathForDate(targetDate);

    QMutexLocker locker(&m_mutex);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }

    int count = 0;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (lineMatchesFilter(line, filter)) {
            ++count;
        }
    }

    return count;
}

QString DeviceLogManager::logFilePath() const
{
    return logFilePathForDate(QDate::currentDate());
}

void DeviceLogManager::beginSession()
{
    if (ensureLogDir()) {
        migrateLegacyLogFile();
    }

    m_runtimeBaseMs = readCounter(QString::fromLatin1(kTotalRuntimeMsKey));
    m_sessionTimer.start();
    m_sessionFinalized = false;
    m_persistTimer.start();
    m_pendingWarnings.clear();
    m_warningFlushTimer.start();

    const QString deviceType = m_jsonStorage
            ? m_jsonStorage->readString(QStringLiteral("deviceType"), QStringLiteral("не указано"))
            : QStringLiteral("не указано");
    const QString serialNumber = m_jsonStorage
            ? m_jsonStorage->readString(QStringLiteral("serialNumber"), QStringLiteral("не указан"))
            : QStringLiteral("не указан");

    appendBootEvent(
            deviceType.trimmed().isEmpty() ? QStringLiteral("не указано") : deviceType.trimmed(),
            serialNumber.trimmed().isEmpty() ? QStringLiteral("не указан") : serialNumber.trimmed(),
            m_runtimeBaseMs,
            readCounter(QString::fromLatin1(kTotalActivationMsKey)));
}

void DeviceLogManager::finalizeSession()
{
    if (m_sessionFinalized) {
        return;
    }

    flushWarningSummaries(true);
    persistCounters();
    m_persistTimer.stop();
    m_warningFlushTimer.stop();
    m_sessionFinalized = true;
}

void DeviceLogManager::persistCounters()
{
    if (m_sessionTimer.isValid()) {
        saveCounter(QString::fromLatin1(kTotalRuntimeMsKey),
                    m_runtimeBaseMs + m_sessionTimer.elapsed());
    }

    qint64 activationDeltaMs = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (m_activation.active && m_activation.timer.isValid()) {
            const qint64 elapsedMs = m_activation.timer.elapsed();
            activationDeltaMs = elapsedMs - m_activation.persistedMs;
            if (activationDeltaMs > 0) {
                m_activation.persistedMs = elapsedMs;
            }
        }
    }

    if (activationDeltaMs > 0) {
        saveCounter(QString::fromLatin1(kTotalActivationMsKey),
                    readCounter(QString::fromLatin1(kTotalActivationMsKey)) + activationDeltaMs);
    }
}

void DeviceLogManager::onActivationStarted(quint8 socketId, bool isCut, quint16 mode, quint16 power,
                                           bool autoMode, quint8 sourceCode)
{
    QMutexLocker locker(&m_mutex);

    m_activation.active = true;
    m_activation.socketId = socketId;
    m_activation.isCut = isCut;
    m_activation.mode = mode;
    m_activation.power = power;
    m_activation.modeId = socketData(
            socketId,
            isCut ? SocketModel::CutModeId : SocketModel::CoagModeId).toInt();
    m_activation.instrumentId = socketData(
            socketId,
            isCut ? SocketModel::CutModeInstrID : SocketModel::CoagModeInstrID).toInt();
    if (m_activation.instrumentId <= 0 || m_activation.instrumentId == 1000) {
        m_activation.instrumentId = 0;
    }
    m_activation.autoMode = autoMode;
    m_activation.sourceCode = sourceCode;
    m_activation.startedAt = QDateTime::currentDateTime();
    m_activation.persistedMs = 0;
    m_activation.timer.start();
}

void DeviceLogManager::onActivationStopped(quint8 stopReason)
{
    Q_UNUSED(stopReason)

    ActivationInfo activation;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_activation.active || !m_activation.timer.isValid()) {
            return;
        }
        activation = m_activation;
        m_activation.active = false;
    }

    const qint64 durationMs = activation.timer.elapsed();
    const qint64 unpersistedDurationMs = durationMs - activation.persistedMs;
    if (unpersistedDurationMs > 0) {
        const qint64 totalActivation = readCounter(QString::fromLatin1(kTotalActivationMsKey)) + unpersistedDurationMs;
        saveCounter(QString::fromLatin1(kTotalActivationMsKey), totalActivation);
    }

    appendActivationEvent(activation, durationMs);
}

void DeviceLogManager::onWarningCode(quint8 warningCode)
{
    if (warningCode == 0x40) {
        return;
    }

    flushWarningSummaries(false);

    const QDateTime now = QDateTime::currentDateTime();
    auto accumulator = m_pendingWarnings.find(warningCode);
    if (accumulator == m_pendingWarnings.end()) {
        appendWarningEvent(warningCode);

        WarningAccumulator newAccumulator;
        newAccumulator.lastSeen = now;
        newAccumulator.summaryStartedAt = now;
        m_pendingWarnings.insert(warningCode, newAccumulator);
        return;
    }

    accumulator->lastSeen = now;
    ++accumulator->suppressedCount;
}

void DeviceLogManager::logPowerOff(quint8 reasonCode)
{
    appendPowerOffEvent(reasonCode);
}

QString DeviceLogManager::logDirPath() const
{
    return AppPaths::instance().onyxLogDir();
}

bool DeviceLogManager::ensureLogDir() const
{
    QDir dir(logDirPath());
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

QString DeviceLogManager::logFileNameForDate(const QDate &date) const
{
    return QString::fromLatin1(kLogFilePrefix)
            + formatLogDate(date)
            + QString::fromLatin1(kLogFileSuffix);
}

QString DeviceLogManager::logFilePathForDate(const QDate &date) const
{
    return logDirPath() + QLatin1Char('/') + logFileNameForDate(date);
}

QDate DeviceLogManager::dateFromLogFileName(const QString &fileName) const
{
    if (!fileName.startsWith(QLatin1String(kLogFilePrefix))
        || !fileName.endsWith(QLatin1String(kLogFileSuffix))) {
        return {};
    }

    const QString prefix = QString::fromLatin1(kLogFilePrefix);
    const QString suffix = QString::fromLatin1(kLogFileSuffix);
    const QString datePart = fileName.mid(prefix.size(),
                                          fileName.size() - prefix.size() - suffix.size());
    return QDate::fromString(datePart, QStringLiteral("dd-MM-yyyy"));
}

QString DeviceLogManager::formatLogDate(const QDate &date) const
{
    return date.toString(QStringLiteral("dd-MM-yyyy"));
}

QDate DeviceLogManager::normalizedLogDate(const QString &date) const
{
    const QString trimmed = date.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("all")) {
        return QDate::currentDate();
    }

    const QDate parsed = QDate::fromString(trimmed, QStringLiteral("dd-MM-yyyy"));
    return parsed.isValid() ? parsed : QDate::currentDate();
}

QStringList DeviceLogManager::sortedLogDates() const
{
    QMutexLocker locker(&m_mutex);
    QDir dir(logDirPath());
    if (!dir.exists()) {
        return {};
    }

    QList<QDate> dates;
    const QFileInfoList files = dir.entryInfoList(
        QStringList() << QString::fromLatin1(kLogFilePrefix) + QStringLiteral("*") + QString::fromLatin1(kLogFileSuffix),
        QDir::Files,
        QDir::Name);

    for (const QFileInfo &info : files) {
        const QDate date = dateFromLogFileName(info.fileName());
        if (date.isValid()) {
            dates.append(date);
        }
    }

    std::sort(dates.begin(), dates.end());
    std::reverse(dates.begin(), dates.end());

    QStringList result;
    result.reserve(dates.size());
    for (const QDate &date : qAsConst(dates)) {
        result.append(formatLogDate(date));
    }

    return result;
}

void DeviceLogManager::migrateLegacyLogFile() const
{
    const QString legacyPath = logDirPath() + QLatin1Char('/') + QLatin1String(kLegacyLogFileName);
    QFile legacyFile(legacyPath);
    if (!legacyFile.exists()) {
        return;
    }

    if (!legacyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QHash<QString, QStringList> linesByDate;
    QTextStream in(&legacyFile);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.size() < 10) {
            continue;
        }

        const QString dateKey = line.left(10);
        if (!QDate::fromString(dateKey, QStringLiteral("dd-MM-yyyy")).isValid()) {
            continue;
        }
        linesByDate[dateKey].append(line);
    }
    legacyFile.close();

    for (auto it = linesByDate.constBegin(); it != linesByDate.constEnd(); ++it) {
        const QDate date = QDate::fromString(it.key(), QStringLiteral("dd-MM-yyyy"));
        const QString targetPath = logFilePathForDate(date);
        QFile targetFile(targetPath);
        const bool existed = targetFile.exists();
        if (!targetFile.open(existed ? QIODevice::Append : QIODevice::WriteOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream out(&targetFile);
        for (const QString &line : it.value()) {
            out << line << Qt::endl;
        }
    }

    const QString backupPath = legacyPath + QStringLiteral(".migrated");
    QFile::remove(backupPath);
    legacyFile.rename(backupPath);
}

bool DeviceLogManager::isDailyLogOverSizeLimit(const QString &filePath) const
{
    const QFileInfo info(filePath);
    return info.exists() && info.size() >= kMaxDailyLogBytes;
}

void DeviceLogManager::appendEvent(const QString &category, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    if (!ensureLogDir()) {
        return;
    }

    const QString filePath = logFilePathForDate(QDate::currentDate());
    if (isDailyLogOverSizeLimit(filePath)) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString(QStringLiteral("dd-MM-yyyy hh:mm:ss.zzz "))
        << QStringLiteral("[%1] ").arg(category)
        << message
        << Qt::endl;
}

void DeviceLogManager::appendCompactEvent(const QStringList &fields)
{
    if (fields.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    if (!ensureLogDir()) {
        return;
    }

    const QString filePath = logFilePathForDate(QDate::currentDate());
    if (isDailyLogOverSizeLimit(filePath)) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << QTime::currentTime().toString(QStringLiteral("hh:mm:ss.zzz"))
        << QLatin1Char('|')
        << fields.join(QLatin1Char('|'))
        << Qt::endl;
}

void DeviceLogManager::appendBootEvent(const QString &deviceType,
                                       const QString &serialNumber,
                                       qint64 runtimeMs,
                                       qint64 activationMs)
{
    QString deviceCode = deviceType;
    if (deviceCode.startsWith(QStringLiteral("ONYX-"))) {
        deviceCode.remove(0, 5);
    }

    appendCompactEvent({
        QStringLiteral("B"),
        deviceCode,
        serialNumber,
        formatCompactTotalDuration(runtimeMs),
        QStringLiteral("A%1").arg(formatCompactTotalDuration(activationMs))
    });
}

void DeviceLogManager::appendActivationEvent(const ActivationInfo &activation,
                                             qint64 durationMs)
{
    appendCompactEvent({
        QStringLiteral("A"),
        activationOutputCode(activation.socketId),
        QStringLiteral("Mod%1").arg(activation.modeId),
        QStringLiteral("P%1").arg(activation.power),
        QStringLiteral("I%1").arg(activation.instrumentId),
        formatCompactDuration(durationMs),
        activationSourceCode(activation.autoMode, activation.sourceCode)
    });
}

void DeviceLogManager::appendPowerOffEvent(quint8 reasonCode)
{
    appendCompactEvent({
        QStringLiteral("P"),
        QString::number(reasonCode)
    });
}

void DeviceLogManager::appendWarningEvent(quint8 warningCode,
                                          qint64 repeatedCount,
                                          qint64 periodSeconds)
{
    QStringList fields{
        QStringLiteral("E"),
        QString::number(warningCode, 16)
            .rightJustified(2, QLatin1Char('0'))
            .toUpper()
    };
    if (repeatedCount > 0) {
        fields.append(QString::number(repeatedCount));
        fields.append(QString::number(periodSeconds));
    }
    appendCompactEvent(fields);
}

QString DeviceLogManager::warningMessage(quint8 warningCode) const
{
    const int code = static_cast<int>(warningCode);
    const QString codeText = QString::number(code, 16)
            .rightJustified(2, QLatin1Char('0'))
            .toUpper();
    return QStringLiteral("Ошибка; код=0x%1; текст=%2")
            .arg(codeText)
            .arg(warningTextForCode(code));
}

QString DeviceLogManager::expandedLogLine(const QString &line, const QDate &date) const
{
    const QStringList parts = line.split(QLatin1Char('|'));
    if (parts.size() < 2) {
        return line;
    }

    const QTime time = QTime::fromString(parts.at(0), QStringLiteral("hh:mm:ss.zzz"));
    if (!time.isValid()) {
        return line;
    }

    const QString timestamp =
            QDateTime(date, time).toString(QStringLiteral("dd-MM-yyyy hh:mm:ss.zzz"));

    if (parts.at(1) == QStringLiteral("E")
            && (parts.size() == 3 || parts.size() == 5)) {
        bool codeOk = false;
        const int code = parts.at(2).toInt(&codeOk, 16);
        if (!codeOk || code < 0 || code > 0xFF) {
            return line;
        }

        QString expanded = QStringLiteral("%1 [ERROR] %2")
                .arg(timestamp)
                .arg(warningMessage(static_cast<quint8>(code)));

        if (parts.size() == 3) {
            return expanded;
        }

        bool countOk = false;
        bool periodOk = false;
        const qint64 repeatedCount = parts.at(3).toLongLong(&countOk);
        const qint64 periodSeconds = parts.at(4).toLongLong(&periodOk);
        if (!countOk || !periodOk || repeatedCount <= 0 || periodSeconds < 0) {
            return line;
        }

        expanded += QStringLiteral("; повторов=%1; период=%2 сек")
                .arg(repeatedCount)
                .arg(periodSeconds);
        return expanded;
    }

    if (parts.at(1) == QStringLiteral("B") && parts.size() == 6) {
        if (parts.at(4).contains(QLatin1Char('h'))
                && parts.at(4).endsWith(QLatin1Char('m'))
                && parts.at(5).startsWith(QLatin1Char('A'))) {
            QString runtime = parts.at(4);
            runtime.replace(QLatin1Char('h'), QStringLiteral(" ч "));
            runtime.replace(QLatin1Char('m'), QStringLiteral(" мин"));

            QString activation = parts.at(5).mid(1);
            activation.replace(QLatin1Char('h'), QStringLiteral(" ч "));
            activation.replace(QLatin1Char('m'), QStringLiteral(" мин"));

            return QStringLiteral("%1 [BOOT] Включение аппарата; тип=ONYX-%2; серийный номер=%3; общая наработка=%4; общее время активации=%5")
                    .arg(timestamp)
                    .arg(parts.at(2))
                    .arg(parts.at(3))
                    .arg(runtime)
                    .arg(activation);
        }

        bool runtimeOk = false;
        bool activationOk = false;
        const qint64 runtimeMs = parts.at(4).toLongLong(&runtimeOk);
        const qint64 activationMs = parts.at(5).toLongLong(&activationOk);
        if (!runtimeOk || !activationOk || runtimeMs < 0 || activationMs < 0) {
            return line;
        }

        return QStringLiteral("%1 [BOOT] Включение аппарата; тип=%2; серийный номер=%3; общая наработка=%4; общее время активации=%5")
                .arg(timestamp)
                .arg(decodeLogValue(parts.at(2)))
                .arg(decodeLogValue(parts.at(3)))
                .arg(formatTotalDuration(runtimeMs))
                .arg(formatTotalDuration(activationMs));
    }

    if (parts.at(1) == QStringLiteral("A") && parts.size() == 8
            && parts.at(3).startsWith(QStringLiteral("Mod"))
            && parts.at(4).startsWith(QLatin1Char('P'))
            && parts.at(5).startsWith(QLatin1Char('I'))) {
        const QHash<QString, QString> outputNames{
            {QStringLiteral("M1"), QStringLiteral("МОНО1")},
            {QStringLiteral("M2"), QStringLiteral("МОНО2")},
            {QStringLiteral("B1"), QStringLiteral("БИ1")},
            {QStringLiteral("B2"), QStringLiteral("БИ2")}
        };
        const QHash<QString, QString> sourceNames{
            {QStringLiteral("P1"), QStringLiteral("педаль 1")},
            {QStringLiteral("P2"), QStringLiteral("педаль 2")},
            {QStringLiteral("H1"), QStringLiteral("кнопка держателя МОНО1")},
            {QStringLiteral("H2"), QStringLiteral("кнопка держателя МОНО2")},
            {QStringLiteral("T"), QStringLiteral("термозажим")},
            {QStringLiteral("Auto"), QStringLiteral("автозапуск")}
        };

        bool modeOk = false;
        bool instrumentOk = false;
        const int modeId = parts.at(3).mid(3).toInt(&modeOk);
        const int instrumentId = parts.at(5).mid(1).toInt(&instrumentOk);
        if (!modeOk || !instrumentOk) {
            return line;
        }

        QString modeName = m_socketModel
                ? m_socketModel->modeNameById(modeId)
                : QString();
        if (modeName.isEmpty()) {
            modeName = QStringLiteral("Неизвестный режим (ID=%1)").arg(modeId);
        }

        QString instrumentName = m_socketModel
                ? m_socketModel->instrumentNameById(instrumentId)
                : QString();
        if (instrumentName.isEmpty()) {
            instrumentName = instrumentId == 0
                    ? QStringLiteral("Другой инструмент")
                    : QStringLiteral("Неизвестный инструмент (ID=%1)").arg(instrumentId);
        }

        QString duration = parts.at(6);
        duration.replace(QLatin1Char('m'), QStringLiteral(" мин "));
        duration.replace(QLatin1Char('s'), QStringLiteral(" сек"));

        return QStringLiteral("%1 [ACTIVATION] Активация; выход=%2; режим=%3; мощность=%4; инструмент=%5; длительность=%6; источник=%7")
                .arg(timestamp)
                .arg(outputNames.value(parts.at(2), parts.at(2)))
                .arg(modeName)
                .arg(parts.at(4).mid(1))
                .arg(instrumentName)
                .arg(duration)
                .arg(sourceNames.value(parts.at(7), parts.at(7)));
    }

    if (parts.at(1) == QStringLiteral("A") && parts.size() == 9) {
        bool modeOk = false;
        bool powerOk = false;
        bool durationOk = false;
        bool autoModeOk = false;
        const int mode = parts.at(4).toInt(&modeOk);
        const int power = parts.at(5).toInt(&powerOk);
        const qint64 durationMs = parts.at(7).toLongLong(&durationOk);
        const int autoMode = parts.at(8).toInt(&autoModeOk);
        if (!modeOk || !powerOk || !durationOk || !autoModeOk
                || durationMs < 0 || (autoMode != 0 && autoMode != 1)) {
            return line;
        }

        return QStringLiteral("%1 [ACTIVATION] Активация; выход=%2; режим=%3 (%4); мощность=%5; инструмент=%6; длительность=%7; источник=%8")
                .arg(timestamp)
                .arg(decodeLogValue(parts.at(2)))
                .arg(decodeLogValue(parts.at(3)))
                .arg(mode)
                .arg(power)
                .arg(decodeLogValue(parts.at(6)))
                .arg(formatDuration(durationMs))
                .arg(sourceText(autoMode != 0, 0));
    }

    if (parts.at(1) == QStringLiteral("P") && parts.size() == 3) {
        bool codeOk = false;
        const int code = parts.at(2).toInt(&codeOk);
        if (!codeOk) {
            return line;
        }
        return QStringLiteral("%1 [POWER_OFF] %2")
                .arg(timestamp)
                .arg(powerOffTextForCode(code));
    }

    return line;
}

void DeviceLogManager::flushWarningSummaries(bool force)
{
    const QDateTime now = QDateTime::currentDateTime();
    auto accumulator = m_pendingWarnings.begin();
    while (accumulator != m_pendingWarnings.end()) {
        const bool quiet = accumulator->lastSeen.msecsTo(now) >= kWarningQuietPeriodMs;
        const bool summaryDue =
                accumulator->summaryStartedAt.msecsTo(now) >= kWarningSummaryIntervalMs;

        if (accumulator->suppressedCount > 0 && (force || quiet || summaryDue)) {
            const qint64 periodMs =
                    accumulator->summaryStartedAt.msecsTo(accumulator->lastSeen);
            appendWarningEvent(accumulator.key(),
                               accumulator->suppressedCount,
                               qMax<qint64>(1, periodMs / 1000));

            accumulator->suppressedCount = 0;
            accumulator->summaryStartedAt = now;
        }

        if (force || quiet) {
            accumulator = m_pendingWarnings.erase(accumulator);
        } else {
            ++accumulator;
        }
    }
}

bool DeviceLogManager::lineMatchesFilter(const QString &line, const QString &filter) const
{
    const QString normalized = filter.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("all")) {
        return true;
    }
    if (normalized == QStringLiteral("errors")) {
        return line.contains(QStringLiteral("[ERROR]"))
                || line.contains(QStringLiteral("|E|"));
    }
    if (normalized == QStringLiteral("boots")) {
        return line.contains(QStringLiteral("[BOOT]"))
                || line.contains(QStringLiteral("|B|"));
    }
    return true;
}

QString DeviceLogManager::formatDuration(qint64 milliseconds) const
{
    if (milliseconds < 0) {
        milliseconds = 0;
    }

    const qint64 totalSeconds = milliseconds / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1 ч %2 мин %3 сек").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
}

QString DeviceLogManager::formatTotalDuration(qint64 milliseconds) const
{
    if (milliseconds < 0) {
        milliseconds = 0;
    }

    const qint64 totalMinutes = milliseconds / 60000;
    const qint64 hours = totalMinutes / 60;
    const qint64 minutes = totalMinutes % 60;
    return QStringLiteral("%1 ч %2 мин").arg(hours).arg(minutes, 2, 10, QLatin1Char('0'));
}

QString DeviceLogManager::formatCompactDuration(qint64 milliseconds) const
{
    const qint64 totalSeconds = qMax<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1m%2s")
            .arg(totalSeconds / 60)
            .arg(totalSeconds % 60);
}

QString DeviceLogManager::formatCompactTotalDuration(qint64 milliseconds) const
{
    const qint64 totalMinutes = qMax<qint64>(0, milliseconds) / 60000;
    return QStringLiteral("%1h%2m")
            .arg(totalMinutes / 60)
            .arg(totalMinutes % 60);
}

QString DeviceLogManager::activationOutputCode(quint8 socketId) const
{
    switch (socketId) {
    case 0: return QStringLiteral("B1");
    case 1: return QStringLiteral("B2");
    case 2: return QStringLiteral("M1");
    case 3: return QStringLiteral("M2");
    default: return QStringLiteral("O%1").arg(socketId);
    }
}

QString DeviceLogManager::activationSourceCode(bool autoMode, quint8 sourceCode) const
{
    if (autoMode) {
        return QStringLiteral("Auto");
    }

    switch (sourceCode) {
    case 0x04:
        return QStringLiteral("P1");
    case 0x01:
    case 0x02:
    case 0x03:
        return QStringLiteral("P2");
    case 0x40:
    case 0x80:
    case 0xC0:
        return QStringLiteral("H1");
    case 0x10:
    case 0x20:
    case 0x30:
        return QStringLiteral("H2");
    case 0x08:
        return QStringLiteral("T");
    default:
        return QStringLiteral("S%1")
                .arg(sourceCode, 2, 16, QLatin1Char('0'))
                .toUpper();
    }
}

QString DeviceLogManager::warningTextForCode(int code) const
{
    switch (code) {
    case 0x01: return QStringLiteral("Ошибка связи: передача не выполнена");
    case 0x02: return QStringLiteral("Ошибка связи: нет ответа");
    case 0x03: return QStringLiteral("Ошибка связи: неверный ответ");
    case 0x04: return QStringLiteral("Ошибка связи: неверная длина пакета");
    case 0x05: return QStringLiteral("Ошибка связи: CRC не совпадает");
    case 0x41: return QStringLiteral("Активация остановлена: холостой ход (автостоп)");
    case 0x42: return QStringLiteral("Активация остановлена: короткое замыкание бранш");
    case 0x43: return QStringLiteral("Активация остановлена: обрыв нейтрального электрода");
    case 0x44: return QStringLiteral("Активация остановлена: закончился аргон");
    case 0x45: return QStringLiteral("Активация остановлена: непроходимость газового тракта");
    case 0x46: return QStringLiteral("Ошибка модуля связи");
    case 0x4F: return QStringLiteral("Активация остановлена: ошибка генератора");
    case 0x80: return QStringLiteral("Ошибка: модуль связи не принимает сигналы от МИФ");
    case 0x81: return QStringLiteral("Ошибка: генератор не отвечает");
    case 0x82: return QStringLiteral("Ошибка: газовый модуль не отвечает");
    case 0x83: return QStringLiteral("Ошибка: не отвечает радиомодуль");
    case 0x84: return QStringLiteral("Ошибка: кнопки или педали зажаты до старта");
    case 0x85: return QStringLiteral("Ошибка: МК НЭ не отвечает");
    case 0x86: return QStringLiteral("Ошибка: МК раскачки не отвечает");
    case 0x87: return QStringLiteral("Ошибка: питание НЭ 5В не соответствует норме");
    case 0x88: return QStringLiteral("Ошибка: питание НЭ 3,3В не соответствует норме");
    case 0x89: return QStringLiteral("Ошибка: перегрев контроллера НЭ");
    case 0x8D: return QStringLiteral("Ошибка: обновление не выполнено");
    case 0x8E: return QStringLiteral("Ошибка: нет рабочей прошивки МУС");
    case 0x90: return QStringLiteral("Критичная ошибка: ИСН при включении");
    case 0x91: return QStringLiteral("Критичная ошибка: АЦП1 (напряжение контура)");
    case 0x92: return QStringLiteral("Критичная ошибка: АЦП2 (ток контура)");
    case 0x93: return QStringLiteral("Критичная ошибка: АЦП3 (ток генератора)");
    case 0x94: return QStringLiteral("Критичная ошибка: АЦП4 (напряжение ИСН)");
    case 0x95: return QStringLiteral("Критичная ошибка: реле");
    case 0x96: return QStringLiteral("Критичная ошибка: ИСН при нормальной работе");
    case 0x97: return QStringLiteral("Критичная ошибка: не найден резонанс при калибровке НЭ");
    case 0x98: return QStringLiteral("Критичная ошибка: АЦП схемы НЭ");
    default:
    {
        const QString codeText = QString::number(code, 16).rightJustified(2, QLatin1Char('0')).toUpper();
        return QStringLiteral("Ошибка устройства (код 0x%1)").arg(codeText);
    }
    }
}

QString DeviceLogManager::powerOffTextForCode(int code) const
{
    switch (code) {
    case UartPowerOff:
        return QStringLiteral("Выключение подтверждено по команде UART");
    case ServicePowerOff:
        return QStringLiteral("Выключение через сервисное меню");
    case ServiceReboot:
        return QStringLiteral("Перезагрузка через сервисное меню");
    case PowerOffCancelled:
        return QStringLiteral("Выключение отменено пользователем");
    case PowerOffFailed:
        return QStringLiteral("Не удалось запустить системную команду выключения");
    case RebootFailed:
        return QStringLiteral("Не удалось запустить системную команду перезагрузки");
    default:
        return QStringLiteral("Причина выключения: код %1").arg(code);
    }
}

QString DeviceLogManager::sourceText(bool autoMode, quint8 sourceCode) const
{
    Q_UNUSED(sourceCode)
    return autoMode ? QStringLiteral("автозапуск") : QStringLiteral("педаль");
}

QString DeviceLogManager::socketData(int socketId, int role) const
{
    if (!m_socketModel) {
        return {};
    }

    const QModelIndex index = m_socketModel->index(socketId, 0);
    if (!index.isValid()) {
        return {};
    }

    return m_socketModel->data(index, role).toString();
}

qint64 DeviceLogManager::readCounter(const QString &key) const
{
    if (!m_jsonStorage) {
        return 0;
    }

    QJsonValue value;
    if (!m_jsonStorage->read(key, &value)) {
        return 0;
    }

    if (value.isString()) {
        return value.toString().toLongLong();
    }
    return static_cast<qint64>(value.toDouble(0));
}

void DeviceLogManager::saveCounter(const QString &key, qint64 value)
{
    if (!m_jsonStorage) {
        return;
    }
    m_jsonStorage->save(key, QJsonValue(static_cast<double>(value)));
}
