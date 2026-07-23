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
        lines.append(line);
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

    const QString deviceType = m_jsonStorage
            ? m_jsonStorage->readString(QStringLiteral("deviceType"), QStringLiteral("не указано"))
            : QStringLiteral("не указано");
    const QString serialNumber = m_jsonStorage
            ? m_jsonStorage->readString(QStringLiteral("serialNumber"), QStringLiteral("не указан"))
            : QStringLiteral("не указан");

    appendEvent(QStringLiteral("BOOT"),
                QStringLiteral("Включение аппарата; тип=%1; серийный номер=%2; общая наработка=%3; общее время активации=%4")
                    .arg(deviceType.trimmed().isEmpty() ? QStringLiteral("не указано") : deviceType.trimmed())
                    .arg(serialNumber.trimmed().isEmpty() ? QStringLiteral("не указан") : serialNumber.trimmed())
                    .arg(formatTotalDuration(m_runtimeBaseMs))
                    .arg(formatTotalDuration(readCounter(QString::fromLatin1(kTotalActivationMsKey)))));
}

void DeviceLogManager::finalizeSession()
{
    if (m_sessionFinalized) {
        return;
    }

    persistCounters();
    m_persistTimer.stop();
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
    m_activation.autoMode = autoMode;
    m_activation.sourceCode = sourceCode;
    m_activation.output = socketData(socketId, SocketModel::SocketName);
    m_activation.modeName = socketData(socketId, isCut ? SocketModel::CutModeName : SocketModel::CoagModeName);
    m_activation.instrument = socketData(socketId, isCut ? SocketModel::CutModeInstrName : SocketModel::CoagModeInstrName);
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

    appendEvent(QStringLiteral("ACTIVATION"),
                QStringLiteral("Активация; выход=%1; режим=%2 (%3); мощность=%4; инструмент=%5; длительность=%6; источник=%7")
                    .arg(activation.output.isEmpty() ? QString::number(activation.socketId + 1) : activation.output)
                    .arg(activation.modeName.isEmpty() ? QStringLiteral("не указан") : activation.modeName)
                    .arg(activation.mode)
                    .arg(activation.power)
                    .arg(activation.instrument.isEmpty() ? QStringLiteral("Другой инструмент") : activation.instrument)
                    .arg(formatDuration(durationMs))
                    .arg(sourceText(activation.autoMode, activation.sourceCode)));
}

void DeviceLogManager::onWarningCode(quint8 warningCode)
{
    if (warningCode == 0x40) {
        return;
    }

    const int code = static_cast<int>(warningCode);
    const QString codeText = QString::number(code, 16).rightJustified(2, QLatin1Char('0')).toUpper();
    appendEvent(QStringLiteral("ERROR"),
                QStringLiteral("Ошибка; код=0x%1; текст=%2")
                    .arg(codeText)
                    .arg(warningTextForCode(code)));
}

void DeviceLogManager::logPowerOff(const QString &message)
{
    appendEvent(QStringLiteral("POWER_OFF"), message);
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
    if (category == QStringLiteral("ERROR") && isDailyLogOverSizeLimit(filePath)) {
        if (appendOrUpdateRepeatedError(filePath, message)) {
            return;
        }
        return;
    }

    if (category == QStringLiteral("ERROR") && appendOrUpdateRepeatedError(filePath, message)) {
        return;
    }

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

bool DeviceLogManager::appendOrUpdateRepeatedError(const QString &filePath, const QString &message)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    if (lines.isEmpty()) {
        return false;
    }

    const QString lastMessage = repeatedEventMessage(lines.constLast(), QStringLiteral("ERROR"));
    if (lastMessage != message) {
        return false;
    }

    const int nextCount = repeatedEventCount(lines.constLast()) + 1;
    lines.last() = withRepeatedEventCount(lines.constLast(), nextCount);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    for (const QString &line : qAsConst(lines)) {
        out << line << Qt::endl;
    }
    return true;
}

QString DeviceLogManager::repeatedEventMessage(const QString &line, const QString &category) const
{
    const QString marker = QStringLiteral("[%1] ").arg(category);
    const int markerIndex = line.indexOf(marker);
    if (markerIndex < 0) {
        return {};
    }

    QString message = line.mid(markerIndex + marker.length());
    const int countStart = message.lastIndexOf(QStringLiteral(" (x"));
    if (countStart < 0 || !message.endsWith(QLatin1Char(')'))) {
        return message;
    }

    bool ok = false;
    message.mid(countStart + 3, message.length() - countStart - 4).toInt(&ok);
    return ok ? message.left(countStart) : message;
}

int DeviceLogManager::repeatedEventCount(const QString &line) const
{
    const int countStart = line.lastIndexOf(QStringLiteral(" (x"));
    if (countStart < 0 || !line.endsWith(QLatin1Char(')'))) {
        return 1;
    }

    bool ok = false;
    const int count = line.mid(countStart + 3, line.length() - countStart - 4).toInt(&ok);
    return ok && count > 0 ? count : 1;
}

QString DeviceLogManager::withRepeatedEventCount(const QString &line, int count) const
{
    const int countStart = line.lastIndexOf(QStringLiteral(" (x"));
    if (countStart >= 0 && line.endsWith(QLatin1Char(')'))) {
        bool ok = false;
        line.mid(countStart + 3, line.length() - countStart - 4).toInt(&ok);
        if (ok) {
            return line.left(countStart) + QStringLiteral(" (x%1)").arg(count);
        }
    }

    return line + QStringLiteral(" (x%1)").arg(count);
}

bool DeviceLogManager::lineMatchesFilter(const QString &line, const QString &filter) const
{
    const QString normalized = filter.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("all")) {
        return true;
    }
    if (normalized == QStringLiteral("errors")) {
        return line.contains(QStringLiteral("[ERROR]"));
    }
    if (normalized == QStringLiteral("boots")) {
        return line.contains(QStringLiteral("[BOOT]"));
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
