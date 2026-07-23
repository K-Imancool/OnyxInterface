#include "HttpUploadController.h"
#include "jsonstorage.h"
#include "linkstm.h"
#include "apppaths.h"

#include <QAbstractSocket>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>
#include <QPointer>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QMetaObject>
#include <QPointer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

const char kDefaultUploadDir[] = "/var/lib/qtpr/incoming";
const char kReleaseZipPassword[] = "Electrosurgical";
const QRegularExpression kReleaseZipNameRe(
    QStringLiteral("^.+-(\\d+)\\.(\\d+)-(\\d+)\\.(\\d+)-(\\d+)\\.zip$"),
    QRegularExpression::CaseInsensitiveOption);
const QRegularExpression kFirmwareFileRe(
    QStringLiteral("^(COM|ARG|GEN)-([0-9]+(?:\\.[0-9]+)*)\\.hex$"),
    QRegularExpression::CaseInsensitiveOption);
const QRegularExpression kSimpleVersionRe(
    QStringLiteral("^\\d+(?:\\.\\d+)*$"));
const char kUploadFirewallGuardPath[] = "/usr/local/sbin/upload-fw-guard";

QString ipv4ToQString(const QHostAddress &a)
{
    return a.toString();
}

QString normalizedBaseUrl(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return QString();
    }
    if (!value.endsWith(QLatin1Char('/'))) {
        value += QLatin1Char('/');
    }
    return value;
}

bool parseBoolString(const QString &value)
{
    const QString v = value.trimmed().toLower();
    return v == QStringLiteral("1")
            || v == QStringLiteral("true")
            || v == QStringLiteral("yes")
            || v == QStringLiteral("on");
}

QString wifiQrEscape(QString value)
{
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral(";"), QStringLiteral("\\;"));
    value.replace(QStringLiteral(","), QStringLiteral("\\,"));
    value.replace(QStringLiteral(":"), QStringLiteral("\\:"));
    value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return value;
}

QString selectPayloadRoot(const QString &extractRoot)
{
    QDir root(extractRoot);
    const QFileInfoList entries = root.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name);
    if (entries.size() == 1 && entries.first().isDir()) {
        return entries.first().absoluteFilePath();
    }
    return extractRoot;
}

QString findManifestPathRecursive(const QString &rootDir)
{
    QDirIterator it(rootDir, QStringList() << QStringLiteral("update-manifest.json"),
                    QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        return it.next();
    }
    return QString();
}

QString resolveUnzipProgramPath()
{
    QString program = QStandardPaths::findExecutable(QStringLiteral("unzip"));
    if (!program.isEmpty()) {
        return program;
    }
    const QStringList candidates = {
        QStringLiteral("/usr/bin/unzip"),
        QStringLiteral("/bin/unzip"),
        QStringLiteral("/usr/local/bin/unzip")
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p) && QFileInfo(p).isExecutable()) {
            return p;
        }
    }
    return QString();
}

QString requestQueryValue(const QString &requestTarget, const QString &key)
{
    const int q = requestTarget.indexOf(QLatin1Char('?'));
    if (q < 0) {
        return QString();
    }
    QUrlQuery query(requestTarget.mid(q + 1));
    return query.queryItemValue(key);
}

QString logArchiveCacheFilePath(const QString &sessionToken)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    return QDir(base).filePath(QStringLiteral("onyxlog-%1.zip").arg(sessionToken));
}

QHostAddress normalizeClientAddress(const QHostAddress &addr)
{
    if (addr.isNull()) {
        return addr;
    }
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        return addr;
    }
    if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        bool mapped = false;
        const quint32 v4 = addr.toIPv4Address(&mapped);
        if (mapped) {
            return QHostAddress(v4);
        }
    }
    return addr;
}

bool writeSocketAll(QTcpSocket *socket, const QByteArray &data, int timeoutMs = 120000)
{
    if (!socket || data.isEmpty()) {
        return true;
    }
    qint64 offset = 0;
    while (offset < data.size()) {
        if (socket->state() != QAbstractSocket::ConnectedState) {
            qWarning() << "HttpUploadController: writeSocketAll socket not connected, state="
                       << socket->state() << "written=" << offset << "of" << data.size();
            return false;
        }
        const qint64 chunk = socket->write(data.constData() + offset, data.size() - offset);
        if (chunk < 0) {
            qWarning() << "HttpUploadController: writeSocketAll write error at offset" << offset
                       << "error=" << socket->errorString();
            return false;
        }
        if (chunk == 0) {
            if (!socket->waitForBytesWritten(timeoutMs)) {
                qWarning() << "HttpUploadController: writeSocketAll waitForBytesWritten timeout at"
                           << offset << "of" << data.size();
                return false;
            }
            continue;
        }
        offset += chunk;
        if (!socket->waitForBytesWritten(timeoutMs)) {
            qWarning() << "HttpUploadController: writeSocketAll flush timeout at" << offset
                       << "of" << data.size();
            return false;
        }
    }
    return true;
}

QString resolveZipProgramPath()
{
    QString program = QStandardPaths::findExecutable(QStringLiteral("zip"));
    if (!program.isEmpty()) {
        return program;
    }
    const QStringList candidates = {
        QStringLiteral("/usr/bin/zip"),
        QStringLiteral("/bin/zip"),
        QStringLiteral("/usr/local/bin/zip")
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p) && QFileInfo(p).isExecutable()) {
            return p;
        }
    }
    return QString();
}

bool copyDirectoryRecursive(const QString &srcDir, const QString &dstDir)
{
    QDir src(srcDir);
    if (!src.exists()) {
        return false;
    }
    QDir().mkpath(dstDir);

    const QFileInfoList entries = src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        const QString targetPath = QDir(dstDir).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryRecursive(entry.absoluteFilePath(), targetPath)) {
                return false;
            }
            continue;
        }
        QFile::remove(targetPath);
        if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
            qWarning() << "HttpUploadController: skip unreadable log file" << entry.absoluteFilePath();
        }
    }
    return true;
}

bool copySqliteDatabaseForArchive(const QString &srcPath, const QString &dstPath)
{
    QFile::remove(dstPath);
    if (QFile::copy(srcPath, dstPath)) {
        return true;
    }

    const QString conn = QStringLiteral("log_zip_export_%1")
            .arg(QRandomGenerator::global()->generate());
    bool copied = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(srcPath);
        if (db.open()) {
            QString escaped = dstPath;
            escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
            QSqlQuery q(db);
            copied = q.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped)) && QFile::exists(dstPath);
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    if (copied) {
        return true;
    }
    return QFile::copy(srcPath, dstPath);
}

QString payloadSha256Hex(const QString &payloadRoot)
{
    QFileInfo fi(payloadRoot);
    if (!fi.exists()) {
        return QString();
    }
    if (fi.isFile()) {
        QFile f(payloadRoot);
        if (!f.open(QIODevice::ReadOnly)) {
            return QString();
        }
        QCryptographicHash h(QCryptographicHash::Sha256);
        while (!f.atEnd()) {
            const QByteArray chunk = f.read(1024 * 1024);
            if (chunk.isEmpty() && !f.atEnd()) {
                return QString();
            }
            h.addData(chunk);
        }
        return QString::fromLatin1(h.result().toHex());
    }

    QStringList relFiles;
    QDirIterator it(payloadRoot, QDir::Files, QDirIterator::Subdirectories);
    QDir root(payloadRoot);
    while (it.hasNext()) {
        const QString rel = root.relativeFilePath(it.next());
        if (rel == QStringLiteral("update-manifest.json")) {
            continue;
        }
        relFiles.append(rel);
    }
    std::sort(relFiles.begin(), relFiles.end());

    QCryptographicHash h(QCryptographicHash::Sha256);
    for (const QString &rel : relFiles) {
        h.addData(rel.toUtf8());
        h.addData("\n", 1);
        QFile f(root.filePath(rel));
        if (!f.open(QIODevice::ReadOnly)) {
            return QString();
        }
        while (!f.atEnd()) {
            const QByteArray chunk = f.read(1024 * 1024);
            if (chunk.isEmpty() && !f.atEnd()) {
                return QString();
            }
            h.addData(chunk);
        }
        h.addData("\n", 1);
    }
    return QString::fromLatin1(h.result().toHex());
}

int compareVersionsDesc(const QString &lhs, const QString &rhs)
{
    const QStringList lparts = lhs.split(QLatin1Char('.'));
    const QStringList rparts = rhs.split(QLatin1Char('.'));
    const int n = qMax(lparts.size(), rparts.size());
    for (int i = 0; i < n; ++i) {
        const int lv = (i < lparts.size()) ? lparts.at(i).toInt() : 0;
        const int rv = (i < rparts.size()) ? rparts.at(i).toInt() : 0;
        if (lv > rv) {
            return -1;
        }
        if (lv < rv) {
            return 1;
        }
    }
    return 0;
}

QStringList scanFirmwareHexVersions(const QString &dirPath, const QString &prefix)
{
    QStringList versions;
    QDir dir(dirPath);
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    const QString upperPrefix = prefix.toUpper();
    for (const QFileInfo &fi : files) {
        const QString base = fi.fileName();
        const QRegularExpressionMatch m = kFirmwareFileRe.match(base);
        if (!m.hasMatch()) {
            continue;
        }
        if (m.captured(1).toUpper() != upperPrefix) {
            continue;
        }
        versions.append(m.captured(2));
    }
    versions.removeDuplicates();
    std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
        return compareVersionsDesc(a, b) < 0;
    });
    return versions;
}

QStringList scanSubdirectoryVersions(const QString &dirPath)
{
    QStringList versions;
    QDir dir(dirPath);
    const QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : dirs) {
        const QString name = fi.fileName().trimmed();
        if (kSimpleVersionRe.match(name).hasMatch()) {
            versions.append(name);
        }
    }
    versions.removeDuplicates();
    std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
        return compareVersionsDesc(a, b) < 0;
    });
    return versions;
}

} // namespace

HttpUploadController::HttpUploadController(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    m_uploadDir = QString::fromUtf8(kDefaultUploadDir);
    m_sessionTimer.setParent(this);
    m_sessionTimer.setSingleShot(true);
    m_sessionTimer.setInterval(kSessionTimeoutMs);
    connect(&m_sessionTimer, &QTimer::timeout, this, &HttpUploadController::onSessionTimeout);
    m_apClientPollTimer.setParent(this);
    m_apClientPollTimer.setInterval(2000);
    connect(&m_apClientPollTimer, &QTimer::timeout, this, &HttpUploadController::pollAccessPointClient);
    connect(m_server, &QTcpServer::newConnection, this, &HttpUploadController::onNewConnection);
    refreshReleaseVersions();
}

void HttpUploadController::setLinkStm(LinkStm *linkStm)
{
    m_linkStm = linkStm;
}

void HttpUploadController::setJsonStorage(JsonStorage *storage)
{
    m_json = storage;
    loadNetworkSettings();
    if (m_json) {
        setCurrentMediaVersion(m_json->readString(QStringLiteral("currentMediaVersion"), QStringLiteral("—")));
    } else {
        setCurrentMediaVersion(QStringLiteral("—"));
    }
    refreshReleaseVersions();
}

bool HttpUploadController::hasVersionListChanged(const QStringList &oldList, const QStringList &newList)
{
    return oldList != newList;
}

void HttpUploadController::setCurrentMediaVersion(const QString &version)
{
    const QString v = version.trimmed().isEmpty() ? QStringLiteral("—") : version.trimmed();
    if (m_currentMediaVersion == v) {
        return;
    }
    m_currentMediaVersion = v;
    emit currentMediaVersionChanged();
}

void HttpUploadController::refreshReleaseVersions()
{
    const AppPaths &paths = AppPaths::instance();
    const QStringList newMain = scanSubdirectoryVersions(paths.releasesMainDir());
    const QStringList newMedia = scanSubdirectoryVersions(paths.releasesMediaDir());
    const QStringList newCom = scanFirmwareHexVersions(paths.releasesComDir(), QStringLiteral("COM"));
    const QStringList newArg = scanFirmwareHexVersions(paths.releasesArgDir(), QStringLiteral("ARG"));
    const QStringList newGen = scanFirmwareHexVersions(paths.releasesGenDir(), QStringLiteral("GEN"));

    bool changed = false;
    if (hasVersionListChanged(m_availableMainVersions, newMain)) {
        m_availableMainVersions = newMain;
        changed = true;
    }
    if (hasVersionListChanged(m_availableMediaVersions, newMedia)) {
        m_availableMediaVersions = newMedia;
        changed = true;
    }
    if (hasVersionListChanged(m_availableComVersions, newCom)) {
        m_availableComVersions = newCom;
        changed = true;
    }
    if (hasVersionListChanged(m_availableArgVersions, newArg)) {
        m_availableArgVersions = newArg;
        changed = true;
    }
    if (hasVersionListChanged(m_availableGenVersions, newGen)) {
        m_availableGenVersions = newGen;
        changed = true;
    }

    if (changed) {
        emit releaseVersionsChanged();
    }
}

void HttpUploadController::setActive(bool v)
{
    if (m_active == v) {
        return;
    }
    m_active = v;
    emit activeChanged();
}

void HttpUploadController::setLastError(const QString &e)
{
    if (m_lastError == e) {
        return;
    }
    m_lastError = e;
    emit lastErrorChanged();
}

void HttpUploadController::setAccessPointStatusText(const QString &text)
{
    if (m_apStatusText == text) {
        return;
    }
    m_apStatusText = text;
    emit accessPointStatusTextChanged();
}

void HttpUploadController::setAccessPointClientConnected(bool connected)
{
    if (m_apClientConnected == connected) {
        return;
    }
    m_apClientConnected = connected;
    emit accessPointClientConnectedChanged();
    updateQrCode();
}

QStringList HttpUploadController::localIpv4Addresses() const
{
    QStringList out;
    QStringList preferred;
    const QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || !(iface.flags() & QNetworkInterface::IsRunning)
            || (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        const QString name = iface.name();
        for (const QNetworkAddressEntry &e : iface.addressEntries()) {
            const QHostAddress a = e.ip();
            if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback()) {
                continue;
            }
            const QString s = ipv4ToQString(a);
            if (name.startsWith(QStringLiteral("wlan"), Qt::CaseInsensitive)
                || name.startsWith(QStringLiteral("wl"), Qt::CaseInsensitive)) {
                preferred.append(s);
            } else {
                out.append(s);
            }
        }
    }
    preferred.sort();
    out.sort();
    for (const QString &s : out) {
        if (!preferred.contains(s)) {
            preferred.append(s);
        }
    }
    return preferred;
}

void HttpUploadController::updateBaseUrl()
{
    const QString url = !m_publicBaseUrl.isEmpty()
            ? normalizedBaseUrl(m_publicBaseUrl)
            : QStringLiteral("http://%1:%2/")
                  .arg(m_listenAddress.isNull() ? QStringLiteral("127.0.0.1") : ipv4ToQString(m_listenAddress))
                  .arg(m_port);
    if (m_baseUrl == url) {
        return;
    }
    m_baseUrl = url;
    emit baseUrlChanged();
}

QHostAddress HttpUploadController::preferredListenAddress() const
{
    if (m_apActive && !m_apAddress.isNull()) {
        return m_apAddress;
    }
    if (m_json) {
        const QString configured = m_json->readString(QStringLiteral("httpUploadListenAddress")).trimmed();
        if (!configured.isEmpty()) {
            const QHostAddress configuredAddr(configured);
            if (!configuredAddr.isNull()) {
                return configuredAddr;
            }
        }
    }
    const QStringList ips = localIpv4Addresses();
    if (ips.isEmpty()) {
        return QHostAddress(QHostAddress::LocalHost);
    }
    return QHostAddress(ips.first());
}

QString HttpUploadController::selectWifiInterface() const
{
    if (m_json) {
        const QString configured = m_json->readString(QStringLiteral("httpUploadWifiInterface")).trimmed();
        if (!configured.isEmpty()) {
            return configured;
        }
    }

    QString stdoutText;
    if (runNmcli({QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("DEVICE,TYPE"),
                  QStringLiteral("device"), QStringLiteral("status")},
                 5000, &stdoutText, nullptr)) {
        const QStringList lines = stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList parts = line.split(QLatin1Char(':'));
            if (parts.size() >= 2 && parts.at(1) == QStringLiteral("wifi")) {
                return parts.at(0);
            }
        }
    }

    const QDir netDir(QStringLiteral("/sys/class/net"));
    const QStringList names = netDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : names) {
        if (name.startsWith(QStringLiteral("wlan"), Qt::CaseInsensitive)
            || name.startsWith(QStringLiteral("wl"), Qt::CaseInsensitive)) {
            return name;
        }
    }
    return QString();
}

QHostAddress HttpUploadController::addressForInterface(const QString &ifaceName) const
{
    const QNetworkInterface iface = QNetworkInterface::interfaceFromName(ifaceName);
    if (!iface.isValid()) {
        return QHostAddress();
    }
    for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
        const QHostAddress addr = entry.ip();
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            return addr;
        }
    }
    return QHostAddress();
}

bool HttpUploadController::runCommandWithSudoFallback(const QString &program, const QStringList &args,
                                                      int timeoutMs, QString *stdoutText,
                                                      QString *stderrText) const
{
    auto runOnce = [&](const QString &runProgram, const QStringList &runArgs,
                      QString *out, QString *err) -> bool {
        QProcess proc;
        proc.start(runProgram, runArgs);
        if (!proc.waitForStarted(1000)) {
            if (err) {
                *err = tr("Не удалось запустить %1").arg(runProgram);
            }
            return false;
        }
        if (!proc.waitForFinished(timeoutMs)) {
            proc.kill();
            proc.waitForFinished(1000);
            if (err) {
                *err = tr("Timeout команды %1").arg(runProgram);
            }
            return false;
        }
        if (out) {
            *out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        }
        const QString stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (err) {
            *err = stdErr;
        }
        return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
    };

    QString directOut;
    QString directErr;
    if (runOnce(program, args, &directOut, &directErr)) {
        if (stdoutText) {
            *stdoutText = directOut;
        }
        if (stderrText) {
            *stderrText = directErr;
        }
        return true;
    }

    QString sudoOut;
    QString sudoErr;
    const bool sudoOk = runOnce(QStringLiteral("sudo"),
                                QStringList{QStringLiteral("-n"), program} + args,
                                &sudoOut, &sudoErr);
    if (stdoutText) {
        *stdoutText = sudoOk ? sudoOut : directOut;
    }
    if (stderrText) {
        *stderrText = sudoOk ? sudoErr : (sudoErr.isEmpty() ? directErr : sudoErr);
    }
    return sudoOk;
}

bool HttpUploadController::runNmcli(const QStringList &args, int timeoutMs,
                                    QString *stdoutText, QString *stderrText) const
{
    const QString nmcliPath = QStandardPaths::findExecutable(QStringLiteral("nmcli"));
    if (nmcliPath.isEmpty()) {
        if (stderrText) {
            *stderrText = tr("nmcli не найден.");
        }
        return false;
    }
    return runCommandWithSudoFallback(nmcliPath, args, timeoutMs, stdoutText, stderrText);
}

void HttpUploadController::loadNetworkSettings()
{
    m_publicBaseUrl.clear();
    m_trustProxyHeaders = false;
    if (!m_json) {
        return;
    }
    m_publicBaseUrl = normalizedBaseUrl(m_json->readString(QStringLiteral("httpUploadPublicBaseUrl")));
    m_trustProxyHeaders = parseBoolString(m_json->readString(QStringLiteral("httpUploadTrustProxyHeaders")));
}

bool HttpUploadController::invokeUploadFirewallGuard(const QString &action, QString *errorText) const
{
    const QFileInfo helper(QString::fromLatin1(kUploadFirewallGuardPath));
    if (!helper.exists()) {
        return true;
    }
    if (!helper.isExecutable()) {
        const QString msg = tr("Скрипт управления firewall найден, но не исполняемый: %1")
                                    .arg(QString::fromLatin1(kUploadFirewallGuardPath));
        if (errorText) {
            *errorText = msg;
        }
        return false;
    }

    QProcess proc;
    const QStringList args = {
        QStringLiteral("-n"),
        QString::fromLatin1(kUploadFirewallGuardPath),
        action
    };
    proc.start(QStringLiteral("sudo"), args);
    if (!proc.waitForStarted(1000)) {
        const QString msg = tr("Не удалось запустить helper firewall.");
        if (errorText) {
            *errorText = msg;
        }
        return false;
    }
    if (!proc.waitForFinished(5000)) {
        proc.kill();
        proc.waitForFinished(1000);
        const QString msg = tr("Timeout вызова helper firewall.");
        if (errorText) {
            *errorText = msg;
        }
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const QString details = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const QString msg = details.isEmpty()
                ? tr("Helper firewall завершился с ошибкой.")
                : tr("Helper firewall завершился с ошибкой: %1").arg(details);
        if (errorText) {
            *errorText = msg;
        }
        return false;
    }
    return true;
}

void HttpUploadController::updateLogDownloadUrl()
{
    const QString old = m_logDownloadUrl;
    if (!m_active || m_baseUrl.isEmpty() || m_sessionToken.isEmpty()) {
        m_logDownloadUrl.clear();
    } else {
        m_logDownloadUrl = m_baseUrl + QStringLiteral("download/onyxlog-bundle.zip?token=") + m_sessionToken;
    }
    if (old != m_logDownloadUrl) {
        emit logDownloadUrlChanged();
    }
}

bool HttpUploadController::generateQrCodeImage(const QString &payload, const QString &fileName,
                                               QString *imagePath, QString *errorText) const
{
    const QString qrencodePath = QStandardPaths::findExecutable(QStringLiteral("qrencode"));
    if (qrencodePath.isEmpty()) {
        if (errorText) {
            *errorText = tr("QR недоступен: не установлен qrencode.");
        }
        return false;
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/wifi_upload");
    QDir().mkpath(dir);
    const QString outPath = dir + QLatin1Char('/') + fileName;

    QProcess proc;
    const QStringList args = {
        QStringLiteral("-o"), outPath,
        QStringLiteral("-s"), QStringLiteral("8"),
        QStringLiteral("-m"), QStringLiteral("1"),
        payload
    };
    proc.start(qrencodePath, args);
    if (!proc.waitForStarted(1000) || !proc.waitForFinished(3000)
        || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 || !QFile::exists(outPath)) {
        if (errorText) {
            *errorText = tr("QR недоступен: ошибка запуска qrencode.");
        }
        return false;
    }

    if (imagePath) {
        *imagePath = QStringLiteral("file://") + outPath
                + QStringLiteral("?ts=") + QString::number(QDateTime::currentMSecsSinceEpoch());
    }
    return true;
}

QString HttpUploadController::accessPointQrPayload() const
{
    return QStringLiteral("WIFI:S:%1;T:WPA;P:%2;H:false;;")
            .arg(wifiQrEscape(m_apSsid), wifiQrEscape(m_apPassword));
}

QString HttpUploadController::accessPointIpAddressString() const
{
    const int slash = m_apConfiguredAddress.indexOf(QLatin1Char('/'));
    return slash > 0 ? m_apConfiguredAddress.left(slash) : m_apConfiguredAddress;
}

bool HttpUploadController::buildLogArchiveBundle(const QString &sessionToken, QString *outFilePath,
                                                qint64 *outFileSize, QString *errorHtml) const
{
    if (!outFilePath || !outFileSize || !errorHtml || sessionToken.isEmpty()) {
        return false;
    }
    outFilePath->clear();
    *outFileSize = 0;
    errorHtml->clear();

    qWarning() << "HttpUploadController: buildLogArchiveBundle start";

    const QString onyxLogDir = AppPaths::instance().onyxLogDir();
    const QString userProgPath = AppPaths::instance().userProgDbPath();

    if (!QDir(onyxLogDir).exists()) {
        qWarning() << "HttpUploadController: buildLogArchiveBundle OnyxLog missing:" << onyxLogDir;
        *errorHtml = QStringLiteral("<p>Каталог журналов не найден: %1</p>").arg(onyxLogDir.toHtmlEscaped());
        return false;
    }

    const QString zipProgram = resolveZipProgramPath();
    if (zipProgram.isEmpty()) {
        qWarning() << "HttpUploadController: buildLogArchiveBundle zip not found in PATH";
        *errorHtml = QStringLiteral("<p>Не найдена утилита <code>zip</code> для сборки архива.</p>");
        return false;
    }

    QTemporaryDir bundleDir;
    if (!bundleDir.isValid()) {
        *errorHtml = QStringLiteral("<p>Не удалось создать временный каталог для архива.</p>");
        return false;
    }

    const QString stageRoot = QDir(bundleDir.path()).filePath(QStringLiteral("stage"));
    const QString stageOnyxLog = QDir(stageRoot).filePath(QStringLiteral("OnyxLog"));
    if (!copyDirectoryRecursive(onyxLogDir, stageOnyxLog)) {
        *errorHtml = QStringLiteral("<p>Не удалось подготовить копию каталога OnyxLog.</p>");
        return false;
    }

    QStringList relEntries;
    relEntries << QStringLiteral("OnyxLog");

    if (QFile::exists(userProgPath)) {
        const QString stageFotek = QDir(stageRoot).filePath(QStringLiteral("FOTEK"));
        QDir().mkpath(stageFotek);
        const QString stageUserProg = QDir(stageFotek).filePath(QStringLiteral("userProg.db"));
        if (copySqliteDatabaseForArchive(userProgPath, stageUserProg)) {
            relEntries << QStringLiteral("FOTEK");
        } else {
            qWarning() << "HttpUploadController: userProg.db not included in log archive";
        }
    }

    const QString zipPath = QDir(bundleDir.path()).filePath(QStringLiteral("onyxlog-bundle.zip"));
    QFile::remove(zipPath);

    QProcess zipProc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString oldPath = env.value(QStringLiteral("PATH"));
    env.insert(QStringLiteral("PATH"),
               oldPath.isEmpty()
               ? QStringLiteral("/usr/local/bin:/usr/bin:/bin")
               : oldPath + QStringLiteral(":/usr/local/bin:/usr/bin:/bin"));
    zipProc.setProcessEnvironment(env);
    zipProc.setWorkingDirectory(stageRoot);

    QStringList args;
    args << QStringLiteral("-r") << QStringLiteral("-q")
         << QStringLiteral("-P") << QString::fromUtf8(kReleaseZipPassword)
         << zipPath
         << relEntries;

    zipProc.start(zipProgram, args);
    if (!zipProc.waitForStarted(3000)) {
        *errorHtml = QStringLiteral("<p>Не удалось запустить <code>zip</code>.</p>");
        return false;
    }
    if (!zipProc.waitForFinished(300000)) {
        zipProc.kill();
        zipProc.waitForFinished(1000);
        *errorHtml = QStringLiteral("<p>Превышено время ожидания при создании архива.</p>");
        return false;
    }

    const QString zipStdout = QString::fromUtf8(zipProc.readAllStandardOutput()).trimmed();
    const QString zipStderr = QString::fromUtf8(zipProc.readAllStandardError()).trimmed();
    const int zipExit = zipProc.exitCode();

    if (zipProc.exitStatus() != QProcess::NormalExit || (zipExit != 0 && zipExit != 12)) {
        qWarning() << "HttpUploadController: buildLogArchiveBundle zip failed, exit=" << zipExit
                   << "stderr=" << zipStderr << "stdout=" << zipStdout;
        QString details = zipStderr;
        if (!zipStdout.isEmpty()) {
            if (!details.isEmpty()) {
                details += QStringLiteral("\n");
            }
            details += zipStdout;
        }
        *errorHtml = details.isEmpty()
                ? QStringLiteral("<p>Ошибка при создании архива (код %1).</p>").arg(zipExit)
                : QStringLiteral("<p>Ошибка при создании архива (код %1): %2</p>")
                      .arg(zipExit)
                      .arg(details.toHtmlEscaped());
        return false;
    }

    const QString cachePath = logArchiveCacheFilePath(sessionToken);
    if (QFile::exists(cachePath) && !QFile::remove(cachePath)) {
        *errorHtml = QStringLiteral("<p>Не удалось подготовить файл архива.</p>");
        return false;
    }
    if (!QFile::copy(zipPath, cachePath)) {
        *errorHtml = QStringLiteral("<p>Не удалось сохранить архив.</p>");
        return false;
    }
    const qint64 size = QFileInfo(cachePath).size();
    if (size <= 0) {
        qWarning() << "HttpUploadController: buildLogArchiveBundle result empty";
        QFile::remove(cachePath);
        *errorHtml = QStringLiteral("<p>Архив пуст.</p>");
        return false;
    }
    *outFilePath = cachePath;
    *outFileSize = size;
    qWarning() << "HttpUploadController: buildLogArchiveBundle ok, path=" << cachePath << "size=" << size;
    return true;
}

void HttpUploadController::resetLogArchiveCache()
{
    QString cacheFile;
    {
        QMutexLocker locker(&m_logArchiveMutex);
        cacheFile = m_logArchiveFilePath;
        m_logArchiveState = LogArchiveState::Idle;
        m_logArchivePayload.clear();
        m_logArchiveFilePath.clear();
        m_logArchiveFileSize = 0;
        m_logArchiveErrorText.clear();
    }
    if (!cacheFile.isEmpty()) {
        QFile::remove(cacheFile);
    }
}

void HttpUploadController::startLogArchiveBuildIfNeeded(bool forceRestart)
{
    {
        QMutexLocker locker(&m_logArchiveMutex);
        if (m_logArchiveState == LogArchiveState::Building) {
            qWarning() << "HttpUploadController: log archive build already running";
            return;
        }
        if (!forceRestart && m_logArchiveState == LogArchiveState::Ready) {
            const qint64 cachedBytes = m_logArchiveFileSize > 0
                    ? m_logArchiveFileSize
                    : m_logArchivePayload.size();
            qWarning() << "HttpUploadController: log archive cache ready,"
                       << cachedBytes << "bytes, skip rebuild";
            return;
        }
        qWarning() << "HttpUploadController: log archive build started, forceRestart="
                   << forceRestart << "prevState=" << logArchiveCacheDebugTextUnlocked();
        m_logArchiveState = LogArchiveState::Building;
        m_logArchivePayload.clear();
        m_logArchiveErrorText.clear();
    }

    struct LogArchiveBuildResult {
        QString filePath;
        qint64 fileSize = 0;
        QString errorHtml;
        bool ok = false;
    };

    const QString buildToken = m_sessionToken;

    auto *watcher = new QFutureWatcher<LogArchiveBuildResult>(this);
    connect(watcher, &QFutureWatcher<LogArchiveBuildResult>::finished, this,
            [this, watcher]() {
        const LogArchiveBuildResult result = watcher->result();
        watcher->deleteLater();

        QMutexLocker locker(&m_logArchiveMutex);
        if (result.ok && result.fileSize > 0 && !result.filePath.isEmpty()) {
            m_logArchivePayload.clear();
            m_logArchiveFilePath = result.filePath;
            m_logArchiveFileSize = result.fileSize;
            m_logArchiveState = LogArchiveState::Ready;
            m_logArchiveErrorText.clear();
            qWarning() << "HttpUploadController: log archive build finished ok, size="
                       << m_logArchiveFileSize << "path=" << m_logArchiveFilePath;
        } else {
            m_logArchivePayload.clear();
            m_logArchiveFilePath.clear();
            m_logArchiveFileSize = 0;
            m_logArchiveState = LogArchiveState::Error;
            m_logArchiveErrorText = result.errorHtml.isEmpty()
                    ? QStringLiteral("Не удалось создать архив.")
                    : result.errorHtml;
            const QString plain = QString(m_logArchiveErrorText)
                    .remove(QRegularExpression(QStringLiteral("<[^>]*>")));
            qWarning() << "HttpUploadController: log archive build failed:" << plain;
        }
    });

    watcher->setFuture(QtConcurrent::run([this, buildToken]() {
        LogArchiveBuildResult result;
        result.ok = buildLogArchiveBundle(buildToken, &result.filePath, &result.fileSize, &result.errorHtml);
        return result;
    }));
}

QString HttpUploadController::logArchiveCacheDebugTextUnlocked() const
{
    switch (m_logArchiveState) {
    case LogArchiveState::Building:
        return QStringLiteral("building");
    case LogArchiveState::Ready: {
        const qint64 bytes = m_logArchiveFileSize > 0 ? m_logArchiveFileSize : m_logArchivePayload.size();
        return QStringLiteral("ready, %1 bytes").arg(bytes);
    }
    case LogArchiveState::Error:
        return QStringLiteral("error");
    case LogArchiveState::Idle:
    default:
        return QStringLiteral("idle");
    }
}

QString HttpUploadController::logArchiveCacheDebugText() const
{
    QMutexLocker locker(&m_logArchiveMutex);
    return logArchiveCacheDebugTextUnlocked();
}

QByteArray HttpUploadController::logArchiveStatusJson() const
{
    QMutexLocker locker(&m_logArchiveMutex);
    const QString debug = logArchiveCacheDebugTextUnlocked();
    switch (m_logArchiveState) {
    case LogArchiveState::Building:
        return QJsonDocument(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("building")},
            {QStringLiteral("debug"), debug}
        }).toJson(QJsonDocument::Compact);
    case LogArchiveState::Ready: {
        const qint64 bytes = m_logArchiveFileSize > 0 ? m_logArchiveFileSize : m_logArchivePayload.size();
        return QJsonDocument(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("ready")},
            {QStringLiteral("size"), bytes},
            {QStringLiteral("debug"), debug}
        }).toJson(QJsonDocument::Compact);
    }
    case LogArchiveState::Error: {
        const QString plain = QString(m_logArchiveErrorText)
                .remove(QRegularExpression(QStringLiteral("<[^>]*>")));
        return QJsonDocument(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("error")},
            {QStringLiteral("message"), plain},
            {QStringLiteral("debug"), debug}
        }).toJson(QJsonDocument::Compact);
    }
    case LogArchiveState::Idle:
    default:
        return QJsonDocument(QJsonObject{
            {QStringLiteral("state"), QStringLiteral("building")},
            {QStringLiteral("debug"), debug}
        }).toJson(QJsonDocument::Compact);
    }
}

bool HttpUploadController::servePreparedLogArchive(QTcpSocket *socket)
{
    QString filePath;
    QByteArray body;
    QString cacheState;
    {
        QMutexLocker locker(&m_logArchiveMutex);
        cacheState = logArchiveCacheDebugTextUnlocked();
        const bool hasFile = !m_logArchiveFilePath.isEmpty()
                && QFile::exists(m_logArchiveFilePath) && m_logArchiveFileSize > 0;
        const bool hasPayload = !m_logArchivePayload.isEmpty();
        if (m_logArchiveState != LogArchiveState::Ready || (!hasFile && !hasPayload)) {
            qWarning() << "HttpUploadController: servePreparedLogArchive not ready, cache="
                       << cacheState;
            return false;
        }
        if (hasFile) {
            filePath = m_logArchiveFilePath;
        } else {
            body = m_logArchivePayload;
        }
    }

    if (!filePath.isEmpty()) {
        qWarning() << "HttpUploadController: servePreparedLogArchive streaming file" << filePath
                   << "peer=" << (socket ? socket->peerAddress().toString() : QString())
                   << "cache=" << cacheState;
        return sendFileDownloadFromPath(socket, filePath);
    }

    qWarning() << "HttpUploadController: servePreparedLogArchive sending" << body.size()
               << "bytes from memory, peer=" << (socket ? socket->peerAddress().toString() : QString())
               << "cache=" << cacheState;
    sendFileDownloadResponse(socket, QByteArrayLiteral("onyxlog.zip"), body);
    return true;
}

bool HttpUploadController::isValidTokenInPath(const QString &path) const
{
    return requestQueryValue(path, QStringLiteral("token")) == m_sessionToken;
}

void HttpUploadController::bindAuthorizedClient(const QHostAddress &peer)
{
    const QHostAddress normalized = normalizeClientAddress(peer);
    if (normalized.isNull()) {
        return;
    }
    if (!m_authorizedClientAddress.isNull()) {
        return;
    }
    m_authorizedClientAddress = normalized;
    qWarning() << "HttpUploadController: session bound to client" << normalized.toString();
}

bool HttpUploadController::isAuthorizedClient(const QHostAddress &peer) const
{
    if (peer.isNull()) {
        return false;
    }
    if (m_authorizedClientAddress.isNull()) {
        return true;
    }
    return normalizeClientAddress(peer) == m_authorizedClientAddress;
}

void HttpUploadController::sendDownloadForbidden(QTcpSocket *socket, const QString &reason,
                                                 const QString &message)
{
    sendJsonResponse(socket, 403, QJsonDocument(QJsonObject{
        {QStringLiteral("state"), QStringLiteral("error")},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("message"), message}
    }).toJson(QJsonDocument::Compact));
}

QHostAddress HttpUploadController::effectiveClientAddress() const
{
    if (!m_client) {
        return QHostAddress();
    }
    const QHostAddress peer = m_client->peerAddress();
    if (!m_trustProxyHeaders || !peer.isLoopback()) {
        return peer;
    }

    const QString forwarded = m_requestHeaders.value(QStringLiteral("x-forwarded-for")).trimmed();
    if (forwarded.isEmpty()) {
        return peer;
    }

    const QString first = forwarded.split(QLatin1Char(','), Qt::SkipEmptyParts).value(0).trimmed();
    if (first.isEmpty()) {
        return peer;
    }

    const QHostAddress forwardedAddr(first);
    return forwardedAddr.isNull() ? peer : forwardedAddr;
}

void HttpUploadController::updateQrCode()
{
    const QString oldPath = m_qrImagePath;
    const QString oldStatus = m_qrStatusText;
    m_qrImagePath.clear();
    m_qrStatusText.clear();

    if (!m_active) {
        if (oldPath != m_qrImagePath) {
            emit qrImagePathChanged();
        }
        if (oldStatus != m_qrStatusText) {
            emit qrStatusTextChanged();
        }
        return;
    }

    QString payload;
    QString fileName;
    if (m_apActive && !m_apClientConnected) {
        payload = accessPointQrPayload();
        fileName = QStringLiteral("wifi_ap_qr.png");
        m_qrStatusText = tr("Сначала подключите телефон или ноутбук к Wi-Fi %1. Пароль: %2")
                .arg(m_apSsid, m_apPassword);
    } else {
        if (m_baseUrl.isEmpty()) {
            m_qrStatusText = tr("Адрес загрузки ещё не готов.");
        } else {
            payload = m_baseUrl;
            fileName = QStringLiteral("upload_qr.png");
            m_qrStatusText = m_apActive && !m_apClientConnected
                    ? tr("Если телефон уже подключён к ONYX-TEST, откройте этот QR для страницы загрузки.")
                    : tr("Устройство подключено. Отсканируйте QR для открытия страницы загрузки.");
        }
    }

    if (!payload.isEmpty()) {
        QString qrPath;
        QString qrError;
        if (generateQrCodeImage(payload, fileName, &qrPath, &qrError)) {
            m_qrImagePath = qrPath;
        } else {
            m_qrStatusText = qrError;
        }
    }

    if (oldPath != m_qrImagePath) {
        emit qrImagePathChanged();
    }
    if (oldStatus != m_qrStatusText) {
        emit qrStatusTextChanged();
    }
}

QString HttpUploadController::uploadSavePath() const
{
    return effectiveUploadDir();
}

QString HttpUploadController::effectiveUploadDir() const
{
    QString dir = m_uploadDir;
    if (dir.isEmpty()) {
        dir = QString::fromUtf8(kDefaultUploadDir);
    }
    QDir d;
    if (d.mkpath(dir)) {
        const QFileInfo fi(dir);
        if (fi.isDir() && fi.isWritable()) {
            return dir;
        }
    }
    const QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/wifi_upload");
    d.mkpath(fallback);
    return fallback;
}

static QString randomToken()
{
    const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    QString s;
    s.reserve(10);
    for (int i = 0; i < 10; ++i) {
        s.append(QLatin1Char(alphabet[QRandomGenerator::global()->bounded(int(sizeof(alphabet) - 2))]));
    }
    return s;
}

QString HttpUploadController::makeAccessPointPassword() const
{
    const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    QString s;
    s.reserve(12);
    for (int i = 0; i < 12; ++i) {
        s.append(QLatin1Char(alphabet[QRandomGenerator::global()->bounded(int(sizeof(alphabet) - 2))]));
    }
    return s;
}

bool HttpUploadController::startAccessPoint(QString *errorText)
{
    m_apInterfaceName = selectWifiInterface();
    if (m_apInterfaceName.isEmpty()) {
        if (errorText) {
            *errorText = tr("Wi-Fi интерфейс не найден.");
        }
        return false;
    }

    if (m_apPassword.isEmpty()) {
        m_apPassword = QStringLiteral("Electrosurgical");
    }
    setAccessPointClientConnected(false);
    setAccessPointStatusText(tr("Запуск точки доступа %1...").arg(m_apSsid));
    emit accessPointChanged();

    QString stderrText;
    runNmcli({QStringLiteral("connection"), QStringLiteral("down"), m_apConnectionName},
             5000, nullptr, nullptr);
    runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), m_apConnectionName},
             5000, nullptr, nullptr);

    const QStringList addArgs = {
        QStringLiteral("connection"), QStringLiteral("add"),
        QStringLiteral("type"), QStringLiteral("wifi"),
        QStringLiteral("ifname"), m_apInterfaceName,
        QStringLiteral("con-name"), m_apConnectionName,
        QStringLiteral("autoconnect"), QStringLiteral("no"),
        QStringLiteral("ssid"), m_apSsid
    };
    if (!runNmcli(addArgs, 10000, nullptr, &stderrText)) {
        if (errorText) {
            *errorText = stderrText.isEmpty()
                    ? tr("Не удалось создать профиль точки доступа.")
                    : tr("Не удалось создать профиль точки доступа: %1").arg(stderrText);
        }
        setAccessPointStatusText(QString());
        m_apPassword.clear();
        emit accessPointChanged();
        return false;
    }

    const QStringList modifyArgs = {
        QStringLiteral("connection"), QStringLiteral("modify"), m_apConnectionName,
        QStringLiteral("802-11-wireless.mode"), QStringLiteral("ap"),
        QStringLiteral("802-11-wireless.band"), QStringLiteral("bg"),
        QStringLiteral("ipv4.method"), QStringLiteral("shared"),
        QStringLiteral("ipv4.addresses"), m_apConfiguredAddress,
        QStringLiteral("ipv6.method"), QStringLiteral("ignore"),
        QStringLiteral("wifi-sec.key-mgmt"), QStringLiteral("wpa-psk"),
        QStringLiteral("wifi-sec.psk"), m_apPassword
    };
    if (!runNmcli(modifyArgs, 10000, nullptr, &stderrText)) {
        runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), m_apConnectionName},
                 5000, nullptr, nullptr);
        if (errorText) {
            *errorText = stderrText.isEmpty()
                    ? tr("Не удалось настроить точку доступа.")
                    : tr("Не удалось настроить точку доступа: %1").arg(stderrText);
        }
        setAccessPointStatusText(QString());
        m_apPassword.clear();
        emit accessPointChanged();
        return false;
    }

    if (!runNmcli({QStringLiteral("connection"), QStringLiteral("up"), m_apConnectionName},
                  20000, nullptr, &stderrText)) {
        runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), m_apConnectionName},
                 5000, nullptr, nullptr);
        if (errorText) {
            *errorText = stderrText.isEmpty()
                    ? tr("Не удалось запустить точку доступа.")
                    : tr("Не удалось запустить точку доступа: %1").arg(stderrText);
        }
        setAccessPointStatusText(QString());
        m_apPassword.clear();
        emit accessPointChanged();
        return false;
    }

    m_apActive = true;
    emit accessPointChanged();

    for (int i = 0; i < 10; ++i) {
        m_apAddress = addressForInterface(m_apInterfaceName);
        if (!m_apAddress.isNull()) {
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        QThread::msleep(300);
    }

    if (m_apAddress.isNull()) {
        m_apAddress = QHostAddress(accessPointIpAddressString());
    }

    setAccessPointStatusText(tr("Точка доступа %1 активна. Ожидание подключения клиента.")
                             .arg(m_apSsid));
    m_apClientPollTimer.start();
    return true;
}

void HttpUploadController::stopAccessPoint()
{
    m_apClientPollTimer.stop();

    if (!m_apConnectionName.isEmpty()) {
        QString stderrText;
        runNmcli({QStringLiteral("connection"), QStringLiteral("down"), m_apConnectionName},
                 10000, nullptr, &stderrText);
        runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), m_apConnectionName},
                 10000, nullptr, &stderrText);
    }

    const bool changed = m_apActive || !m_apInterfaceName.isEmpty()
            || !m_apAddress.isNull();
    m_apActive = false;
    m_apInterfaceName.clear();
    m_apAddress = QHostAddress();
    setAccessPointClientConnected(false);
    setAccessPointStatusText(QString());
    if (changed) {
        emit accessPointChanged();
    }
}

bool HttpUploadController::hasConnectedAccessPointClient() const
{
    if (m_apInterfaceName.isEmpty()) {
        return false;
    }

    QString iwOut;
    QString iwErr;
    const QString iwPath = QStandardPaths::findExecutable(QStringLiteral("iw"));
    if (!iwPath.isEmpty()
        && runCommandWithSudoFallback(iwPath,
                                      {QStringLiteral("dev"), m_apInterfaceName,
                                       QStringLiteral("station"), QStringLiteral("dump")},
                                      5000, &iwOut, &iwErr)) {
        if (iwOut.contains(QStringLiteral("Station "))) {
            return true;
        }
    }

    QString neighOut;
    QString neighErr;
    const QString ipPath = QStandardPaths::findExecutable(QStringLiteral("ip"));
    if (!ipPath.isEmpty()
        && runCommandWithSudoFallback(ipPath,
                                      {QStringLiteral("neigh"), QStringLiteral("show"),
                                       QStringLiteral("dev"), m_apInterfaceName},
                                      5000, &neighOut, &neighErr)) {
        const QStringList lines = neighOut.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.contains(QStringLiteral("FAILED")) || line.contains(QStringLiteral("INCOMPLETE"))) {
                continue;
            }
            if (!m_apAddress.isNull() && line.startsWith(ipv4ToQString(m_apAddress) + QLatin1Char(' '))) {
                continue;
            }
            return true;
        }
    }

    return false;
}

void HttpUploadController::pollAccessPointClient()
{
    if (!m_apActive) {
        return;
    }
    const bool connected = hasConnectedAccessPointClient();
    setAccessPointClientConnected(connected);
    setAccessPointStatusText(connected
                             ? tr("К точке доступа подключено устройство.")
                             : tr("Точка доступа %1 активна. Ожидание подключения клиента.")
                                   .arg(m_apSsid));
}

void HttpUploadController::startSession()
{
    setLastError(QString());
    if (m_active) {
        return;
    }

    loadNetworkSettings();
    if (m_json) {
        const QString p = m_json->readString(QStringLiteral("httpUploadPort"));
        if (!p.isEmpty()) {
            bool ok = false;
            const int parsed = p.toInt(&ok);
            if (ok && parsed > 0 && parsed < 65536 && m_port != parsed) {
                m_port = parsed;
                emit listenPortChanged();
            }
        }
        const QString d = m_json->readString(QStringLiteral("httpUploadDir"));
        if (!d.isEmpty()) {
            m_uploadDir = d;
        }
        const QString apAddress = m_json->readString(QStringLiteral("httpUploadApAddress")).trimmed();
        if (!apAddress.isEmpty()) {
            m_apConfiguredAddress = apAddress.contains(QLatin1Char('/'))
                    ? apAddress
                    : apAddress + QStringLiteral("/24");
        }
    }

    QString apError;
    if (!startAccessPoint(&apError)) {
        setLastError(apError);
        return;
    }

    m_sessionToken = randomToken();
    emit sessionTokenChanged();
    resetLogArchiveCache();
    m_authorizedClientAddress = QHostAddress();

    QDir().mkpath(effectiveUploadDir());
    emit uploadSavePathChanged();

    m_listenAddress = preferredListenAddress();
    if (!m_server->listen(m_listenAddress, static_cast<quint16>(m_port))) {
        setLastError(tr("Не удалось занять порт %1: %2")
                             .arg(m_port)
                             .arg(m_server->errorString()));
        m_listenAddress = QHostAddress();
        m_sessionToken.clear();
        emit sessionTokenChanged();
        stopAccessPoint();
        return;
    }

    QString fwError;
    if (!invokeUploadFirewallGuard(QStringLiteral("open"), &fwError)) {
        if (m_server->isListening()) {
            m_server->close();
        }
        m_listenAddress = QHostAddress();
        m_sessionToken.clear();
        emit sessionTokenChanged();
        stopAccessPoint();
        setLastError(fwError);
        return;
    }

    updateBaseUrl();
    setActive(true);
    updateLogDownloadUrl();
    updateQrCode();
    m_sessionTimer.start();
    qWarning() << "HttpUploadController: startSession, prebuilding log archive";
    startLogArchiveBuildIfNeeded(true);
}

void HttpUploadController::stopSession()
{
    resetLogArchiveCache();
    QString fwError;
    if (!invokeUploadFirewallGuard(QStringLiteral("close"), &fwError)) {
        qWarning() << "HttpUploadController:" << fwError;
    }

    m_sessionTimer.stop();
    if (m_client) {
        m_client->disconnect(this);
        m_client->abort();
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_rxBuffer.clear();
    m_headerComplete = false;
    m_contentLength = -1;
    m_requestHeaders.clear();
    m_uploadInProgress = false;
    m_uploadProgress = 0.0;
    m_uploadStatusText.clear();
    emit uploadProgressChanged();
    if (m_server->isListening()) {
        m_server->close();
    }
    m_listenAddress = QHostAddress();
    m_authorizedClientAddress = QHostAddress();
    m_sessionToken.clear();
    emit sessionTokenChanged();
    m_rxBuffer.clear();
    m_headerComplete = false;
    m_contentLength = -1;
    m_requestHeaders.clear();
    setActive(false);
    m_baseUrl.clear();
    emit baseUrlChanged();
    stopAccessPoint();
    updateLogDownloadUrl();
    updateQrCode();
}

void HttpUploadController::onSessionTimeout()
{
    stopSession();
}

void HttpUploadController::onNewConnection()
{
    if (m_client && m_client->state() != QAbstractSocket::ConnectedState) {
        m_client->deleteLater();
        m_client = nullptr;
        m_rxBuffer.clear();
        m_headerComplete = false;
        m_contentLength = -1;
        m_requestHeaders.clear();
    }

    if (m_client) {
        // Во время приёма большого файла браузер может открыть дополнительное соединение
        // (например, favicon/параллельный запрос). Не отвечаем 503, оставляем его в pending,
        // обработаем после освобождения активного m_client.
        return;
    }

    m_client = m_server->nextPendingConnection();
    if (!m_client) {
        return;
    }
    m_rxBuffer.clear();
    m_headerComplete = false;
    m_contentLength = -1;
    m_requestHeaders.clear();
    connect(m_client, &QTcpSocket::readyRead, this, &HttpUploadController::onClientReadyRead);
    connect(m_client, &QTcpSocket::disconnected, this, &HttpUploadController::onClientDisconnected);

    // Браузер может открыть "пустой" keep-alive сокет и не отправлять запрос.
    // Чтобы не блокировать очередь pending-коннектов, освобождаем такой сокет по таймауту.
    QPointer<QTcpSocket> acceptedSock(m_client);
    QTimer::singleShot(2500, this, [this, acceptedSock]() {
        if (!acceptedSock) {
            return;
        }
        if (m_client == acceptedSock && m_rxBuffer.isEmpty() && !m_headerComplete) {
            releaseClientSocket();
        }
    });
}

void HttpUploadController::onClientDisconnected()
{
    QTcpSocket *const socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || socket != m_client) {
        if (socket) {
            socket->deleteLater();
        }
        return;
    }

    if (m_client) {
        QTcpSocket *const oldClient = m_client;
        m_client = nullptr;
        oldClient->deleteLater();
    }
    m_rxBuffer.clear();
    m_headerComplete = false;
    m_contentLength = -1;
    m_requestHeaders.clear();

    if (m_server && m_server->hasPendingConnections()) {
        QMetaObject::invokeMethod(this, "onNewConnection", Qt::QueuedConnection);
    }
}

void HttpUploadController::drainPendingConnections()
{
    while (!m_client && m_server && m_server->hasPendingConnections()) {
        onNewConnection();
        if (m_client && m_rxBuffer.isEmpty() && !m_headerComplete) {
            break;
        }
    }
}

void HttpUploadController::releaseClientSocket()
{
    if (m_client) {
        QTcpSocket *sock = m_client;
        m_client = nullptr;
        sock->disconnect(this);
        sock->disconnectFromHost();
        sock->deleteLater();
    }
    m_rxBuffer.clear();
    m_headerComplete = false;
    m_contentLength = -1;
    m_requestHeaders.clear();

    if (m_server && m_server->hasPendingConnections()) {
        QMetaObject::invokeMethod(this, &HttpUploadController::drainPendingConnections, Qt::QueuedConnection);
    }
}

void HttpUploadController::sendHttpResponse(QTcpSocket *socket, int statusCode, const QByteArray &contentType,
                                          const QByteArray &body)
{
    QByteArray line;
    switch (statusCode) {
    case 200:
        line = "HTTP/1.1 200 OK\r\n";
        break;
    case 204:
        line = "HTTP/1.1 204 No Content\r\n";
        break;
    case 400:
        line = "HTTP/1.1 400 Bad Request\r\n";
        break;
    case 403:
        line = "HTTP/1.1 403 Forbidden\r\n";
        break;
    case 404:
        line = "HTTP/1.1 404 Not Found\r\n";
        break;
    case 413:
        line = "HTTP/1.1 413 Payload Too Large\r\n";
        break;
    default:
        line = QByteArray("HTTP/1.1 ") + QByteArray::number(statusCode) + " Error\r\n";
        break;
    }
    QByteArray hdr = line;
    hdr += "Connection: close\r\n";
    hdr += "Cache-Control: no-store, no-cache, must-revalidate\r\n";
    if (!contentType.isEmpty()) {
        hdr += "Content-Type: ";
        hdr += contentType;
        hdr += "\r\n";
    }
    hdr += "Content-Length: ";
    hdr += QByteArray::number(body.size());
    hdr += "\r\n\r\n";
    socket->write(hdr);
    if (!body.isEmpty()) {
        socket->write(body);
    }
    socket->flush();
}

bool HttpUploadController::sendFileDownloadFromPath(QTcpSocket *socket, const QString &filePath)
{
    if (!socket || filePath.isEmpty()) {
        qWarning() << "HttpUploadController: sendFileDownloadFromPath invalid args";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "HttpUploadController: sendFileDownloadFromPath open failed:" << filePath
                   << file.errorString();
        return false;
    }

    const qint64 fileSize = file.size();
    QByteArray hdr = "HTTP/1.1 200 OK\r\n";
    hdr += "Connection: close\r\n";
    hdr += "Content-Type: application/zip\r\n";
    hdr += "Content-Disposition: attachment; filename=\"onyxlog.zip\"\r\n";
    hdr += "Cache-Control: no-store\r\n";
    hdr += "Content-Length: ";
    hdr += QByteArray::number(fileSize);
    hdr += "\r\n\r\n";

    const bool hdrOk = writeSocketAll(socket, hdr);
    bool bodyOk = hdrOk;
    if (bodyOk) {
        constexpr int kChunkSize = 64 * 1024;
        while (!file.atEnd()) {
            const QByteArray chunk = file.read(kChunkSize);
            if (chunk.isEmpty() && !file.atEnd()) {
                bodyOk = false;
                break;
            }
            if (!chunk.isEmpty() && !writeSocketAll(socket, chunk)) {
                bodyOk = false;
                break;
            }
        }
    }
    socket->flush();
    qWarning() << "HttpUploadController: sendFileDownloadFromPath hdrOk=" << hdrOk
               << "bodyOk=" << bodyOk << "bytes=" << fileSize
               << "path=" << filePath << "socketState=" << socket->state();
    return hdrOk && bodyOk;
}

void HttpUploadController::sendFileDownloadResponse(QTcpSocket *socket, const QByteArray &downloadFileName,
                                                    const QByteArray &body)
{
    Q_UNUSED(downloadFileName)
    if (!socket) {
        qWarning() << "HttpUploadController: sendFileDownloadResponse null socket";
        return;
    }
    QByteArray hdr = "HTTP/1.1 200 OK\r\n";
    hdr += "Connection: close\r\n";
    hdr += "Content-Type: application/zip\r\n";
    hdr += "Content-Disposition: attachment; filename=\"onyxlog.zip\"\r\n";
    hdr += "Cache-Control: no-store\r\n";
    hdr += "Content-Length: ";
    hdr += QByteArray::number(body.size());
    hdr += "\r\n\r\n";
    const bool hdrOk = writeSocketAll(socket, hdr);
    const bool bodyOk = writeSocketAll(socket, body);
    socket->flush();
    qWarning() << "HttpUploadController: sendFileDownloadResponse hdrOk=" << hdrOk
               << "bodyOk=" << bodyOk << "bytes=" << body.size()
               << "socketState=" << socket->state();
}

void HttpUploadController::sendJsonResponse(QTcpSocket *socket, int statusCode, const QByteArray &jsonBody)
{
    QByteArray line;
    switch (statusCode) {
    case 200:
        line = "HTTP/1.1 200 OK\r\n";
        break;
    case 403:
        line = "HTTP/1.1 403 Forbidden\r\n";
        break;
    case 503:
        line = "HTTP/1.1 503 Service Unavailable\r\n";
        break;
    default:
        line = "HTTP/1.1 500 Internal Server Error\r\n";
        break;
    }
    QByteArray hdr = line;
    hdr += "Connection: close\r\n";
    hdr += "Content-Type: application/json; charset=utf-8\r\n";
    hdr += "Cache-Control: no-store, no-cache, must-revalidate\r\n";
    hdr += "Content-Length: ";
    hdr += QByteArray::number(jsonBody.size());
    hdr += "\r\n\r\n";
    writeSocketAll(socket, hdr);
    writeSocketAll(socket, jsonBody);
    socket->flush();
}

void HttpUploadController::sendSimpleHtml(QTcpSocket *socket, int statusCode, const QString &title,
                                        const QString &bodyHtml)
{
    const QString page = QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>%1</title></head>"
                                        "<body>%2</body></html>")
                                 .arg(title, bodyHtml);
    sendHttpResponse(socket, statusCode, "text/html; charset=utf-8", page.toUtf8());
}

QByteArray HttpUploadController::buildUploadPageHtml() const
{
    const QString token = m_sessionToken.toHtmlEscaped();
    const QString tokenJs = QString(m_sessionToken)
            .replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
            .replace(QLatin1Char('\''), QStringLiteral("\\'"));
    QString serialHtml = QStringLiteral("—");
    QString typeHtml = QStringLiteral("—");
    if (m_json) {
        const QString s = m_json->readString(QStringLiteral("serialNumber"), QString()).trimmed();
        if (!s.isEmpty()) {
            serialHtml = s.toHtmlEscaped();
        }
        const QString t = m_json->readString(QStringLiteral("deviceType"), QString()).trimmed();
        if (!t.isEmpty()) {
            typeHtml = t.toHtmlEscaped();
        }
    }
    const QString html = QStringLiteral(
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1\"><title>Загрузка файлов</title></head><body>"
            "<h1>Загрузка файлов</h1>"
            "<p><strong>Серийный номер:</strong> %2<br><strong>Тип аппарата:</strong> %3</p>"
            "<p><strong>Важно:</strong> эта ссылка действует только для текущей сессии и только для устройства, которое первым открыло страницу. После перезапуска приёма или таймаута откройте страницу заново.</p>"
            "<p>Выберите файл обновления в формате <b>имя-a.b-c.d-e.zip</b> (например: onyx-5.6-3.4-1.zip)</p>"
            "<form id=\"uploadForm\" method=\"post\" action=\"/upload\" enctype=\"multipart/form-data\">"
            "<input type=\"hidden\" name=\"token\" value=\"%1\">"
            "<p style=\"display:flex;flex-direction:column;align-items:flex-start;gap:12px;\">"
            "<input id=\"fileInput\" type=\"file\" name=\"file\" multiple style=\"display:none;\">"
            "<button id=\"pickFileBtn\" type=\"button\" style=\"font-size:18px;padding:12px 20px;\">Выбрать файл</button>"
            "<span id=\"selectedFiles\" style=\"color:#555;font-size:16px;\">Файл не выбран</span>"
            "</p>"
            "<p><button id=\"submitBtn\" type=\"submit\" style=\"font-size:18px;padding:12px 20px;\">Отправить</button></p>"
            "</form>"
            "<div id=\"progressWrap\" style=\"display:none;max-width:520px;\">"
            "<div style=\"height:10px;background:#ddd;border-radius:5px;overflow:hidden;\">"
            "<div id=\"progressBar\" style=\"height:10px;width:0%%;background:#1976d2;\"></div>"
            "</div>"
            "<p id=\"progressText\" style=\"margin:6px 0 0 0;color:#333;\">0%%</p>"
            "</div>"
            "<div id=\"result\"></div>"
            "<hr><h2>Передача файла с устройства</h2>"
            "<p><button id=\"logDownloadBtn\" type=\"button\" style=\"font-size:18px;padding:12px 20px;\">"
            "Скачать архив журналов (ZIP)</button></p>"
            "<p id=\"logDownloadStatus\" style=\"color:#555;font-size:14px;\"></p>"
            "<a id=\"logDownloadDirectLink\" href=\"#\" style=\"display:none;font-size:17px;font-weight:bold;color:#1976d2;"
            "margin:8px 0;\"></a>"
            "<p style=\"color:#777;font-size:13px;\">Передача по HTTP в локальной Wi‑Fi. Chrome может показать "
            "«Невозможно безопасно скачать файл» — это предупреждение, не ошибка: файл обычно всё равно "
            "появляется в «Загрузках».</p>"
            "<p style=\"color:#555;font-size:14px;\">В архиве каталог <code>OnyxLog</code> и при наличии файл <code>FOTEK/userProg.db</code>. "
            "Пароль на архив такой же, как у входящего ZIP с обновлением ПО.</p>"
            "<script>"
            "(function(){"
            "var sessionToken='%4';"
            "var logBtn=document.getElementById('logDownloadBtn');"
            "var logStatus=document.getElementById('logDownloadStatus');"
            "function setLogStatus(msg){ if(logStatus) logStatus.textContent=msg||''; }"
            "function xhrNetDebug(prefix,xhr){"
            "var parts=[prefix||'Ошибка'];"
            "if(xhr){"
            "parts.push('readyState='+xhr.readyState);"
            "parts.push('status='+xhr.status);"
            "if(xhr.statusText) parts.push(xhr.statusText);"
            "}"
            "return parts.join(', ');"
            "}"
            "var logZipLoading=false;"
            "function ensureDownloadFrame(){"
            "var frame=document.getElementById('logDownloadFrame');"
            "if(!frame){"
            "frame=document.createElement('iframe');"
            "frame.id='logDownloadFrame';"
            "frame.style.cssText='display:none;width:0;height:0;border:0';"
            "frame.title='download';"
            "document.body.appendChild(frame);"
            "}"
            "return frame;"
            "}"
            "function fetchPreparedZip(fileSize){"
            "if(logZipLoading) return;"
            "logZipLoading=true;"
            "var url='/download/onyxlog-bundle.zip?token='+encodeURIComponent(sessionToken)+'&_='+(Date.now());"
            "if(logBtn) logBtn.disabled=false;"
            "var sizeHint=(fileSize>0)?(' ('+fileSize+' байт)'):'';"
            "setLogStatus('Архив отправлен'+sizeHint+'. Проверьте «Загрузки». Предупреждение Chrome о HTTP можно игнорировать.');"
            "var link=document.getElementById('logDownloadDirectLink');"
            "if(link){"
            "link.style.display='inline-block';"
            "link.href=url;"
            "link.textContent='Скачать onyxlog.zip ещё раз';"
            "}"
            "ensureDownloadFrame().src=url;"
            "setTimeout(function(){ logZipLoading=false; },3000);"
            "}"
            "function pollArchiveStatus(forceFresh){"
            "var statusUrl='/download/status?token='+encodeURIComponent(sessionToken)"
            "+'&fresh='+(forceFresh?1:0);"
            "var sxhr=new XMLHttpRequest();"
            "sxhr.open('GET',statusUrl,true);"
            "sxhr.timeout=30000;"
            "sxhr.onload=function(){"
            "if(sxhr.status===403){"
            "if(logBtn) logBtn.disabled=false;"
            "var forbidMsg='Доступ запрещён (403)';"
            "try{"
            "var fd=JSON.parse(sxhr.responseText);"
            "if(fd.message) forbidMsg=fd.message;"
            "if(fd.reason==='token') forbidMsg='Сессия устарела. Закройте вкладку и откройте страницу снова по QR.';"
            "else if(fd.reason==='client') forbidMsg='Откройте страницу загрузки на этом телефоне и повторите.';"
            "}catch(e){}"
            "setLogStatus(forbidMsg);"
            "return;"
            "}"
            "if(sxhr.status<200||sxhr.status>=300){"
            "if(logBtn) logBtn.disabled=false;"
            "setLogStatus(xhrNetDebug('Ошибка подготовки',sxhr));"
            "return;"
            "}"
            "var data=null;"
            "try{ data=JSON.parse(sxhr.responseText); }catch(e){"
            "if(logBtn) logBtn.disabled=false;"
            "setLogStatus('Ошибка ответа сервера: '+String(sxhr.responseText).slice(0,120));"
            "return;"
            "}"
            "if(data.state==='building'){"
            "setLogStatus('Подготовка архива…'+(data.debug?' ['+data.debug+']':''));"
            "setTimeout(function(){ pollArchiveStatus(false); },1500);"
            "return;"
            "}"
            "if(data.state==='error'){"
            "if(!forceFresh){ pollArchiveStatus(true); return; }"
            "if(logBtn) logBtn.disabled=false;"
            "var errMsg=data.message||'Ошибка создания архива';"
            "if(data.debug) errMsg+=' ['+data.debug+']';"
            "setLogStatus(errMsg);"
            "return;"
            "}"
            "if(data.state==='ready'){"
            "var sz=(data.size!=null)?data.size:0;"
            "setLogStatus('Архив готов'+(sz?(' ('+sz+' байт)'):'')+'. Запуск передачи…');"
            "setTimeout(function(){ fetchPreparedZip(sz); },200);"
            "return;"
            "}"
            "if(logBtn) logBtn.disabled=false;"
            "setLogStatus('Неизвестный статус: '+JSON.stringify(data));"
            "};"
            "sxhr.onerror=function(){ if(logBtn) logBtn.disabled=false; setLogStatus(xhrNetDebug('Ошибка сети при подготовке',sxhr)); };"
            "sxhr.ontimeout=function(){ if(logBtn) logBtn.disabled=false; setLogStatus(xhrNetDebug('Таймаут при подготовке',sxhr)); };"
            "sxhr.send();"
            "}"
            "function downloadLogArchive(){"
            "if(!sessionToken){ setLogStatus('Сессия недоступна'); return; }"
            "if(logBtn) logBtn.disabled=true;"
            "setLogStatus('Подготовка архива, подождите…');"
            "pollArchiveStatus(false);"
            "}"
            "if(logBtn){ logBtn.addEventListener('click',downloadLogArchive); }"
            "var form=document.getElementById('uploadForm');"
            "if(!form) return;"
            "var fileInput=document.getElementById('fileInput');"
            "var pickBtn=document.getElementById('pickFileBtn');"
            "var selectedFiles=document.getElementById('selectedFiles');"
            "if(pickBtn && fileInput){"
            "pickBtn.addEventListener('click',function(){ fileInput.click(); });"
            "fileInput.addEventListener('change',function(){"
            "if(!selectedFiles) return;"
            "var n=(fileInput.files&&fileInput.files.length)?fileInput.files.length:0;"
            "if(n===0){ selectedFiles.textContent='Файл не выбран'; return; }"
            "selectedFiles.textContent='Выбран ' + fileInput.files[0].name;"
            "});"
            "}"
            "form.addEventListener('submit',function(ev){"
            "ev.preventDefault();"
            "var res=document.getElementById('result');"
            "var wrap=document.getElementById('progressWrap');"
            "var bar=document.getElementById('progressBar');"
            "var txt=document.getElementById('progressText');"
            "if(!fileInput || !fileInput.files || fileInput.files.length===0){"
            "if(res){res.innerHTML='<p>Файлы не выбраны</p>';}"
            "if(wrap){wrap.style.display='none';}"
            "return;"
            "}"
            "var nameRe=/.+-\\d+\\.\\d+-\\d+\\.\\d+-\\d+\\.zip$/i;"
            "for(var i=0;i<fileInput.files.length;i++){"
            "var fname=(fileInput.files[i]&&fileInput.files[i].name)?fileInput.files[i].name:'';"
            "if(!nameRe.test(fname)){"
            "if(res){res.innerHTML='<p>Имя файла должно быть в формате name-a.b-c.d-e.zip</p>';}"
            "if(wrap){wrap.style.display='none';}"
            "return;"
            "}"
            "}"
            "var fd=new FormData(form);"
            "wrap.style.display='block';"
            "bar.style.width='0%';"
            "txt.textContent='0%';"
            "res.innerHTML='';"
            "var xhr=new XMLHttpRequest();"
            "xhr.open('POST','/upload',true);"
            "xhr.upload.onprogress=function(e){"
            "if(!e.lengthComputable) return;"
            "var p=Math.max(0,Math.min(100,Math.round((e.loaded/e.total)*100)));"
            "bar.style.width=p+'%';"
            "txt.textContent='Отправка: '+p+'%';"
            "};"
            "xhr.onload=function(){"
            "res.innerHTML=xhr.responseText||'';"
            "if(xhr.status>=200 && xhr.status<300){"
            "bar.style.width='100%'; txt.textContent='Отправка завершена';"
            "}else{"
            "txt.textContent='Ошибка отправки ('+xhr.status+')';"
            "}"
            "};"
            "xhr.onerror=function(){ txt.textContent='Ошибка сети при отправке'; };"
            "xhr.send(fd);"
            "});"
            "})();"
            "</script>"
            "</body></html>")
                                 .arg(token, serialHtml, typeHtml, tokenJs); // %1 token, %2 serial, %3 type, %4 JS token
    return html.toUtf8();
}

bool HttpUploadController::extractMultipartBoundary(const QString &contentType, QByteArray *boundaryPrefixOut)
{
    const int idx = contentType.indexOf(QStringLiteral("boundary="), 0, Qt::CaseInsensitive);
    if (idx < 0) {
        return false;
    }
    QString b = contentType.mid(idx + 9).trimmed();
    if (b.startsWith(QLatin1Char('"'))) {
        const int end = b.indexOf(QLatin1Char('"'), 1);
        if (end > 1) {
            b = b.mid(1, end - 1);
        }
    } else {
        const int semi = b.indexOf(QLatin1Char(';'));
        if (semi >= 0) {
            b = b.left(semi).trimmed();
        }
    }
    if (b.isEmpty()) {
        return false;
    }
    *boundaryPrefixOut = QByteArray("--") + b.toUtf8();
    return true;
}

QString HttpUploadController::sanitizeFileName(const QString &rawName)
{
    QString base = rawName.trimmed();
    base.replace(QRegularExpression(QStringLiteral("[\\\\/]+")), QStringLiteral("_"));
    base.replace(QStringLiteral(".."), QStringLiteral("_"));
    if (base.length() > 200) {
        base = base.left(200);
    }
    return base;
}

QString HttpUploadController::makeUniquePath(const QString &fileName) const
{
    const QString dir = effectiveUploadDir();
    QString base = fileName;
    const int dot = base.lastIndexOf(QLatin1Char('.'));
    QString stem = dot > 0 ? base.left(dot) : base;
    QString ext = dot > 0 ? base.mid(dot) : QString();
    QString path = dir + QLatin1Char('/') + base;
    int n = 1;
    while (QFile::exists(path)) {
        path = dir + QLatin1Char('/') + stem + QLatin1Char('_') + QString::number(n) + ext;
        ++n;
    }
    return path;
}

QString HttpUploadController::normalizeRelPath(const QString &rawRelPath)
{
    const QString clean = QDir::cleanPath(rawRelPath.trimmed());
    if (clean.isEmpty() || clean == QStringLiteral(".")
        || clean.startsWith(QLatin1Char('/'))
        || clean.startsWith(QStringLiteral("../"))
        || clean.contains(QStringLiteral("/../"))) {
        return QString();
    }
    return clean;
}

bool HttpUploadController::copyFileReplace(const QString &srcPath, const QString &dstPath, QString *errorMessage)
{
    QFileInfo srcFi(srcPath);
    if (!srcFi.exists() || !srcFi.isFile()) {
        *errorMessage = QStringLiteral("file not found");
        return false;
    }
    if (!QDir().mkpath(QFileInfo(dstPath).absolutePath())) {
        *errorMessage = QStringLiteral("mkdir");
        return false;
    }
    if (QFile::exists(dstPath) && !QFile::remove(dstPath)) {
        *errorMessage = QStringLiteral("remove old");
        return false;
    }
    if (!QFile::copy(srcPath, dstPath)) {
        *errorMessage = QStringLiteral("copy");
        return false;
    }
    QFile::setPermissions(dstPath, QFile::permissions(srcPath));
    return true;
}

bool HttpUploadController::copyDirectoryContentsReplace(const QString &srcDirPath, const QString &dstDirPath, QString *errorMessage)
{
    QDir srcDir(srcDirPath);
    if (!srcDir.exists()) {
        *errorMessage = QStringLiteral("source dir missing");
        return false;
    }

    if (!QDir().mkpath(dstDirPath)) {
        *errorMessage = QStringLiteral("mkdir target");
        return false;
    }

    QDirIterator it(srcDirPath, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString srcPath = it.next();
        const QFileInfo srcFi(srcPath);
        const QString rel = srcDir.relativeFilePath(srcPath);
        const QString dstPath = QDir(dstDirPath).filePath(rel);
        if (srcFi.isDir()) {
            if (!QDir().mkpath(dstPath)) {
                *errorMessage = QStringLiteral("mkdir child");
                return false;
            }
            continue;
        }
        if (srcFi.isFile()) {
            QString copyErr;
            if (!copyFileReplace(srcPath, dstPath, &copyErr)) {
                *errorMessage = QStringLiteral("copy file");
                return false;
            }
        }
    }
    return true;
}

void HttpUploadController::setDetectedReleaseInfo(const QString &releaseVer,
                                                  const QString &binaryVer,
                                                  const QString &mediaVer,
                                                  const QString &comVer,
                                                  const QString &argVer,
                                                  const QString &genVer)
{
    if (m_detectedReleaseVersion == releaseVer
        && m_detectedBinaryVersion == binaryVer
        && m_detectedMediaVersion == mediaVer
        && m_detectedComVersion == comVer
        && m_detectedArgVersion == argVer
        && m_detectedGenVersion == genVer) {
        return;
    }
    m_detectedReleaseVersion = releaseVer;
    m_detectedBinaryVersion = binaryVer;
    m_detectedMediaVersion = mediaVer;
    m_detectedComVersion = comVer;
    m_detectedArgVersion = argVer;
    m_detectedGenVersion = genVer;
    emit detectedReleaseChanged();
}

bool HttpUploadController::processReleaseArchiveBytes(const QString &sourceFileName,
                                                      const QByteArray &archiveBytes,
                                                      QString *errorMessage)
{
    const QString baseName = QFileInfo(sourceFileName).fileName();
    const QRegularExpressionMatch fileMatch = kReleaseZipNameRe.match(baseName);
    if (!fileMatch.hasMatch()) {
        *errorMessage = QStringLiteral("invalid release name");
        return false;
    }

    const QString releaseVersion = QStringLiteral("%1.%2-%3.%4-%5")
                                   .arg(fileMatch.captured(1),
                                        fileMatch.captured(2),
                                        fileMatch.captured(3),
                                        fileMatch.captured(4),
                                        fileMatch.captured(5));
    const QString fallbackBinaryVersion = QStringLiteral("%1.%2")
                                          .arg(fileMatch.captured(1), fileMatch.captured(2));
    const QString fallbackMediaVersion = QStringLiteral("%1.%2")
                                         .arg(fileMatch.captured(3), fileMatch.captured(4));

    QTemporaryDir tmpRoot;
    if (!tmpRoot.isValid()) {
        *errorMessage = QStringLiteral("tmp");
        return false;
    }
    const QString archivePath = tmpRoot.path() + QStringLiteral("/incoming.zip");
    QFile af(archivePath);
    if (!af.open(QIODevice::WriteOnly) || af.write(archiveBytes) != archiveBytes.size()) {
        *errorMessage = QStringLiteral("write archive");
        return false;
    }
    af.close();

    const QString unpackRoot = tmpRoot.path() + QStringLiteral("/unpacked");
    if (!QDir().mkpath(unpackRoot)) {
        *errorMessage = QStringLiteral("mkdir unpack");
        return false;
    }

    const QString unzipProgram = resolveUnzipProgramPath();
    if (unzipProgram.isEmpty()) {
        *errorMessage = QStringLiteral("unzip");
        return false;
    }

    auto tryUnzip = [&](const QStringList &args, QString *stderrOut) -> bool {
        QProcess unzipProc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString oldPath = env.value(QStringLiteral("PATH"));
        env.insert(QStringLiteral("PATH"),
                   oldPath.isEmpty()
                   ? QStringLiteral("/usr/local/bin:/usr/bin:/bin")
                   : oldPath + QStringLiteral(":/usr/local/bin:/usr/bin:/bin"));
        unzipProc.setProcessEnvironment(env);
        unzipProc.start(unzipProgram, args);
        if (!unzipProc.waitForStarted(2000)) {
            if (stderrOut) {
                *stderrOut = QStringLiteral("unzip start failed: %1").arg(unzipProgram);
            }
            return false;
        }
        if (!unzipProc.waitForFinished(120000)) {
            if (stderrOut) {
                *stderrOut = QStringLiteral("unzip timeout");
            }
            return false;
        }
        const QString stderrText = QString::fromUtf8(unzipProc.readAllStandardError());
        if (stderrOut) {
            *stderrOut = stderrText.trimmed();
        }
        return unzipProc.exitCode() == 0;
    };

    QString unzipErr;
    if (!tryUnzip(QStringList()
                  << QStringLiteral("-P") << QString::fromUtf8(kReleaseZipPassword)
                  << QStringLiteral("-o") << archivePath
                  << QStringLiteral("-d") << unpackRoot,
                  &unzipErr)) {
        QDir(unpackRoot).removeRecursively();
        QDir().mkpath(unpackRoot);
        if (!tryUnzip(QStringList()
                      << QStringLiteral("-o") << archivePath
                      << QStringLiteral("-d") << unpackRoot,
                      &unzipErr)) {
            *errorMessage = QStringLiteral("unzip");
            return false;
        }
    }

    QString payloadRoot = selectPayloadRoot(unpackRoot);
    QString manifestPath = QDir(payloadRoot).filePath(QStringLiteral("update-manifest.json"));
    if (!QFileInfo::exists(manifestPath)) {
        const QString foundManifest = findManifestPathRecursive(unpackRoot);
        if (!foundManifest.isEmpty()) {
            manifestPath = foundManifest;
            payloadRoot = QFileInfo(foundManifest).absolutePath();
        }
    }

    QFile mf(manifestPath);
    if (!mf.open(QIODevice::ReadOnly)) {
        *errorMessage = QStringLiteral("manifest missing");
        return false;
    }
    const QJsonDocument manifestDoc = QJsonDocument::fromJson(mf.readAll());
    mf.close();
    if (!manifestDoc.isObject()) {
        *errorMessage = QStringLiteral("manifest invalid");
        return false;
    }
    const QJsonObject manifestObj = manifestDoc.object();

    QJsonArray deployPaths = manifestObj.value(QStringLiteral("deployPaths")).toArray();
    if (deployPaths.isEmpty()) {
        // Для ручной загрузки допускаем минимальный манифест без deployPaths:
        // берём все файлы payload, кроме update-manifest.json.
        QDirIterator it(payloadRoot, QDir::Files, QDirIterator::Subdirectories);
        QDir root(payloadRoot);
        QStringList allFiles;
        while (it.hasNext()) {
            const QString rel = root.relativeFilePath(it.next());
            if (rel != QStringLiteral("update-manifest.json")) {
                allFiles.append(rel);
            }
        }
        std::sort(allFiles.begin(), allFiles.end());
        for (const QString &rel : allFiles) {
            deployPaths.append(rel);
        }
        if (deployPaths.isEmpty()) {
            *errorMessage = QStringLiteral("manifest deployPaths empty");
            return false;
        }
    }

    const QString expectedPayloadSha256 =
        manifestObj.value(QStringLiteral("payloadSha256")).toString().trimmed().toLower();
    if (expectedPayloadSha256.isEmpty()) {
        *errorMessage = QStringLiteral("manifest payload sha missing");
        return false;
    }
    const QString actualPayloadSha256 = payloadSha256Hex(payloadRoot).trimmed().toLower();
    if (actualPayloadSha256.isEmpty()) {
        *errorMessage = QStringLiteral("payload sha calc");
        return false;
    }
    if (expectedPayloadSha256 != actualPayloadSha256) {
        *errorMessage = QStringLiteral("payload sha mismatch");
        return false;
    }

    QString binaryVersion = manifestObj.value(QStringLiteral("binaryVersion")).toString().trimmed();
    if (binaryVersion.isEmpty()) {
        binaryVersion = fallbackBinaryVersion;
    }
    QString mediaVersion = manifestObj.value(QStringLiteral("mediaVersion")).toString().trimmed();
    if (mediaVersion.isEmpty()) {
        mediaVersion = fallbackMediaVersion;
    }

    QString fwCom;
    QString fwArg;
    QString fwGen;
    const QJsonObject fwObj = manifestObj.value(QStringLiteral("firmware")).toObject();
    if (!fwObj.isEmpty()) {
        fwCom = fwObj.value(QStringLiteral("com")).toString().trimmed();
        fwArg = fwObj.value(QStringLiteral("arg")).toString().trimmed();
        fwGen = fwObj.value(QStringLiteral("gen")).toString().trimmed();
    }

    const AppPaths &paths = AppPaths::instance();
    const QString mainRoot = paths.releasesMainVersionDir(binaryVersion);
    const QString mediaRoot = paths.releasesMediaVersionDir(mediaVersion);
    const QString comRoot = paths.releasesComDir();
    const QString argRoot = paths.releasesArgDir();
    const QString genRoot = paths.releasesGenDir();

    for (const QJsonValue &v : deployPaths) {
        const QString rel = normalizeRelPath(v.toString());
        if (rel.isEmpty()) {
            *errorMessage = QStringLiteral("manifest path invalid");
            return false;
        }
        const QString srcPath = QDir(payloadRoot).filePath(rel);
        if (!QFileInfo::exists(srcPath)) {
            *errorMessage = QStringLiteral("manifest path missing");
            return false;
        }

        const QString base = QFileInfo(rel).fileName();
        const QRegularExpressionMatch fwMatch = kFirmwareFileRe.match(base);
        QString dstPath;
        if (base == QStringLiteral("UserInterface")) {
            dstPath = QDir(mainRoot).filePath(QStringLiteral("UserInterface"));
        } else if (fwMatch.hasMatch()) {
            const QString kind = fwMatch.captured(1).toLower();
            const QString ver = fwMatch.captured(2);
            if (kind == QStringLiteral("com")) {
                dstPath = QDir(comRoot).filePath(base);
                if (fwCom.isEmpty()) {
                    fwCom = ver;
                }
            } else if (kind == QStringLiteral("arg")) {
                dstPath = QDir(argRoot).filePath(base);
                if (fwArg.isEmpty()) {
                    fwArg = ver;
                }
            } else if (kind == QStringLiteral("gen")) {
                dstPath = QDir(genRoot).filePath(base);
                if (fwGen.isEmpty()) {
                    fwGen = ver;
                }
            }
        } else {
            dstPath = QDir(mediaRoot).filePath(rel);
        }

        QString copyErr;
        if (!copyFileReplace(srcPath, dstPath, &copyErr)) {
            *errorMessage = QStringLiteral("copy failed");
            return false;
        }
    }

    setDetectedReleaseInfo(releaseVersion, binaryVersion, mediaVersion, fwCom, fwArg, fwGen);
    refreshReleaseVersions();
    if (m_json) {
        m_json->saveString(QStringLiteral("currentBinaryVersion"), binaryVersion);
        m_json->saveString(QStringLiteral("currentMediaVersion"), mediaVersion);
    }
    setCurrentMediaVersion(mediaVersion);
    return true;
}

bool HttpUploadController::parseMultipartAndSave(const QByteArray &body, const QString &contentType, int *filesSaved,
                                                 QString *errorMessage)
{
    *filesSaved = 0;
    QByteArray delim;
    if (!extractMultipartBoundary(contentType, &delim)) {
        *errorMessage = QStringLiteral("boundary");
        return false;
    }

    const QByteArray sep = QByteArray("\r\n") + delim;

    int pos = body.indexOf(delim);
    if (pos < 0) {
        *errorMessage = QStringLiteral("no boundary");
        return false;
    }
    pos += delim.size();
    if (pos + 1 < body.size() && body[pos] == '\r') {
        ++pos;
    }
    if (pos < body.size() && body[pos] == '\n') {
        ++pos;
    }

    QString formToken;
    bool sawInvalidReleaseZipName = false;

    while (pos < body.size()) {
        const int headerEnd = body.indexOf("\r\n\r\n", pos);
        if (headerEnd < 0) {
            *errorMessage = QStringLiteral("truncated headers");
            return false;
        }
        const QByteArray partHdr = body.mid(pos, headerEnd - pos);
        const int contentStart = headerEnd + 4;

        QString hdrStr = QString::fromUtf8(partHdr);
        QString nameField;
        QString fileName;
        const QRegularExpression nameRe(QStringLiteral("name=\"([^\"]*)\""));
        const QRegularExpression fnRe(QStringLiteral("filename=\"([^\"]*)\""));
        {
            const auto m = nameRe.match(hdrStr);
            if (m.hasMatch()) {
                nameField = m.captured(1);
            }
            const auto mf = fnRe.match(hdrStr);
            if (mf.hasMatch()) {
                fileName = mf.captured(1);
            }
        }

        const int sepPos = body.indexOf(sep, contentStart);
        if (sepPos < 0) {
            *errorMessage = QStringLiteral("truncated part");
            return false;
        }
        const QByteArray partBody = body.mid(contentStart, sepPos - contentStart);
        const int afterBoundary = sepPos + sep.size();
        const bool isClosing = (afterBoundary + 1 < body.size() && body[afterBoundary] == '-'
                                && body[afterBoundary + 1] == '-');

        if (nameField == QStringLiteral("token")) {
            formToken = QString::fromUtf8(partBody).trimmed();
        } else if (nameField == QStringLiteral("file") && !fileName.isEmpty()) {
            const QString safe = sanitizeFileName(QFileInfo(fileName).fileName());
            if (!safe.isEmpty()) {
                if (*filesSaved >= kMaxFilesPerRequest) {
                    *errorMessage = QStringLiteral("too many files");
                    return false;
                }
                // Разрешаем отправлять вместе с архивом сопутствующие файлы,
                // но обрабатываем только zip-релизы формата name-a.b-c.d-e.zip.
                if (!safe.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                    if (isClosing) {
                        break;
                    }
                    pos = afterBoundary;
                    if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') {
                        pos += 2;
                    } else if (pos < body.size() && body[pos] == '\n') {
                        ++pos;
                    }
                    continue;
                }
                if (!kReleaseZipNameRe.match(safe).hasMatch()) {
                    sawInvalidReleaseZipName = true;
                    if (isClosing) {
                        break;
                    }
                    pos = afterBoundary;
                    if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') {
                        pos += 2;
                    } else if (pos < body.size() && body[pos] == '\n') {
                        ++pos;
                    }
                    continue;
                }
                QString processError;
                if (!processReleaseArchiveBytes(safe, partBody, &processError)) {
                    if (processError == QStringLiteral("invalid release name")) {
                        *errorMessage = QStringLiteral("invalid release name");
                    } else if (processError == QStringLiteral("manifest missing")) {
                        *errorMessage = QStringLiteral("manifest-missing");
                    } else if (processError == QStringLiteral("manifest invalid")) {
                        *errorMessage = QStringLiteral("manifest-invalid");
                    } else if (processError == QStringLiteral("manifest deployPaths empty")) {
                        *errorMessage = QStringLiteral("manifest-deploypaths");
                    } else if (processError == QStringLiteral("manifest payload sha missing")) {
                        *errorMessage = QStringLiteral("manifest-sha-missing");
                    } else if (processError == QStringLiteral("manifest path invalid")
                               || processError == QStringLiteral("manifest path missing")) {
                        *errorMessage = QStringLiteral("manifest-path");
                    } else if (processError == QStringLiteral("payload sha calc")
                               || processError == QStringLiteral("payload sha mismatch")) {
                        *errorMessage = QStringLiteral("checksum");
                    } else if (processError == QStringLiteral("unzip")) {
                        *errorMessage = QStringLiteral("unzip");
                    } else {
                        *errorMessage = QStringLiteral("write");
                    }
                    return false;
                }
                ++(*filesSaved);
            }
        }

        if (isClosing) {
            break;
        }
        pos = afterBoundary;
        if (pos + 1 < body.size() && body[pos] == '\r' && body[pos + 1] == '\n') {
            pos += 2;
        } else if (pos < body.size() && body[pos] == '\n') {
            ++pos;
        }
    }

    if (!m_active || m_sessionToken.isEmpty() || formToken != m_sessionToken) {
        *errorMessage = QStringLiteral("token");
        return false;
    }
    if (*filesSaved == 0) {
        *errorMessage = sawInvalidReleaseZipName
                ? QStringLiteral("invalid release name")
                : QStringLiteral("no files");
        return false;
    }
    return true;
}

void HttpUploadController::tryProcessBuffer()
{
    if (!m_client) {
        return;
    }

    if (!m_headerComplete) {
        const int sep = m_rxBuffer.indexOf("\r\n\r\n");
        if (sep < 0) {
            if (m_rxBuffer.size() > 65536) {
                sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"), QStringLiteral("<p>Слишком длинные заголовки</p>"));
                if (m_client) {
                    m_client->disconnectFromHost();
                }
            }
            return;
        }

        const QByteArray headerBlob = m_rxBuffer.left(sep);
        m_rxBuffer.remove(0, sep + 4);

        const QList<QByteArray> lines = headerBlob.split('\n');
        if (lines.isEmpty()) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"), QStringLiteral("<p>Некорректный запрос</p>"));
            if (m_client) {
                m_client->disconnectFromHost();
            }
            return;
        }

        const QByteArray reqLine = lines.first().trimmed();
        const QList<QByteArray> parts = reqLine.split(' ');
        if (parts.size() < 2) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"), QStringLiteral("<p>Некорректная строка запроса</p>"));
            if (m_client) {
                m_client->disconnectFromHost();
            }
            return;
        }
        m_method = QString::fromLatin1(parts[0]);
        m_path = QString::fromLatin1(parts[1]);
        const int q = m_path.indexOf(QLatin1Char('?'));
        if (q >= 0) {
            m_path = m_path.left(q);
        }

        m_requestHeaders.clear();
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray ln = lines.at(i).trimmed();
            const int c = ln.indexOf(':');
            if (c > 0) {
                const QString k = QString::fromLatin1(ln.left(c).trimmed().toLower());
                const QString v = QString::fromLatin1(ln.mid(c + 1).trimmed());
                m_requestHeaders.insert(k, v);
            }
        }

        m_headerComplete = true;
        const QHostAddress peerAddress = effectiveClientAddress();
        const QString peerText = peerAddress.toString();
        qWarning() << "HttpUploadController: request" << m_method << m_path << "from" << peerText;

        if (m_method == QStringLiteral("GET")) {
            if (m_path == QStringLiteral("/") || m_path.isEmpty()) {
                if (m_authorizedClientAddress.isNull()) {
                    bindAuthorizedClient(peerAddress);
                } else if (!isAuthorizedClient(peerAddress)) {
                    qWarning() << "HttpUploadController: rejected GET from unauthorized client" << peerText;
                    sendSimpleHtml(m_client, 403, QStringLiteral("Доступ запрещён"),
                                   QStringLiteral("<p>Сессия уже открыта на другом устройстве.</p>"));
                    releaseClientSocket();
                    return;
                }
                sendHttpResponse(m_client, 200, "text/html; charset=utf-8", buildUploadPageHtml());
            } else if (m_path.startsWith(QStringLiteral("/download/status"))) {
                const QString requestTarget = QString::fromLatin1(parts[1]);
                if (!isValidTokenInPath(requestTarget)) {
                    qWarning() << "HttpUploadController: /download/status invalid token from" << peerText
                               << "got=" << requestQueryValue(requestTarget, QStringLiteral("token"))
                               << "expected=" << m_sessionToken;
                    sendDownloadForbidden(m_client, QStringLiteral("token"),
                                          QStringLiteral("Сессия устарела. Откройте страницу заново по QR."));
                } else if (!isAuthorizedClient(peerAddress)) {
                    qWarning() << "HttpUploadController: /download/status client mismatch from" << peerText
                               << "bound=" << m_authorizedClientAddress.toString();
                    sendDownloadForbidden(m_client, QStringLiteral("client"),
                                          QStringLiteral("Скачивание доступно только с устройства, открывшего страницу."));
                } else {
                    if (m_authorizedClientAddress.isNull()) {
                        bindAuthorizedClient(peerAddress);
                    }
                    const bool forceRestart = requestQueryValue(requestTarget, QStringLiteral("fresh"))
                            == QStringLiteral("1");
                    qWarning() << "HttpUploadController: GET /download/status from" << peerText
                               << "fresh=" << forceRestart
                               << "cache=" << logArchiveCacheDebugText();
                    startLogArchiveBuildIfNeeded(forceRestart);
                    sendJsonResponse(m_client, 200, logArchiveStatusJson());
                }
            } else if (m_path.startsWith(QStringLiteral("/download/onyxlog-bundle.zip"))
                       || m_path.startsWith(QStringLiteral("/download/logFile.txt"))) {
                const QString requestTarget = QString::fromLatin1(parts[1]);
                if (!isValidTokenInPath(requestTarget)) {
                    qWarning() << "HttpUploadController: GET download zip invalid token from" << peerText;
                    sendDownloadForbidden(m_client, QStringLiteral("token"),
                                          QStringLiteral("Сессия устарела. Откройте страницу заново по QR."));
                } else if (!isAuthorizedClient(peerAddress)) {
                    qWarning() << "HttpUploadController: rejected log download from unauthorized client" << peerText
                               << "bound=" << m_authorizedClientAddress.toString();
                    sendDownloadForbidden(m_client, QStringLiteral("client"),
                                          QStringLiteral("Скачивание доступно только с устройства, открывшего страницу."));
                } else {
                    if (m_authorizedClientAddress.isNull()) {
                        bindAuthorizedClient(peerAddress);
                    }
                    qWarning() << "HttpUploadController: GET download zip from" << peerText
                               << "cache=" << logArchiveCacheDebugText();
                    if (!servePreparedLogArchive(m_client)) {
                        qWarning() << "HttpUploadController: GET download zip not ready, 503";
                        sendJsonResponse(m_client, 503, logArchiveStatusJson());
                    }
                }
            } else if (m_path == QStringLiteral("/favicon.ico")) {
                sendHttpResponse(m_client, 204, QByteArray(), QByteArray());
            } else {
                sendSimpleHtml(m_client, 404, QStringLiteral("Не найдено"), QStringLiteral("<p>Страница не найдена</p>"));
            }
            releaseClientSocket();
            return;
        }

        if (m_method != QStringLiteral("POST")) {
            sendSimpleHtml(m_client, 404, QStringLiteral("Не найдено"), QStringLiteral("<p>Метод не поддерживается</p>"));
            releaseClientSocket();
            return;
        }

        if (m_path != QStringLiteral("/upload")) {
            sendSimpleHtml(m_client, 404, QStringLiteral("Не найдено"), QStringLiteral("<p>Страница не найдена</p>"));
            releaseClientSocket();
            return;
        }
        if (!m_active || m_sessionToken.isEmpty()) {
            qWarning() << "HttpUploadController: rejected upload without active session from" << peerText;
            sendSimpleHtml(m_client, 403, QStringLiteral("Доступ запрещён"),
                           QStringLiteral("<p>Сессия загрузки не активна. Откройте страницу заново с устройства.</p>"));
            releaseClientSocket();
            return;
        }
        if (!isAuthorizedClient(peerAddress)) {
            qWarning() << "HttpUploadController: rejected upload from unauthorized client" << peerText;
            sendSimpleHtml(m_client, 403, QStringLiteral("Доступ запрещён"),
                           QStringLiteral("<p>Загрузка доступна только с устройства, открывшего сессию.</p>"));
            releaseClientSocket();
            return;
        }

        const QString cl = m_requestHeaders.value(QStringLiteral("content-length"));
        if (cl.isEmpty()) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>Нужен заголовок Content-Length</p>"));
            releaseClientSocket();
            return;
        }
        bool okLen = false;
        m_contentLength = cl.toLongLong(&okLen);
        if (!okLen || m_contentLength < 0 || m_contentLength > kMaxBodyBytes) {
            sendSimpleHtml(m_client, 413, QStringLiteral("Слишком большой"),
                           QStringLiteral("<p>Размер запроса превышает допустимый</p>"));
            releaseClientSocket();
            return;
        }
        m_uploadInProgress = true;
        m_uploadProgress = 0.0;
        m_uploadStatusText = tr("Получение файла...");
        emit uploadProgressChanged();
    }

    // body
    if (m_contentLength < 0) {
        return;
    }
    if (m_rxBuffer.size() > m_contentLength) {
        sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"), QStringLiteral("<p>Лишние данные в теле запроса</p>"));
        releaseClientSocket();
        return;
    }
    if (m_rxBuffer.size() < m_contentLength) {
        if (m_contentLength > 0 && m_uploadInProgress) {
            const double p = static_cast<double>(m_rxBuffer.size()) / static_cast<double>(m_contentLength);
            m_uploadProgress = p;
            if (m_uploadProgress < 0.0) m_uploadProgress = 0.0;
            if (m_uploadProgress > 1.0) m_uploadProgress = 1.0;
            m_uploadStatusText = tr("Получение файла: %1%")
                                 .arg(QString::number(m_uploadProgress * 100.0, 'f', 0));
            emit uploadProgressChanged();
        }
        return;
    }

    const QByteArray body = m_rxBuffer.left(static_cast<int>(m_contentLength));
    m_rxBuffer.clear();

    const QString ct = m_requestHeaders.value(QStringLiteral("content-type"));
    if (!ct.contains(QStringLiteral("multipart/form-data"), Qt::CaseInsensitive)) {
        sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"), QStringLiteral("<p>Ожидается multipart/form-data</p>"));
        releaseClientSocket();
        return;
    }

    int nFiles = 0;
    QString err;
    if (!parseMultipartAndSave(body, ct, &nFiles, &err)) {
        m_uploadInProgress = false;
        m_uploadProgress = 0.0;
        m_uploadStatusText = tr("Ошибка загрузки");
        emit uploadProgressChanged();
        if (err == QStringLiteral("token")) {
            sendSimpleHtml(m_client, 403, QStringLiteral("Доступ запрещён"),
                           QStringLiteral("<p>Неверный или устаревший токен. Откройте страницу снова с устройства.</p>"));
        } else if (err == QStringLiteral("no files")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"), QStringLiteral("<p>Файлы не выбраны</p>"));
        } else if (err == QStringLiteral("too many files")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"), QStringLiteral("<p>Слишком много файлов за один раз</p>"));
        } else if (err == QStringLiteral("invalid release name")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>Имя файла должно быть в формате name-a.b-c.d-e.zip</p>"));
        } else if (err == QStringLiteral("manifest-missing")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>В архиве не найден update-manifest.json</p>"));
        } else if (err == QStringLiteral("manifest-invalid")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>update-manifest.json повреждён или не является JSON-объектом</p>"));
        } else if (err == QStringLiteral("manifest-deploypaths")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>В архиве нет файлов для раскладки (deployPaths пуст и payload пуст)</p>"));
        } else if (err == QStringLiteral("manifest-sha-missing")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>В update-manifest.json отсутствует поле payloadSha256</p>"));
        } else if (err == QStringLiteral("manifest-path")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>Пути из update-manifest.json некорректны или отсутствуют в payload</p>"));
        } else if (err == QStringLiteral("checksum")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>Контрольная сумма payloadSha256 не прошла проверку</p>"));
        } else if (err == QStringLiteral("unzip")) {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>Не удалось открыть zip-архив. Проверьте пароль/целостность архива.</p>"));
        } else {
            sendSimpleHtml(m_client, 400, QStringLiteral("Ошибка"),
                           QStringLiteral("<p>Не удалось разобрать данные формы</p>"));
        }
        releaseClientSocket();
        return;
    }

    emit filesReceived(nFiles);
    m_uploadInProgress = false;
    m_uploadProgress = 1.0;
    m_uploadStatusText = tr("Загрузка завершена");
    emit uploadProgressChanged();
    sendSimpleHtml(m_client, 200, QStringLiteral("Готово"),
                   QStringLiteral("<p>Успешно загружено файлов: %1</p><p><a href=\"/\">Загрузить ещё</a></p>").arg(nFiles));
    releaseClientSocket();
}

void HttpUploadController::onClientReadyRead()
{
    QTcpSocket *const socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || socket != m_client) {
        return;
    }

    if (!m_client) {
        return;
    }
    m_rxBuffer.append(m_client->readAll());
    tryProcessBuffer();
}

bool HttpUploadController::applyMainVersion(const QString &version)
{
    const QString v = version.trimmed();
    if (!kSimpleVersionRe.match(v).hasMatch()) {
        setLastError(tr("Некорректная версия интерфейса"));
        return false;
    }

    const QString srcPath = AppPaths::instance().releasesMainBinaryPath(v);
    const QString dstPath = QStringLiteral("/usr/share/qtpr/UserInterface");
    const QString bakPath = QStringLiteral("/usr/share/qtpr/UserInterface.bak");

    if (QFile::exists(dstPath)) {
        if (QFile::exists(bakPath) && !QFile::remove(bakPath)) {
            setLastError(tr("Не удалось удалить старый backup UserInterface.bak"));
            return false;
        }
        if (!QFile::copy(dstPath, bakPath)) {
            setLastError(tr("Не удалось создать backup UserInterface.bak"));
            return false;
        }
    }

    QString copyErr;
    if (!copyFileReplace(srcPath, dstPath, &copyErr)) {
        setLastError(tr("Не удалось обновить модуль интерфейса (%1)").arg(copyErr));
        return false;
    }

    const QFileDevice::Permissions execPerms =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::ExeOther;
    if (!QFile::setPermissions(dstPath, execPerms) || !QFileInfo(dstPath).isExecutable()) {
        setLastError(tr("Не удалось выставить права на запуск для UserInterface"));
        return false;
    }

    if (m_json) {
        m_json->saveString(QStringLiteral("currentBinaryVersion"), v);
    }
    setLastError(QString());
    return true;
}

bool HttpUploadController::applyMediaVersion(const QString &version)
{
    const QString v = version.trimmed();
    if (!kSimpleVersionRe.match(v).hasMatch()) {
        setLastError(tr("Некорректная версия медиафайлов"));
        return false;
    }

    const QString srcDir = AppPaths::instance().releasesMediaVersionDir(v);
    const QString dstDir = AppPaths::instance().fotekRoot();
    QString copyErr;
    if (!copyDirectoryContentsReplace(srcDir, dstDir, &copyErr)) {
        setLastError(tr("Не удалось обновить медиафайлы"));
        return false;
    }

    if (m_json) {
        m_json->saveString(QStringLiteral("currentMediaVersion"), v);
    }
    setCurrentMediaVersion(v);
    setLastError(QString());
    return true;
}

void HttpUploadController::setMcFirmwareUpdateProgress(int progress)
{
    if (m_mcFirmwareUpdateProgress == progress) {
        return;
    }
    m_mcFirmwareUpdateProgress = progress;
    emit mcFirmwareUpdateProgressChanged();
}

void HttpUploadController::onMcFirmwareParseError(const QString &message)
{
    setMcFirmwareUpdateProgress(-1);
    if (!message.isEmpty()) {
        setLastError(message);
    }
}

void HttpUploadController::abortMcFirmwareUpdate()
{
    if (m_mcFirmwareUpdateProgress < 0) {
        return;
    }
    if (!m_linkStm) {
        setMcFirmwareUpdateProgress(-1);
        return;
    }
    LinkStm *const link = m_linkStm;
    const bool invoked = QMetaObject::invokeMethod(link, [link]() {
        link->abortFirmwareUpdate(QString());
    }, Qt::QueuedConnection);
    if (!invoked) {
        setMcFirmwareUpdateProgress(-1);
        return;
    }
    setMcFirmwareUpdateProgress(-1);
}

bool HttpUploadController::applyMcFirmwareFromReleases(const QString &version, const QString &releasesSubdir,
                                                       const QString &filePrefixUpper, int mcUnitRaw)
{
    if (!m_linkStm) {
        setMcFirmwareUpdateProgress(-1);
        setLastError(tr("Обновление МК недоступно"));
        return false;
    }
    const QString v = version.trimmed();
    if (v.isEmpty() || v == QStringLiteral("—")) {
        setMcFirmwareUpdateProgress(-1);
        setLastError(tr("Не выбрана версия"));
        return false;
    }
    const QString path = AppPaths::instance().releasesFirmwareHexPath(releasesSubdir, filePrefixUpper, v);
    if (!QFileInfo::exists(path)) {
        setMcFirmwareUpdateProgress(-1);
        setLastError(tr("Файл не найден: %1").arg(path));
        return false;
    }
    LinkStm *const link = m_linkStm;
    const bool invoked = QMetaObject::invokeMethod(link, [link, path, v, mcUnitRaw]() {
        link->startFirmwareUpdateFromFile(path, v, mcUnitRaw);
    }, Qt::QueuedConnection);
    if (!invoked) {
        setMcFirmwareUpdateProgress(-1);
        setLastError(tr("Не удалось поставить обновление в очередь"));
        return false;
    }
    setMcFirmwareUpdateProgress(0);
    setLastError(QString());
    return true;
}

bool HttpUploadController::applyComVersion(const QString &version)
{
    return applyMcFirmwareFromReleases(version, QStringLiteral("com"), QStringLiteral("COM"),
                                       static_cast<int>(LinkStm::MC_COM));
}

bool HttpUploadController::applyArgVersion(const QString &version)
{
    return applyMcFirmwareFromReleases(version, QStringLiteral("arg"), QStringLiteral("ARG"),
                                       static_cast<int>(LinkStm::MC_ARG));
}

bool HttpUploadController::applyGenVersion(const QString &version)
{
    return applyMcFirmwareFromReleases(version, QStringLiteral("gen"), QStringLiteral("GEN"),
                                       static_cast<int>(LinkStm::MC_GEN));
}

bool HttpUploadController::restartDemo1UserService()
{
    setLastError(tr("Команда перезагрузки после обновления"));
    QCoreApplication::exit(0);
    return true;
}
