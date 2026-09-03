#include "HttpUploadController.h"
#include "jsonstorage.h"
#include "linkstm.h"
#include "apppaths.h"
#include "userprogtransfercontroller.h"

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
#include <QHash>
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
#include <QVariant>
#include <QMetaObject>
#include <QPointer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QTimer>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

const char kDefaultUploadDir[] = "/var/lib/qtpr/incoming";
const char kReleaseZipPassword[] = "Electrosurgical";
const char kLogArchiveZipPassword[] = "FOTEK-EKB";
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

QString decodeHttpRequestPath(const QByteArray &rawTarget)
{
    const int q = rawTarget.indexOf('?');
    const QByteArray pathBytes = q >= 0 ? rawTarget.left(q) : rawTarget;
    return QUrl::fromPercentEncoding(pathBytes);
}

QString logArchiveCacheFilePath(const QString &sessionToken)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    return QDir(base).filePath(QStringLiteral("onyxlog-%1.zip").arg(sessionToken));
}

QString userProgCacheFilePath(const QString &sessionToken)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    return QDir(base).filePath(QStringLiteral("onyx-userprogs-%1.db").arg(sessionToken));
}

bool isUserProgFileName(const QString &name)
{
    return name.endsWith(QStringLiteral(".db"), Qt::CaseInsensitive)
            || name.endsWith(QStringLiteral(".sqlite"), Qt::CaseInsensitive)
            || name.endsWith(QStringLiteral(".sqlite3"), Qt::CaseInsensitive);
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
    Q_UNUSED(timeoutMs)
    if (!socket || data.isEmpty()) {
        return true;
    }
    if (socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "HttpUploadController: writeSocketAll socket not connected, state="
                   << socket->state() << "size=" << data.size();
        return false;
    }
    const qint64 written = socket->write(data);
    if (written != data.size()) {
        qWarning() << "HttpUploadController: writeSocketAll short write" << written
                   << "of" << data.size() << "error=" << socket->errorString();
        return false;
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

void HttpUploadController::loadFavicon()
{
    const QString path = AppPaths::instance().iconsDir()
            + QStringLiteral("/favicon.ico");
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        m_faviconData = f.readAll();
        qWarning() << "HttpUploadController: loaded favicon" << path
                   << "size=" << m_faviconData.size();
    } else {
        m_faviconData.clear();
    }
}

void HttpUploadController::setJsonStorage(JsonStorage *storage)
{
    m_json = storage;
    loadNetworkSettings();
    loadFavicon();
    if (m_json) {
        setCurrentMediaVersion(m_json->readString(QStringLiteral("currentMediaVersion"), QStringLiteral("—")));
    } else {
        setCurrentMediaVersion(QStringLiteral("—"));
    }
    refreshReleaseVersions();
    QTimer::singleShot(0, this, [this]() { applyIdleWifiRadio(); });
}

void HttpUploadController::setUserProgTransfer(UserProgTransferController *transfer)
{
    m_userProgTransfer = transfer;
}

bool HttpUploadController::isLogDownloadMode() const
{
    return m_sessionKind == SessionKind::LogDownload;
}

bool HttpUploadController::isUserProgDownloadMode() const
{
    return m_sessionKind == SessionKind::UserProgDownload;
}

bool HttpUploadController::isUserProgUploadMode() const
{
    return m_sessionKind == SessionKind::UserProgUpload;
}

bool HttpUploadController::isPreparedDownloadMode() const
{
    return m_sessionKind == SessionKind::LogDownload
            || m_sessionKind == SessionKind::UserProgDownload;
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

bool HttpUploadController::wifiAlwaysEnabled() const
{
    return m_json && parseBoolString(m_json->readString(QStringLiteral("wifiAlwaysEnabled"),
                                                        QStringLiteral("0")));
}

bool HttpUploadController::setWifiRadioEnabled(bool enabled, QString *errorText)
{
    QString stderrText;
    if (runNmcli({QStringLiteral("radio"), QStringLiteral("wifi"),
                  enabled ? QStringLiteral("on") : QStringLiteral("off")},
                 5000, nullptr, &stderrText)) {
        qInfo("Wi-Fi radio %s", enabled ? "on" : "off");
        return true;
    }

    if (errorText) {
        *errorText = stderrText.isEmpty()
                ? (enabled ? tr("Не удалось включить Wi-Fi.")
                           : tr("Не удалось выключить Wi-Fi."))
                : (enabled ? tr("Не удалось включить Wi-Fi: %1").arg(stderrText)
                           : tr("Не удалось выключить Wi-Fi: %1").arg(stderrText));
    }
    return false;
}

void HttpUploadController::applyIdleWifiRadio()
{
    if (m_active) {
        return;
    }
    QString error;
    if (!setWifiRadioEnabled(wifiAlwaysEnabled(), &error) && !error.isEmpty()) {
        qWarning() << "HttpUploadController: idle Wi-Fi radio:" << error;
    }
}

bool HttpUploadController::ensureWifiReadyForSession(QString *errorText)
{
    return setWifiRadioEnabled(true, errorText);
}

void HttpUploadController::cleanupWifiAfterSession()
{
    if (wifiAlwaysEnabled()) {
        return;
    }

    QString error;
    if (!setWifiRadioEnabled(false, &error) && !error.isEmpty()) {
        qWarning() << "HttpUploadController: failed to disable Wi-Fi after session:" << error;
    }
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
        m_logDownloadUrl = m_baseUrl + sessionPreparedDownloadFetchPath()
                + QStringLiteral("?token=") + m_sessionToken;
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
        *errorHtml = QStringLiteral("<p>%1</p>").arg(pageText("archive.noLogDir").arg(onyxLogDir.toHtmlEscaped()));
        return false;
    }

    const QString zipProgram = resolveZipProgramPath();
    if (zipProgram.isEmpty()) {
        qWarning() << "HttpUploadController: buildLogArchiveBundle zip not found in PATH";
        *errorHtml = htmlP("archive.noZip");
        return false;
    }

    QTemporaryDir bundleDir;
    if (!bundleDir.isValid()) {
        *errorHtml = htmlP("archive.noTempDir");
        return false;
    }

    const QString stageRoot = QDir(bundleDir.path()).filePath(QStringLiteral("stage"));
    const QString stageOnyxLog = QDir(stageRoot).filePath(QStringLiteral("OnyxLog"));
    if (!copyDirectoryRecursive(onyxLogDir, stageOnyxLog)) {
        *errorHtml = htmlP("archive.copyOnyxLog");
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
         << QStringLiteral("-P") << QString::fromUtf8(kLogArchiveZipPassword)
         << zipPath
         << relEntries;

    zipProc.start(zipProgram, args);
    if (!zipProc.waitForStarted(3000)) {
        *errorHtml = htmlP("archive.zipStartFail");
        return false;
    }
    if (!zipProc.waitForFinished(300000)) {
        zipProc.kill();
        zipProc.waitForFinished(1000);
        *errorHtml = htmlP("archive.zipTimeout");
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
                ? QStringLiteral("<p>%1</p>").arg(pageText("archive.zipFail").arg(zipExit).toHtmlEscaped())
                : QStringLiteral("<p>%1</p>").arg(pageText("archive.zipFailDetails")
                      .arg(zipExit)
                      .arg(details)
                      .toHtmlEscaped());
        return false;
    }

    const QString cachePath = logArchiveCacheFilePath(sessionToken);
    if (QFile::exists(cachePath) && !QFile::remove(cachePath)) {
        *errorHtml = htmlP("archive.prepareFile");
        return false;
    }
    if (!QFile::copy(zipPath, cachePath)) {
        *errorHtml = htmlP("archive.saveFail");
        return false;
    }
    const qint64 size = QFileInfo(cachePath).size();
    if (size <= 0) {
        qWarning() << "HttpUploadController: buildLogArchiveBundle result empty";
        QFile::remove(cachePath);
        *errorHtml = htmlP("archive.empty");
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
    emit logArchiveStateChanged();
}

bool HttpUploadController::logArchiveReady() const
{
    QMutexLocker locker(&m_logArchiveMutex);
    return m_logArchiveState == LogArchiveState::Ready;
}

bool HttpUploadController::logArchiveBuilding() const
{
    QMutexLocker locker(&m_logArchiveMutex);
    return m_logArchiveState == LogArchiveState::Building;
}

QString HttpUploadController::logArchiveError() const
{
    QMutexLocker locker(&m_logArchiveMutex);
    if (m_logArchiveState != LogArchiveState::Error) {
        return QString();
    }
    QString text = m_logArchiveErrorText;
    text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
    return text.trimmed();
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
    emit logArchiveStateChanged();

    struct LogArchiveBuildResult {
        QString filePath;
        qint64 fileSize = 0;
        QString errorHtml;
        bool ok = false;
    };

    const QString buildToken = m_sessionToken;

    auto *watcher = new QFutureWatcher<LogArchiveBuildResult>(this);
    connect(watcher, &QFutureWatcher<LogArchiveBuildResult>::finished, this,
            [this, watcher, buildToken]() {
        const LogArchiveBuildResult result = watcher->result();
        watcher->deleteLater();

        if (buildToken != m_sessionToken || !isPreparedDownloadMode()) {
            if (!result.filePath.isEmpty()) {
                QFile::remove(result.filePath);
            }
            return;
        }

        {
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
                        ? htmlP("archive.createFail")
                        : result.errorHtml;
                const QString plain = QString(m_logArchiveErrorText)
                        .remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                qWarning() << "HttpUploadController: log archive build failed:" << plain;
            }
        }
        emit logArchiveStateChanged();
    });

    const SessionKind kind = m_sessionKind;
    const QList<int> scopeIds = m_userProgExportScopeIds;

    watcher->setFuture(QtConcurrent::run([this, buildToken, kind, scopeIds]() {
        LogArchiveBuildResult result;
        if (kind == SessionKind::UserProgDownload) {
            const QString cachePath = userProgCacheFilePath(buildToken);
            QFile::remove(cachePath);
            result.ok = UserProgTransferController::exportScopesToFile(
                        scopeIds, cachePath, &result.errorHtml);
            if (result.ok) {
                result.filePath = cachePath;
                result.fileSize = QFileInfo(cachePath).size();
                if (result.fileSize <= 0) {
                    result.ok = false;
                    result.errorHtml = htmlP("archive.userprogEmpty");
                    QFile::remove(cachePath);
                }
            }
            return result;
        }
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
        sendFileDownloadFromPath(socket, filePath);
        return true;
    }

    qWarning() << "HttpUploadController: servePreparedLogArchive sending" << body.size()
               << "bytes from memory, peer=" << (socket ? socket->peerAddress().toString() : QString())
               << "cache=" << cacheState;
    sendFileDownloadResponse(socket, QByteArrayLiteral("onyxlog.zip"), body);
    return true;
}

QString HttpUploadController::logArchiveDownloadFileName() const
{
    QString serial;
    if (m_json) {
        serial = m_json->readString(QStringLiteral("serialNumber"), QString()).trimmed();
    }
    serial.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    serial.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    if (serial.isEmpty()) {
        serial = QStringLiteral("unknown");
    }
    return QStringLiteral("onyx-%1-log-%2-%3.zip")
            .arg(deviceTypeFileTag(), serial, QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
}

QString HttpUploadController::sessionDownloadFileName() const
{
    if (isUserProgDownloadMode()) {
        if (!m_userProgDownloadFileName.isEmpty()) {
            return m_userProgDownloadFileName;
        }
        return buildUserProgExportFileName(QString());
    }
    return logArchiveDownloadFileName();
}

QString HttpUploadController::sessionPreparedDownloadFetchPath() const
{
    if (isUserProgDownloadMode()) {
        return QStringLiteral("download/userprogs.db");
    }
    return QStringLiteral("download/") + logArchiveDownloadFileName();
}

QString HttpUploadController::buildUserProgExportFileName(const QString &namePrefix) const
{
    const QString prefix = sanitizeUserProgNamePrefix(namePrefix);
    const QString suffix = userProgExportSuffix();
    if (prefix.isEmpty()) {
        return suffix;
    }
    return prefix + suffix;
}

QString HttpUploadController::sanitizeUserProgNamePrefix(const QString &raw) const
{
    QString s = raw.trimmed();
    s.replace(QRegularExpression(QStringLiteral(R"([\s\\/:*?"<>|])")), QStringLiteral("_"));
    s.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    s.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    return s;
}

QString HttpUploadController::userProgExportSuffix() const
{
    return deviceTypeFileTag() == QLatin1String("m")
            ? QStringLiteral("_onyx-m.db")
            : QStringLiteral("_onyx-am.db");
}

QByteArray HttpUploadController::sessionDownloadContentType() const
{
    return isUserProgDownloadMode()
            ? QByteArrayLiteral("application/octet-stream")
            : QByteArrayLiteral("application/zip");
}

void HttpUploadController::scheduleAccessPointShutdownAfterUserProgUpload()
{
    setUserProgImportCompleted(true);
    const quint64 generation = m_sessionGeneration;
    QTimer::singleShot(1500, this, [this, generation]() {
        if (generation != m_sessionGeneration || !m_userProgImportCompleted) {
            return;
        }
        stopSession();
    });
}

static QByteArray logArchiveContentDisposition(const QString &fileName)
{
    QString asciiFallback = fileName;
    asciiFallback.replace(QRegularExpression(QStringLiteral(R"([^\x20-\x7E])")), QStringLiteral("_"));
    return QByteArray("Content-Disposition: attachment; filename=\"")
            + asciiFallback.toLatin1()
            + "\"; filename*=UTF-8''"
            + QUrl::toPercentEncoding(fileName)
            + "\r\n";
}

bool HttpUploadController::isValidTokenInPath(const QString &path) const
{
    return requestQueryValue(path, QStringLiteral("token")) == m_sessionToken;
}

void HttpUploadController::bindAuthorizedClient(const QHostAddress &peer, bool allowRebind)
{
    const QHostAddress normalized = normalizeClientAddress(peer);
    if (normalized.isNull()) {
        return;
    }
    if (!m_authorizedClientAddress.isNull()) {
        if (m_authorizedClientAddress == normalized) {
            return;
        }
        if (!allowRebind) {
            return;
        }
        qWarning() << "HttpUploadController: rebinding session from"
                   << m_authorizedClientAddress.toString() << "to" << normalized.toString();
    }
    m_authorizedClientAddress = normalized;
    qWarning() << "HttpUploadController: session bound to client" << normalized.toString();
}

bool HttpUploadController::ensureDownloadClientAccess(const QHostAddress &peer, bool allowRebind)
{
    if (isAuthorizedClient(peer)) {
        return true;
    }
    if (!allowRebind) {
        return false;
    }
    bindAuthorizedClient(peer, true);
    return isAuthorizedClient(peer);
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
            m_qrStatusText = isPreparedDownloadMode()
                    ? tr("Адрес страницы ещё не готов.")
                    : tr("Адрес загрузки ещё не готов.");
        } else {
            payload = m_baseUrl;
            fileName = QStringLiteral("upload_qr.png");
            m_qrStatusText = isPreparedDownloadMode()
                    ? tr("Устройство подключено. Отсканируйте QR для открытия страницы скачивания.")
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

    const AccessPointSetupResult result = runAccessPointSetup(
            m_apInterfaceName, m_apSsid, m_apConnectionName, m_apConfiguredAddress, m_apPassword);
    if (!result.ok) {
        if (errorText) {
            *errorText = result.errorText;
        }
        setAccessPointStatusText(QString());
        m_apPassword.clear();
        m_apInterfaceName.clear();
        emit accessPointChanged();
        return false;
    }

    m_apInterfaceName = result.interfaceName;
    m_apAddress = result.address;
    m_apActive = true;
    emit accessPointChanged();
    setAccessPointStatusText(tr("Точка доступа %1 активна. Ожидание подключения клиента.")
                             .arg(m_apSsid));
    m_apClientPollTimer.start();
    return true;
}

HttpUploadController::AccessPointSetupResult HttpUploadController::runAccessPointSetup(
        const QString &interfaceName,
        const QString &ssid,
        const QString &connectionName,
        const QString &configuredAddress,
        const QString &password) const
{
    AccessPointSetupResult result;
    result.interfaceName = interfaceName;
    if (result.interfaceName.isEmpty()) {
        result.errorText = tr("Wi-Fi интерфейс не найден.");
        return result;
    }

    QString stderrText;
    runNmcli({QStringLiteral("connection"), QStringLiteral("down"), connectionName},
             5000, nullptr, nullptr);
    runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), connectionName},
             5000, nullptr, nullptr);

    const QStringList addArgs = {
        QStringLiteral("connection"), QStringLiteral("add"),
        QStringLiteral("type"), QStringLiteral("wifi"),
        QStringLiteral("ifname"), result.interfaceName,
        QStringLiteral("con-name"), connectionName,
        QStringLiteral("autoconnect"), QStringLiteral("no"),
        QStringLiteral("ssid"), ssid
    };
    if (!runNmcli(addArgs, 10000, nullptr, &stderrText)) {
        result.errorText = stderrText.isEmpty()
                ? tr("Не удалось создать профиль точки доступа.")
                : tr("Не удалось создать профиль точки доступа: %1").arg(stderrText);
        return result;
    }
    result.profileCreated = true;

    const QStringList modifyArgs = {
        QStringLiteral("connection"), QStringLiteral("modify"), connectionName,
        QStringLiteral("802-11-wireless.mode"), QStringLiteral("ap"),
        QStringLiteral("802-11-wireless.band"), QStringLiteral("bg"),
        QStringLiteral("ipv4.method"), QStringLiteral("shared"),
        QStringLiteral("ipv4.addresses"), configuredAddress,
        QStringLiteral("ipv6.method"), QStringLiteral("ignore"),
        QStringLiteral("wifi-sec.key-mgmt"), QStringLiteral("wpa-psk"),
        QStringLiteral("wifi-sec.psk"), password
    };
    if (!runNmcli(modifyArgs, 10000, nullptr, &stderrText)) {
        runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), connectionName},
                 5000, nullptr, nullptr);
        result.profileCreated = false;
        result.errorText = stderrText.isEmpty()
                ? tr("Не удалось настроить точку доступа.")
                : tr("Не удалось настроить точку доступа: %1").arg(stderrText);
        return result;
    }

    if (!runNmcli({QStringLiteral("connection"), QStringLiteral("up"), connectionName},
                  20000, nullptr, &stderrText)) {
        runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), connectionName},
                 5000, nullptr, nullptr);
        result.profileCreated = false;
        result.errorText = stderrText.isEmpty()
                ? tr("Не удалось запустить точку доступа.")
                : tr("Не удалось запустить точку доступа: %1").arg(stderrText);
        return result;
    }

    for (int i = 0; i < 10; ++i) {
        result.address = addressForInterface(result.interfaceName);
        if (!result.address.isNull()) {
            break;
        }
        QThread::msleep(300);
    }

    if (result.address.isNull()) {
        const int slash = configuredAddress.indexOf(QLatin1Char('/'));
        result.address = QHostAddress(slash > 0 ? configuredAddress.left(slash) : configuredAddress);
    }

    result.ok = true;
    return result;
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

void HttpUploadController::setSessionKind(SessionKind kind)
{
    if (m_sessionKind == kind) {
        return;
    }
    m_sessionKind = kind;
    emit sessionModeChanged();
}

void HttpUploadController::setUserProgImportCompleted(bool completed)
{
    if (m_userProgImportCompleted == completed) {
        if (!completed) {
            m_userProgImportSummary.clear();
            m_userProgImportError.clear();
            m_userProgImportedPrograms = 0;
            m_userProgImportedNewFolders = 0;
            m_userProgImportedExistingFolders = 0;
        }
        return;
    }
    m_userProgImportCompleted = completed;
    if (!completed) {
        m_userProgImportSummary.clear();
        m_userProgImportError.clear();
        m_userProgImportedPrograms = 0;
        m_userProgImportedNewFolders = 0;
        m_userProgImportedExistingFolders = 0;
    }
    emit userProgImportCompletedChanged();
}

void HttpUploadController::setLogDownloadCompleted(bool completed)
{
    if (m_logDownloadCompleted == completed) {
        return;
    }
    m_logDownloadCompleted = completed;
    emit logDownloadCompletedChanged();
}

void HttpUploadController::scheduleAccessPointShutdownAfterLogDownload()
{
    setLogDownloadCompleted(true);
    const quint64 generation = m_sessionGeneration;
    QTimer::singleShot(1500, this, [this, generation]() {
        if (generation != m_sessionGeneration || !m_logDownloadCompleted) {
            return;
        }
        stopSession();
    });
}

void HttpUploadController::fillDeviceIdentityHtml(QString *serialHtml, QString *typeHtml) const
{
    if (!serialHtml || !typeHtml) {
        return;
    }
    *serialHtml = QStringLiteral("—");
    *typeHtml = QStringLiteral("—");
    if (!m_json) {
        return;
    }
    const QString s = m_json->readString(QStringLiteral("serialNumber"), QString()).trimmed();
    if (!s.isEmpty()) {
        *serialHtml = s.toHtmlEscaped();
    }
    const QString t = m_json->readString(QStringLiteral("deviceType"), QString()).trimmed();
    if (!t.isEmpty()) {
        *typeHtml = t.toHtmlEscaped();
    }
}

QString HttpUploadController::uiLanguage() const
{
    const QString lang = m_json
            ? m_json->readString(QStringLiteral("language"), QStringLiteral("ru")).trimmed().toLower()
            : QStringLiteral("ru");
    if (lang == QLatin1String("en") || lang == QLatin1String("es")) {
        return lang;
    }
    return QStringLiteral("ru");
}

QString HttpUploadController::deviceTypeFileTag() const
{
    const QString type = m_json
            ? m_json->readString(QStringLiteral("deviceType"), QStringLiteral("ONYX-AM")).trimmed().toUpper()
            : QStringLiteral("ONYX-AM");
    return type == QLatin1String("ONYX-M") ? QStringLiteral("m") : QStringLiteral("am");
}

QString HttpUploadController::pageText(const char *key) const
{
    const QString lang = uiLanguage();
    const int col = (lang == QLatin1String("en")) ? 1 : ((lang == QLatin1String("es")) ? 2 : 0);
    static const struct {
        const char *id;
        const char *t[3];
    } rows[] = {
        {"upload.title", {"Загрузка файлов", "File upload", "Carga de archivos"}},
        {"upload.notice", {"<strong>Внимание:</strong> при повторном запуске приёма обновите страницу.",
                           "<strong>Attention:</strong> if you restart the transfer, refresh this page.",
                           "<strong>Atención:</strong> si vuelve a iniciar la recepción, actualice la página."}},
        {"upload.hint", {"Выберите файл обновления в формате <b>имя-a.b-c.d-e.zip</b><br>Например: <b>onyx-5.6-3.4-1.zip</b>",
                         "Select an update file named <b>name-a.b-c.d-e.zip</b><br>Example: <b>onyx-5.6-3.4-1.zip</b>",
                         "Seleccione un archivo de actualización con el formato <b>nombre-a.b-c.d-e.zip</b><br>Ejemplo: <b>onyx-5.6-3.4-1.zip</b>"}},
        {"upload.pick", {"Выбрать файл", "Choose file", "Elegir archivo"}},
        {"upload.none", {"Файл не выбран", "No file selected", "Archivo no seleccionado"}},
        {"upload.send", {"Отправить", "Send", "Enviar"}},
        {"upload.sending", {"Отправка: ", "Sending: ", "Enviando: "}},
        {"upload.done", {"Отправка завершена", "Upload complete", "Envío completado"}},
        {"upload.sendError", {"Ошибка отправки", "Upload error", "Error de envío"}},
        {"upload.netError", {"Ошибка сети при отправке", "Network error while sending", "Error de red al enviar"}},
        {"upload.badName", {"Имя файла должно быть в формате name-a.b-c.d-e.zip",
                            "The file name must be in the format name-a.b-c.d-e.zip",
                            "El nombre del archivo debe tener el formato name-a.b-c.d-e.zip"}},
        {"upload.success", {"Файл успешно загружен", "File uploaded successfully", "Archivo cargado correctamente"}},
        {"upload.readyTitle", {"Готово", "Done", "Listo"}},
        {"log.title", {"Скачать лог-файл", "Download log file", "Descargar archivo de registro"}},
        {"log.notice", {"<strong>Внимание:</strong> при повторном запуске передачи обновите страницу.",
                        "<strong>Attention:</strong> if you restart the transfer, refresh this page.",
                        "<strong>Atención:</strong> si vuelve a iniciar la transferencia, actualice la página."}},
        {"log.hint", {"Нажмите кнопку, чтобы скачать архив журнала событий аппарата (лог-файл) на это устройство.",
                      "Tap the button to download the device event log archive (log file) to this device.",
                      "Pulse el botón para descargar el archivo del registro de eventos del aparato (archivo log) en este dispositivo."}},
        {"log.browser", {"Браузер может предупредить, что файл скачивается небезопасно — это из‑за локальной сети без HTTPS, не ошибка. Файл обычно всё равно появляется в «Загрузках».",
                         "The browser may warn that the file is not downloaded securely — this is due to the local network without HTTPS, not an error. The file usually still appears in Downloads.",
                         "El navegador puede avisar de que el archivo no se descarga de forma segura: se debe a la red local sin HTTPS, no es un error. El archivo suele aparecer igualmente en Descargas."}},
        {"log.button", {"Скачать лог-файл", "Download log file", "Descargar archivo de registro"}},
        {"common.serial", {"Серийный номер", "Serial number", "Número de serie"}},
        {"common.type", {"Тип аппарата", "Device type", "Tipo de aparato"}},
        {"units.mb", {"МБ", "MB", "MB"}},
        {"units.kb", {"КБ", "KB", "KB"}},
        {"log.js.transferring", {"Передача архива…", "Transferring archive…", "Transfiriendo archivo…"}},
        {"log.js.transfer", {"Передача", "Transfer", "Transferencia"}},
        {"log.js.of", {"из", "of", "de"}},
        {"log.js.transferError", {"Ошибка передачи", "Transfer error", "Error de transferencia"}},
        {"log.js.empty", {"Пустой ответ сервера", "Empty server response", "Respuesta vacía del servidor"}},
        {"log.js.savedAs", {"Файл сохранён как", "File saved as", "Archivo guardado como"}},
        {"log.js.checkDownloads", {"Проверьте «Загрузки».", "Check Downloads.", "Compruebe Descargas."}},
        {"log.js.done", {"Файл сохранён на это устройство.\nТочка доступа ONYX-SERVICE автоматически отключилась.\nПодключитесь к сети Интернет и перешлите лог-файл в сервисную службу ООО «ФОТЕК».\nСпасибо!",
                        "The file has been saved on this device.\nThe ONYX-SERVICE access point has been turned off.\nConnect to the Internet and forward the log file to the FOTEK LLC service department.\nThank you!",
                        "El archivo se ha guardado en este dispositivo.\nEl punto de acceso ONYX-SERVICE se ha desconectado.\nConéctese a Internet y reenvíe el archivo de registro al servicio técnico de FOTEK.\n¡Gracias!"}},
        {"log.js.sessionExpired", {"Сессия устарела. Обновите страницу.", "The session has expired. Refresh the page.", "La sesión ha caducado. Actualice la página."}},
        {"log.js.prepError", {"Ошибка подготовки архива", "Archive preparation error", "Error al preparar el archivo"}},
        {"log.js.serverError", {"Ошибка ответа сервера", "Server response error", "Error de respuesta del servidor"}},
        {"log.js.preparing", {"Подготовка архива…", "Preparing archive…", "Preparando archivo…"}},
        {"log.js.ready", {"Архив готов. Запуск передачи…", "Archive ready. Starting transfer…", "Archivo listo. Iniciando transferencia…"}},
        {"log.js.unknown", {"Неизвестный статус", "Unknown status", "Estado desconocido"}},
        {"log.js.noSession", {"Сессия недоступна", "Session unavailable", "Sesión no disponible"}},
        {"log.js.preparingWait", {"Подготовка архива, подождите…", "Preparing archive, please wait…", "Preparando archivo, espere…"}},
        {"log.js.netError", {"Ошибка сети при передаче", "Network error during transfer", "Error de red al transferir"}},
        {"log.js.timeout", {"Таймаут при передаче", "Transfer timed out", "Tiempo de espera agotado"}},
        {"log.js.netPrep", {"Ошибка сети при подготовке", "Network error while preparing", "Error de red al preparar"}},
        {"log.js.timeoutPrep", {"Таймаут при подготовке", "Preparation timed out", "Tiempo de espera al preparar"}},
        {"userprog.dl.title", {"Скачать программы пользователя", "Download user programs", "Descargar programas de usuario"}},
        {"userprog.dl.hint", {"Нажмите кнопку, чтобы скачать выбранные папки программ пользователя на это устройство.",
                              "Tap the button to download the selected user program folders to this device.",
                              "Pulse el botón para descargar las carpetas de programas de usuario seleccionadas en este dispositivo."}},
        {"userprog.dl.button", {"Скачать программы", "Download programs", "Descargar programas"}},
        {"userprog.dl.fileName", {"Имя файла", "File name", "Nombre de archivo"}},
        {"userprog.dl.done", {"Файл программ сохранён на это устройство.\nТочка доступа ONYX-SERVICE автоматически отключилась.",
                             "The programs file has been saved on this device.\nThe ONYX-SERVICE access point has been turned off.",
                             "El archivo de programas se ha guardado en este dispositivo.\nEl punto de acceso ONYX-SERVICE se ha desconectado."}},
        {"userprog.ul.title", {"Загрузка программ пользователя", "Upload user programs", "Carga de programas de usuario"}},
        {"userprog.ul.hint", {"Выберите файл программ пользователя (например, xxx_onyx-m.db), ранее скачанный с аппарата ONYX. По умолчанию он загружается в папку \"Загрузки\", можно использовать поиск по слову \"onyx\".",
                              "Select a user programs file (for example, xxx_onyx-m.db) previously downloaded from an ONYX unit. By default it is saved in Downloads; you can search for \"onyx\".",
                              "Seleccione un archivo de programas de usuario (por ejemplo, xxx_onyx-m.db) descargado previamente de un aparato ONYX. Por defecto se guarda en Descargas; puede buscar \"onyx\"."}},
        {"userprog.ul.badName", {"Выберите файл программ с расширением .db или .sqlite",
                                 "Select a programs file with a .db or .sqlite extension",
                                 "Seleccione un archivo de programas con extensión .db o .sqlite"}},
        {"userprog.ul.success", {"Программы успешно загружены на аппарат",
                                 "Programs were imported to the device",
                                 "Los programas se importaron al aparato"}},
        {"http.title.error", {"Ошибка", "Error", "Error"}},
        {"http.title.notFound", {"Не найдено", "Not found", "No encontrado"}},
        {"http.title.forbidden", {"Доступ запрещён", "Access denied", "Acceso denegado"}},
        {"http.title.tooLarge", {"Слишком большой", "Too large", "Demasiado grande"}},
        {"http.headersTooLong", {"Слишком длинные заголовки", "Headers too long", "Cabeceras demasiado largas"}},
        {"http.badRequest", {"Некорректный запрос", "Invalid request", "Solicitud no válida"}},
        {"http.badRequestLine", {"Некорректная строка запроса", "Invalid request line", "Línea de solicitud no válida"}},
        {"http.pageNotFound", {"Страница не найдена", "Page not found", "Página no encontrada"}},
        {"http.methodNotSupported", {"Метод не поддерживается", "Method not supported", "Método no admitido"}},
        {"http.uploadSessionInactive", {"Сессия загрузки не активна. Откройте страницу заново с устройства.",
                                        "The upload session is not active. Open the page again from the unit.",
                                        "La sesión de carga no está activa. Abra la página de nuevo desde el aparato."}},
        {"http.uploadWrongClient", {"Загрузка доступна только с устройства, открывшего сессию.",
                                    "Upload is only available from the device that opened the session.",
                                    "La carga solo está disponible desde el dispositivo que abrió la sesión."}},
        {"http.needContentLength", {"Нужен заголовок Content-Length",
                                    "Content-Length header is required",
                                    "Se necesita la cabecera Content-Length"}},
        {"http.bodyTooLarge", {"Размер запроса превышает допустимый",
                               "The request is too large",
                               "El tamaño de la solicitud supera el límite"}},
        {"http.extraBody", {"Лишние данные в теле запроса",
                            "Unexpected extra data in the request body",
                            "Datos adicionales inesperados en el cuerpo de la solicitud"}},
        {"http.expectMultipart", {"Ожидается multipart/form-data",
                                  "multipart/form-data is required",
                                  "Se espera multipart/form-data"}},
        {"http.badToken", {"Неверный или устаревший токен. Откройте страницу снова с устройства.",
                           "Invalid or expired token. Open the page again from the unit.",
                           "Token no válido o caducado. Abra la página de nuevo desde el aparato."}},
        {"http.noFiles", {"Файлы не выбраны", "No files selected", "No se han seleccionado archivos"}},
        {"http.tooManyFiles", {"Слишком много файлов за один раз",
                               "Too many files in one request",
                               "Demasiados archivos en una sola solicitud"}},
        {"http.manifestMissing", {"В архиве не найден update-manifest.json",
                                  "update-manifest.json was not found in the archive",
                                  "No se encontró update-manifest.json en el archivo"}},
        {"http.manifestInvalid", {"update-manifest.json повреждён или не является JSON-объектом",
                                  "update-manifest.json is damaged or is not a JSON object",
                                  "update-manifest.json está dañado o no es un objeto JSON"}},
        {"http.manifestDeploypaths", {"В архиве нет файлов для раскладки (deployPaths пуст и payload пуст)",
                                      "The archive has no files to deploy (deployPaths and payload are empty)",
                                      "El archivo no contiene ficheros para desplegar (deployPaths y payload están vacíos)"}},
        {"http.manifestShaMissing", {"В update-manifest.json отсутствует поле payloadSha256",
                                     "payloadSha256 is missing from update-manifest.json",
                                     "Falta el campo payloadSha256 en update-manifest.json"}},
        {"http.manifestPath", {"Пути из update-manifest.json некорректны или отсутствуют в payload",
                               "Paths from update-manifest.json are invalid or missing from the payload",
                               "Las rutas de update-manifest.json no son válidas o faltan en el payload"}},
        {"http.checksum", {"Контрольная сумма payloadSha256 не прошла проверку",
                           "payloadSha256 checksum verification failed",
                           "La suma de comprobación payloadSha256 no es válida"}},
        {"http.unzip", {"Не удалось открыть zip-архив. Проверьте пароль/целостность архива.",
                        "Could not open the zip archive. Check the password and archive integrity.",
                        "No se pudo abrir el archivo zip. Compruebe la contraseña y la integridad del archivo."}},
        {"http.userprogImportFail", {"Не удалось импортировать программы пользователя.",
                                     "Failed to import user programs.",
                                     "No se pudieron importar los programas de usuario."}},
        {"http.parseForm", {"Не удалось разобрать данные формы",
                            "Failed to parse the form data",
                            "No se pudieron analizar los datos del formulario"}},
        {"http.sessionExpiredQr", {"Сессия устарела. Откройте страницу заново по QR.",
                                   "The session has expired. Open the page again via the QR code.",
                                   "La sesión ha caducado. Abra la página de nuevo con el código QR."}},
        {"http.downloadWrongClient", {"Скачивание доступно только с устройства, открывшего страницу.",
                                      "Download is only available from the device that opened the page.",
                                      "La descarga solo está disponible desde el dispositivo que abrió la página."}},
        {"http.confirmWrongClient", {"Подтверждение доступно только с устройства, открывшего страницу.",
                                     "Confirmation is only available from the device that opened the page.",
                                     "La confirmación solo está disponible desde el dispositivo que abrió la página."}},
        {"archive.noLogDir", {"Каталог журналов не найден: %1",
                              "Log directory not found: %1",
                              "No se encontró el directorio de registros: %1"}},
        {"archive.noZip", {"Не найдена утилита zip для сборки архива.",
                           "The zip utility was not found.",
                           "No se encontró la utilidad zip para crear el archivo."}},
        {"archive.noTempDir", {"Не удалось создать временный каталог для архива.",
                               "Failed to create a temporary directory for the archive.",
                               "No se pudo crear un directorio temporal para el archivo."}},
        {"archive.copyOnyxLog", {"Не удалось подготовить копию каталога OnyxLog.",
                                 "Failed to prepare a copy of the OnyxLog directory.",
                                 "No se pudo preparar una copia del directorio OnyxLog."}},
        {"archive.zipStartFail", {"Не удалось запустить zip.",
                                  "Failed to start zip.",
                                  "No se pudo iniciar zip."}},
        {"archive.zipTimeout", {"Превышено время ожидания при создании архива.",
                                "Timed out while creating the archive.",
                                "Se agotó el tiempo de espera al crear el archivo."}},
        {"archive.zipFail", {"Ошибка при создании архива (код %1).",
                             "Archive creation failed (code %1).",
                             "Error al crear el archivo (código %1)."}},
        {"archive.zipFailDetails", {"Ошибка при создании архива (код %1): %2",
                                    "Archive creation failed (code %1): %2",
                                    "Error al crear el archivo (código %1): %2"}},
        {"archive.prepareFile", {"Не удалось подготовить файл архива.",
                                 "Failed to prepare the archive file.",
                                 "No se pudo preparar el archivo."}},
        {"archive.saveFail", {"Не удалось сохранить архив.",
                              "Failed to save the archive.",
                              "No se pudo guardar el archivo."}},
        {"archive.empty", {"Архив пуст.", "The archive is empty.", "El archivo está vacío."}},
        {"archive.createFail", {"Не удалось создать архив.",
                                "Failed to create the archive.",
                                "No se pudo crear el archivo."}},
        {"archive.userprogEmpty", {"Файл программ пуст.",
                                   "The programs file is empty.",
                                   "El archivo de programas está vacío."}}
    };
    for (const auto &row : rows) {
        if (qstrcmp(row.id, key) == 0) {
            return QString::fromUtf8(row.t[col]);
        }
    }
    return QString::fromUtf8(key);
}

QString HttpUploadController::htmlP(const char *key) const
{
    return QStringLiteral("<p>%1</p>").arg(pageText(key).toHtmlEscaped());
}

void HttpUploadController::sendTranslatedHtml(QTcpSocket *socket, int statusCode,
                                              const char *titleKey, const char *bodyKey)
{
    sendSimpleHtml(socket, statusCode, pageText(titleKey), htmlP(bodyKey));
}

static QString applyPagePlaceholders(QString html, QHash<QString, QString> values)
{
    QString json = values.take(QStringLiteral("{{tjson}}"));
    QList<QString> keys = values.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    for (const QString &key : keys) {
        html.replace(key, values.value(key));
    }
    if (!json.isEmpty()) {
        json.replace(QLatin1Char('<'), QStringLiteral("\\u003c"));
        json.replace(QChar(0x2028), QStringLiteral("\\u2028"));
        json.replace(QChar(0x2029), QStringLiteral("\\u2029"));
        html.replace(QStringLiteral("{{tjson}}"), json.trimmed());
    }
    return html;
}

void HttpUploadController::startSession()
{
    beginSession(SessionKind::FirmwareUpload);
}

void HttpUploadController::startLogDownloadSession()
{
    setLogDownloadCompleted(false);
    beginSession(SessionKind::LogDownload);
}

void HttpUploadController::startUserProgDownloadSession(const QVariantList &scopeIds,
                                                        const QString &downloadFileName)
{
    m_userProgExportScopeIds.clear();
    for (const QVariant &v : scopeIds) {
        bool ok = false;
        const int id = v.toInt(&ok);
        if (ok && id > 1000) {
            m_userProgExportScopeIds.append(id);
        }
    }
    std::sort(m_userProgExportScopeIds.begin(), m_userProgExportScopeIds.end());
    m_userProgExportScopeIds.erase(
                std::unique(m_userProgExportScopeIds.begin(), m_userProgExportScopeIds.end()),
                m_userProgExportScopeIds.end());

    QString fileName = QFileInfo(downloadFileName.trimmed()).fileName();
    const QString suffix = userProgExportSuffix();
    if (fileName.isEmpty()) {
        fileName = buildUserProgExportFileName(QString());
    } else if (!fileName.endsWith(suffix, Qt::CaseInsensitive)) {
        fileName = buildUserProgExportFileName(fileName);
    } else {
        const int suffixLen = suffix.length();
        const QString prefix = fileName.left(fileName.length() - suffixLen);
        fileName = buildUserProgExportFileName(prefix);
    }
    if (m_userProgDownloadFileName != fileName) {
        m_userProgDownloadFileName = fileName;
        emit userProgDownloadFileNameChanged();
    }

    setLogDownloadCompleted(false);
    if (m_active && m_sessionKind == SessionKind::UserProgDownload) {
        startLogArchiveBuildIfNeeded(true);
        return;
    }
    beginSession(SessionKind::UserProgDownload);
}

void HttpUploadController::startUserProgUploadSession()
{
    setUserProgImportCompleted(false);
    beginSession(SessionKind::UserProgUpload);
}

void HttpUploadController::beginSession(SessionKind kind)
{
    setLastError(QString());
    setUserProgImportCompleted(false);
    if (m_active) {
        if (m_sessionKind == kind) {
            if (isPreparedDownloadMode() && !logArchiveReady() && !logArchiveBuilding()) {
                startLogArchiveBuildIfNeeded(true);
            }
            return;
        }
        stopSession();
    }

    setSessionKind(kind);

    loadNetworkSettings();
    QString wifiError;
    if (!ensureWifiReadyForSession(&wifiError)) {
        setSessionKind(SessionKind::FirmwareUpload);
        setLastError(wifiError);
        return;
    }
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

    if (isPreparedDownloadMode()) {
        startLogDownloadSessionInternal();
        return;
    }
    if (kind == SessionKind::UserProgUpload) {
        startDeferredAccessPointSession();
        return;
    }

    QString apError;
    if (!startAccessPoint(&apError)) {
        cleanupWifiAfterSession();
        setSessionKind(SessionKind::FirmwareUpload);
        setLastError(apError);
        return;
    }

    m_sessionToken = randomToken();
    emit sessionTokenChanged();
    resetLogArchiveCache();
    m_authorizedClientAddress = QHostAddress();

    if (!activateHttpAfterAccessPoint()) {
        return;
    }
}

void HttpUploadController::startLogDownloadSessionInternal()
{
    const quint64 generation = ++m_sessionGeneration;
    setLogDownloadCompleted(false);
    m_sessionToken = randomToken();
    emit sessionTokenChanged();
    resetLogArchiveCache();
    m_authorizedClientAddress = QHostAddress();
    startLogArchiveBuildIfNeeded(true);

    QTimer::singleShot(0, this, [this, generation]() {
        if (generation != m_sessionGeneration) {
            return;
        }
        launchAccessPointThenActivate(generation);
    });
}

void HttpUploadController::startDeferredAccessPointSession()
{
    const quint64 generation = ++m_sessionGeneration;
    m_sessionToken = randomToken();
    emit sessionTokenChanged();
    resetLogArchiveCache();
    m_authorizedClientAddress = QHostAddress();

    QTimer::singleShot(0, this, [this, generation]() {
        if (generation != m_sessionGeneration) {
            return;
        }
        launchAccessPointThenActivate(generation);
    });
}

void HttpUploadController::launchAccessPointThenActivate(quint64 generation)
{
    if (generation != m_sessionGeneration) {
        return;
    }

    if (m_apPassword.isEmpty()) {
        m_apPassword = QStringLiteral("Electrosurgical");
    }
    setAccessPointClientConnected(false);
    setAccessPointStatusText(tr("Запуск точки доступа %1...").arg(m_apSsid));
    emit accessPointChanged();

    const QString interfaceName = selectWifiInterface();
    if (interfaceName.isEmpty()) {
        cleanupWifiAfterSession();
        setSessionKind(SessionKind::FirmwareUpload);
        setLastError(tr("Wi-Fi интерфейс не найден."));
        setAccessPointStatusText(QString());
        m_apPassword.clear();
        emit accessPointChanged();
        return;
    }

    const QString ssid = m_apSsid;
    const QString connectionName = m_apConnectionName;
    const QString configuredAddress = m_apConfiguredAddress;
    const QString password = m_apPassword;

    auto *watcher = new QFutureWatcher<AccessPointSetupResult>(this);
    connect(watcher, &QFutureWatcher<AccessPointSetupResult>::finished, this,
            [this, watcher, generation, connectionName]() {
        const AccessPointSetupResult result = watcher->result();
        watcher->deleteLater();

        if (generation != m_sessionGeneration) {
            if (result.profileCreated && !m_apActive) {
                runNmcli({QStringLiteral("connection"), QStringLiteral("down"), connectionName},
                         5000, nullptr, nullptr);
                runNmcli({QStringLiteral("connection"), QStringLiteral("delete"), connectionName},
                         5000, nullptr, nullptr);
            }
            return;
        }

        if (!result.ok) {
            cleanupWifiAfterSession();
            setSessionKind(SessionKind::FirmwareUpload);
            setLastError(result.errorText);
            setAccessPointStatusText(QString());
            m_apPassword.clear();
            emit accessPointChanged();
            return;
        }

        m_apInterfaceName = result.interfaceName;
        m_apAddress = result.address;
        m_apActive = true;
        emit accessPointChanged();
        setAccessPointStatusText(tr("Точка доступа %1 активна. Ожидание подключения клиента.")
                                 .arg(m_apSsid));
        m_apClientPollTimer.start();
        activateHttpAfterAccessPoint();
    });

    watcher->setFuture(QtConcurrent::run([this, interfaceName, ssid, connectionName, configuredAddress, password]() {
        return runAccessPointSetup(interfaceName, ssid, connectionName, configuredAddress, password);
    }));
}

bool HttpUploadController::activateHttpAfterAccessPoint()
{
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
        cleanupWifiAfterSession();
        setSessionKind(SessionKind::FirmwareUpload);
        return false;
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
        cleanupWifiAfterSession();
        setLastError(fwError);
        setSessionKind(SessionKind::FirmwareUpload);
        return false;
    }

    updateBaseUrl();
    setActive(true);
    updateLogDownloadUrl();
    updateQrCode();
    m_sessionTimer.start();
    return true;
}

void HttpUploadController::stopSession()
{
    ++m_sessionGeneration;
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
    cleanupWifiAfterSession();
    updateLogDownloadUrl();
    updateQrCode();
    m_userProgDownloadFileName.clear();
    emit userProgDownloadFileNameChanged();
    setSessionKind(SessionKind::FirmwareUpload);
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
        QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
        if (sock->state() == QAbstractSocket::ConnectedState) {
            sock->disconnectFromHost();
        }
        if (sock->state() == QAbstractSocket::UnconnectedState) {
            sock->deleteLater();
        }
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
    if (!writeSocketAll(socket, hdr) || (!body.isEmpty() && !writeSocketAll(socket, body))) {
        qWarning() << "HttpUploadController: sendHttpResponse short write, status=" << statusCode
                   << "bodyBytes=" << body.size();
    }
    socket->flush();
}

bool HttpUploadController::sendFileDownloadFromPath(QTcpSocket *socket, const QString &filePath)
{
    QPointer<QTcpSocket> sock(socket);
    if (!sock || filePath.isEmpty()) {
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
    const QString downloadName = sessionDownloadFileName();
    QByteArray hdr = "HTTP/1.1 200 OK\r\n";
    hdr += "Connection: close\r\n";
    hdr += "Content-Type: ";
    hdr += sessionDownloadContentType();
    hdr += "\r\n";
    hdr += logArchiveContentDisposition(downloadName);
    hdr += "Cache-Control: no-store\r\n";
    hdr += "Content-Length: ";
    hdr += QByteArray::number(fileSize);
    hdr += "\r\n\r\n";

    const bool hdrOk = writeSocketAll(sock, hdr);
    bool bodyOk = hdrOk;
    if (bodyOk) {
        constexpr int kChunkSize = 64 * 1024;
        while (!file.atEnd()) {
            if (!sock || sock->state() != QAbstractSocket::ConnectedState) {
                bodyOk = false;
                break;
            }
            const QByteArray chunk = file.read(kChunkSize);
            if (chunk.isEmpty() && !file.atEnd()) {
                bodyOk = false;
                break;
            }
            if (!chunk.isEmpty() && !writeSocketAll(sock, chunk)) {
                bodyOk = false;
                break;
            }
        }
    }
    if (sock) {
        sock->flush();
        qWarning() << "HttpUploadController: sendFileDownloadFromPath hdrOk=" << hdrOk
                   << "bodyOk=" << bodyOk << "bytes=" << fileSize
                   << "path=" << filePath << "socketState=" << sock->state()
                   << "bytesToWrite=" << sock->bytesToWrite();
    }
    return hdrOk && bodyOk;
}

void HttpUploadController::sendFileDownloadResponse(QTcpSocket *socket, const QByteArray &downloadFileName,
                                                    const QByteArray &body)
{
    if (!socket) {
        qWarning() << "HttpUploadController: sendFileDownloadResponse null socket";
        return;
    }
    const QString downloadName = downloadFileName.isEmpty()
            ? sessionDownloadFileName()
            : QString::fromUtf8(downloadFileName);
    QByteArray hdr = "HTTP/1.1 200 OK\r\n";
    hdr += "Connection: close\r\n";
    hdr += "Content-Type: ";
    hdr += sessionDownloadContentType();
    hdr += "\r\n";
    hdr += logArchiveContentDisposition(downloadName);
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
    QString serialHtml;
    QString typeHtml;
    fillDeviceIdentityHtml(&serialHtml, &typeHtml);
    QJsonObject jsTr;
    jsTr.insert(QStringLiteral("none"), pageText("upload.none"));
    jsTr.insert(QStringLiteral("badName"), pageText(isUserProgUploadMode() ? "userprog.ul.badName" : "upload.badName"));
    jsTr.insert(QStringLiteral("sending"), pageText("upload.sending"));
    jsTr.insert(QStringLiteral("done"), pageText("upload.done"));
    jsTr.insert(QStringLiteral("sendError"), pageText("upload.sendError"));
    jsTr.insert(QStringLiteral("netError"), pageText("upload.netError"));
    QHash<QString, QString> ph;
    ph.insert(QStringLiteral("{{lang}}"), uiLanguage());
    ph.insert(QStringLiteral("{{title}}"), pageText(isUserProgUploadMode() ? "userprog.ul.title" : "upload.title").toHtmlEscaped());
    ph.insert(QStringLiteral("{{serialLabel}}"), pageText("common.serial").toHtmlEscaped());
    ph.insert(QStringLiteral("{{typeLabel}}"), pageText("common.type").toHtmlEscaped());
    ph.insert(QStringLiteral("{{serial}}"), serialHtml);
    ph.insert(QStringLiteral("{{type}}"), typeHtml);
    ph.insert(QStringLiteral("{{notice}}"), pageText("upload.notice"));
    ph.insert(QStringLiteral("{{hint}}"), pageText(isUserProgUploadMode() ? "userprog.ul.hint" : "upload.hint"));
    ph.insert(QStringLiteral("{{pick}}"), pageText("upload.pick").toHtmlEscaped());
    ph.insert(QStringLiteral("{{none}}"), pageText("upload.none").toHtmlEscaped());
    ph.insert(QStringLiteral("{{send}}"), pageText("upload.send").toHtmlEscaped());
    ph.insert(QStringLiteral("{{token}}"), token);
    ph.insert(QStringLiteral("{{accept}}"), isUserProgUploadMode()
              ? QStringLiteral("accept=\".db,.sqlite,.sqlite3\"")
              : QString());
    ph.insert(QStringLiteral("{{userProg}}"), isUserProgUploadMode()
              ? QStringLiteral("1") : QStringLiteral("0"));
    ph.insert(QStringLiteral("{{tjson}}"), QString::fromUtf8(QJsonDocument(jsTr).toJson(QJsonDocument::Compact)));
    const QString html = applyPagePlaceholders(QString::fromUtf8(
            "<!DOCTYPE html><html lang=\"{{lang}}\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1\"><title>{{title}}</title>"
            "<link rel=\"icon\" type=\"image/png\" href=\"/favicon.ico\">"
            "<style>"
            ":root{--fotek-blue:#264093;--fotek-orange:#faa731;--bg:#f4f6fb;--card:#fff;--text:#1f2a44;--muted:#5c6b8a;}"
            "*{box-sizing:border-box;}"
            "body{margin:0;padding:20px 16px 32px;font-family:Arial,Helvetica,sans-serif;background:var(--bg);color:var(--text);}"
            ".page{max-width:560px;margin:0 auto;}"
            ".header{background:var(--fotek-blue);color:#fff;padding:18px 20px;border-radius:16px 16px 0 0;"
            "box-shadow:0 8px 24px rgba(38,64,147,.18);}"
            ".header h1{margin:0;font-size:24px;font-weight:700;}"
            ".header p{margin:8px 0 0;font-size:14px;opacity:.92;}"
            ".card{background:var(--card);border:1px solid #dbe3f2;border-top:none;border-radius:0 0 16px 16px;"
            "padding:20px;box-shadow:0 10px 28px rgba(38,64,147,.08);}"
            ".notice{margin:0 0 16px;padding:12px 14px;border-left:4px solid var(--fotek-orange);"
            "background:#fff7ea;border-radius:10px;font-size:14px;line-height:1.45;color:#5a4630;}"
            ".hint{margin:0 0 18px;font-size:15px;line-height:1.5;color:var(--muted);}"
            ".hint b{color:var(--fotek-blue);}"
            ".file-row{display:flex;flex-direction:column;align-items:stretch;gap:12px;}"
            ".btn{border:none;border-radius:12px;padding:14px 20px;font-size:17px;font-weight:700;cursor:pointer;"
            "transition:transform .08s ease,opacity .15s ease;}"
            ".btn:active{transform:scale(.98);}"
            ".btn-pick{background:#fff;color:var(--fotek-blue);border:2px solid var(--fotek-blue);}"
            ".btn-submit{background:var(--fotek-orange);color:#000;box-shadow:0 6px 18px rgba(250,167,49,.35);"
            "display:none;}"
            ".file-name{padding:12px 14px;border-radius:10px;background:#eef2fb;color:var(--muted);font-size:15px;"
            "word-break:break-all;min-height:46px;display:flex;align-items:center;}"
            ".file-name.ready{color:var(--fotek-blue);background:#edf2ff;font-weight:600;}"
            ".progress-wrap{display:none;margin-top:18px;}"
            ".progress-track{height:12px;background:#dbe3f2;border-radius:999px;overflow:hidden;}"
            ".progress-bar{height:100%;width:0;background:linear-gradient(90deg,var(--fotek-orange),#ffc46a);"
            "border-radius:999px;transition:width .15s ease;}"
            ".progress-text{margin:8px 0 0;font-size:14px;color:var(--fotek-blue);font-weight:600;}"
            ".result{margin-top:16px;}"
            ".msg-error{margin:0;padding:12px 14px;border-radius:10px;background:#ffebee;color:#b71c1c;font-size:15px;}"
            "</style></head><body>"
            "<div class=\"page\">"
            "<div class=\"header\">"
            "<h1>{{title}}</h1>"
            "<p>{{serialLabel}}: {{serial}} · {{typeLabel}}: {{type}}</p>"
            "</div>"
            "<div class=\"card\">"
            "<p class=\"notice\">{{notice}}</p>"
            "<p class=\"hint\">{{hint}}</p>"
            "<form id=\"uploadForm\" method=\"post\" action=\"/upload\" enctype=\"multipart/form-data\">"
            "<input type=\"hidden\" name=\"token\" value=\"{{token}}\">"
            "<div class=\"file-row\">"
            "<input id=\"fileInput\" type=\"file\" name=\"file\" multiple {{accept}} style=\"display:none;\">"
            "<button id=\"pickFileBtn\" class=\"btn btn-pick\" type=\"button\">{{pick}}</button>"
            "<div id=\"selectedFiles\" class=\"file-name\">{{none}}</div>"
            "<button id=\"submitBtn\" class=\"btn btn-submit\" type=\"submit\">{{send}}</button>"
            "</div>"
            "</form>"
            "<div id=\"progressWrap\" class=\"progress-wrap\">"
            "<div class=\"progress-track\"><div id=\"progressBar\" class=\"progress-bar\"></div></div>"
            "<p id=\"progressText\" class=\"progress-text\">0%</p>"
            "</div>"
            "<div id=\"result\" class=\"result\"></div>"
            "</div></div>"
            "<script>"
            "(function(){"
            "var T={{tjson}};"
            "var userProg={{userProg}};"
            "var form=document.getElementById('uploadForm');"
            "if(!form) return;"
            "var fileInput=document.getElementById('fileInput');"
            "var pickBtn=document.getElementById('pickFileBtn');"
            "var submitBtn=document.getElementById('submitBtn');"
            "var selectedFiles=document.getElementById('selectedFiles');"
            "function showError(msg){"
            "var res=document.getElementById('result');"
            "if(res){ res.innerHTML='<p class=\"msg-error\">'+msg+'</p>'; }"
            "}"
            "function updateFileSelection(){"
            "var n=(fileInput&&fileInput.files)?fileInput.files.length:0;"
            "if(!selectedFiles) return;"
            "if(n===0){"
            "selectedFiles.textContent=T.none;"
            "selectedFiles.classList.remove('ready');"
            "if(submitBtn) submitBtn.style.display='none';"
            "return;"
            "}"
            "selectedFiles.textContent=fileInput.files[0].name;"
            "selectedFiles.classList.add('ready');"
            "if(submitBtn) submitBtn.style.display='block';"
            "}"
            "if(pickBtn && fileInput){"
            "pickBtn.addEventListener('click',function(){ fileInput.click(); });"
            "fileInput.addEventListener('change',updateFileSelection);"
            "}"
            "form.addEventListener('submit',function(ev){"
            "ev.preventDefault();"
            "var res=document.getElementById('result');"
            "var wrap=document.getElementById('progressWrap');"
            "var bar=document.getElementById('progressBar');"
            "var txt=document.getElementById('progressText');"
            "if(!fileInput || !fileInput.files || fileInput.files.length===0){"
            "showError(T.none);"
            "if(wrap){wrap.style.display='none';}"
            "return;"
            "}"
            "var nameRe=userProg?/\\.(db|sqlite3?)$/i:/.+-\\d+\\.\\d+-\\d+\\.\\d+-\\d+\\.zip$/i;"
            "for(var i=0;i<fileInput.files.length;i++){"
            "var fname=(fileInput.files[i]&&fileInput.files[i].name)?fileInput.files[i].name:'';"
            "if(!nameRe.test(fname)){"
            "showError(T.badName);"
            "if(wrap){wrap.style.display='none';}"
            "return;"
            "}"
            "}"
            "var fd=new FormData(form);"
            "wrap.style.display='block';"
            "bar.style.width='0%';"
            "txt.textContent='0%';"
            "if(res){res.innerHTML='';}"
            "if(submitBtn){ submitBtn.disabled=true; submitBtn.style.opacity='0.7'; }"
            "if(pickBtn){ pickBtn.disabled=true; pickBtn.style.opacity='0.7'; }"
            "var xhr=new XMLHttpRequest();"
            "xhr.open('POST','/upload',true);"
            "xhr.timeout=300000;"
            "xhr.upload.onprogress=function(e){"
            "if(!e.lengthComputable) return;"
            "var p=Math.max(0,Math.min(100,Math.round((e.loaded/e.total)*100)));"
            "bar.style.width=p+'%';"
            "txt.textContent=T.sending+p+'%';"
            "};"
            "function resetUI(){"
            "if(submitBtn){ submitBtn.disabled=false; submitBtn.style.opacity='1'; }"
            "if(pickBtn){ pickBtn.disabled=false; pickBtn.style.opacity='1'; }"
            "}"
            "xhr.onload=function(){"
            "resetUI();"
            "if(xhr.status>=200 && xhr.status<300){"
            "if(res){ res.innerHTML=xhr.responseText||''; }"
            "bar.style.width='100%'; txt.textContent=T.done;"
            "if(submitBtn){ submitBtn.style.display='none'; }"
            "}else{"
            "bar.style.width='0%';"
            "txt.textContent=T.sendError+' ('+xhr.status+')';"
            "showError(xhr.responseText||T.sendError);"
            "}"
            "};"
            "xhr.onerror=function(){"
            "resetUI();"
            "bar.style.width='0%';"
            "txt.textContent=T.netError;"
            "showError(T.netError);"
            "};"
            "xhr.ontimeout=function(){"
            "resetUI();"
            "bar.style.width='0%';"
            "txt.textContent=T.netError;"
            "showError(T.netError);"
            "};"
            "xhr.send(fd);"
            "});"
            "})();"
            "</script>"
            "</body></html>"), ph);
    return html.toUtf8();
}

QByteArray HttpUploadController::buildLogDownloadPageHtml() const
{
    QString serialHtml;
    QString typeHtml;
    fillDeviceIdentityHtml(&serialHtml, &typeHtml);
    QJsonObject jsTr;
    jsTr.insert(QStringLiteral("mb"), pageText("units.mb"));
    jsTr.insert(QStringLiteral("kb"), pageText("units.kb"));
    jsTr.insert(QStringLiteral("dec"), uiLanguage() == QLatin1String("ru") ? QStringLiteral(",") : QStringLiteral("."));
    jsTr.insert(QStringLiteral("transferring"), pageText("log.js.transferring"));
    jsTr.insert(QStringLiteral("transfer"), pageText("log.js.transfer"));
    jsTr.insert(QStringLiteral("of"), pageText("log.js.of"));
    jsTr.insert(QStringLiteral("transferError"), pageText("log.js.transferError"));
    jsTr.insert(QStringLiteral("empty"), pageText("log.js.empty"));
    jsTr.insert(QStringLiteral("savedAs"), pageText("log.js.savedAs"));
    jsTr.insert(QStringLiteral("checkDownloads"), pageText("log.js.checkDownloads"));
    jsTr.insert(QStringLiteral("sessionExpired"), pageText("log.js.sessionExpired"));
    jsTr.insert(QStringLiteral("prepError"), pageText("log.js.prepError"));
    jsTr.insert(QStringLiteral("serverError"), pageText("log.js.serverError"));
    jsTr.insert(QStringLiteral("preparing"), pageText("log.js.preparing"));
    jsTr.insert(QStringLiteral("ready"), pageText("log.js.ready"));
    jsTr.insert(QStringLiteral("unknown"), pageText("log.js.unknown"));
    jsTr.insert(QStringLiteral("noSession"), pageText("log.js.noSession"));
    jsTr.insert(QStringLiteral("preparingWait"), pageText("log.js.preparingWait"));
    jsTr.insert(QStringLiteral("netError"), pageText("log.js.netError"));
    jsTr.insert(QStringLiteral("timeout"), pageText("log.js.timeout"));
    jsTr.insert(QStringLiteral("netPrep"), pageText("log.js.netPrep"));
    jsTr.insert(QStringLiteral("timeoutPrep"), pageText("log.js.timeoutPrep"));
    const bool userProgDownload = isUserProgDownloadMode();
    QHash<QString, QString> ph;
    ph.insert(QStringLiteral("{{lang}}"), uiLanguage());
    ph.insert(QStringLiteral("{{title}}"), pageText(userProgDownload ? "userprog.dl.title" : "log.title").toHtmlEscaped());
    ph.insert(QStringLiteral("{{serialLabel}}"), pageText("common.serial").toHtmlEscaped());
    ph.insert(QStringLiteral("{{typeLabel}}"), pageText("common.type").toHtmlEscaped());
    ph.insert(QStringLiteral("{{serial}}"), serialHtml);
    ph.insert(QStringLiteral("{{type}}"), typeHtml);
    ph.insert(QStringLiteral("{{notice}}"), pageText("log.notice"));
    ph.insert(QStringLiteral("{{hint}}"), pageText(userProgDownload ? "userprog.dl.hint" : "log.hint").toHtmlEscaped());
    ph.insert(QStringLiteral("{{browser}}"), pageText("log.browser").toHtmlEscaped());
    if (userProgDownload) {
        ph.insert(QStringLiteral("{{fileNameBlock}}"),
                  QStringLiteral("<p class=\"fname\"><strong>%1:</strong> %2</p>")
                  .arg(pageText("userprog.dl.fileName").toHtmlEscaped(),
                       QString(sessionDownloadFileName()).toHtmlEscaped()));
    } else {
        ph.insert(QStringLiteral("{{fileNameBlock}}"), QString());
    }
    ph.insert(QStringLiteral("{{button}}"), pageText(userProgDownload ? "userprog.dl.button" : "log.button").toHtmlEscaped());
    QString doneHtml = pageText(userProgDownload ? "userprog.dl.done" : "log.js.done").toHtmlEscaped();
    doneHtml.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    ph.insert(QStringLiteral("{{doneHtml}}"), doneHtml);
    ph.insert(QStringLiteral("{{token}}"), QString(m_sessionToken)
              .replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
              .replace(QLatin1Char('\''), QStringLiteral("\\'")));
    const QString downloadFetchPath = QStringLiteral("/") + sessionPreparedDownloadFetchPath();
    QString downloadFetchPathJs = downloadFetchPath;
    downloadFetchPathJs.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    downloadFetchPathJs.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    ph.insert(QStringLiteral("{{downloadFetchPath}}"), downloadFetchPathJs);
    ph.insert(QStringLiteral("{{fileName}}"), QString(sessionDownloadFileName())
              .replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
              .replace(QLatin1Char('\''), QStringLiteral("\\'")));
    ph.insert(QStringLiteral("{{tjson}}"), QString::fromUtf8(QJsonDocument(jsTr).toJson(QJsonDocument::Compact)));
    const QString html = applyPagePlaceholders(QString::fromUtf8(
            "<!DOCTYPE html><html lang=\"{{lang}}\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, "
            "initial-scale=1\"><title>{{title}}</title>"
            "<link rel=\"icon\" type=\"image/png\" href=\"/favicon.ico\">"
            "<style>"
            ":root{--fotek-blue:#264093;--fotek-orange:#faa731;--bg:#f4f6fb;--card:#fff;--text:#1f2a44;--muted:#5c6b8a;}"
            "*{box-sizing:border-box;}"
            "body{margin:0;padding:20px 16px 32px;font-family:Arial,Helvetica,sans-serif;background:var(--bg);color:var(--text);}"
            ".page{max-width:560px;margin:0 auto;}"
            ".header{background:var(--fotek-blue);color:#fff;padding:18px 20px;border-radius:16px 16px 0 0;"
            "box-shadow:0 8px 24px rgba(38,64,147,.18);}"
            ".header h1{margin:0;font-size:24px;font-weight:700;}"
            ".header p{margin:8px 0 0;font-size:14px;opacity:.92;}"
            ".card{background:var(--card);border:1px solid #dbe3f2;border-top:none;border-radius:0 0 16px 16px;"
            "padding:20px;box-shadow:0 10px 28px rgba(38,64,147,.08);}"
            ".notice{margin:0 0 16px;padding:12px 14px;border-left:4px solid var(--fotek-orange);"
            "background:#fff7ea;border-radius:10px;font-size:14px;line-height:1.45;color:#5a4630;}"
            ".hint{margin:0 0 18px;font-size:15px;line-height:1.5;color:var(--muted);}"
            ".fname{margin:0 0 14px;padding:10px 12px;background:#eef3ff;border-radius:10px;"
            "font-size:15px;line-height:1.45;color:var(--text);}"
            ".btn{border:none;border-radius:12px;padding:14px 20px;font-size:17px;font-weight:700;cursor:pointer;"
            "background:var(--fotek-orange);color:#000;box-shadow:0 6px 18px rgba(250,167,49,.35);width:100%;}"
            ".btn:disabled{opacity:.7;cursor:default;}"
            ".status{margin:14px 0 0;font-size:15px;color:var(--fotek-blue);font-weight:600;min-height:22px;}"
            ".done{display:none;margin:0;padding:14px;border-left:4px solid #2e7d32;background:#e8f5e9;"
            "border-radius:10px;font-size:15px;line-height:1.55;color:#1b5e20;}"
            "</style></head><body>"
            "<div class=\"page\">"
            "<div class=\"header\">"
            "<h1>{{title}}</h1>"
            "<p>{{serialLabel}}: {{serial}} · {{typeLabel}}: {{type}}</p>"
            "</div>"
            "<div class=\"card\">"
            "<p class=\"notice\">{{notice}}</p>"
            "<p class=\"hint\">{{hint}}</p>"
            "{{fileNameBlock}}"
            "<p class=\"notice\">{{browser}}</p>"
            "<button id=\"logDownloadBtn\" class=\"btn\" type=\"button\">{{button}}</button>"
            "<p id=\"logDownloadStatus\" class=\"status\"></p>"
            "<p id=\"logDownloadDone\" class=\"done\">{{doneHtml}}</p>"
            "</div></div>"
            "<script>"
            "(function(){"
            "var T={{tjson}};"
            "var sessionToken='{{token}}';"
            "var downloadFileName='{{fileName}}';"
            "var downloadFetchPath='{{downloadFetchPath}}';"
            "var logBtn=document.getElementById('logDownloadBtn');"
            "var logStatus=document.getElementById('logDownloadStatus');"
            "function setLogStatus(msg){ if(logStatus) logStatus.textContent=msg||''; }"
            "function setBusy(busy){ if(logBtn){ logBtn.disabled=!!busy; } }"
            "function formatSize(bytes){"
            "if(!(bytes>0)) return '0 '+T.mb;"
            "var mb=bytes/1048576;"
            "if(mb<0.1) return Math.max(1,Math.round(bytes/1024))+' '+T.kb;"
            "return mb.toFixed(1).replace('.', T.dec)+' '+T.mb;"
            "}"
            "var logZipLoading=false;"
            "function fetchPreparedZip(fileSize){"
            "if(logZipLoading) return;"
            "logZipLoading=true;"
            "setBusy(true);"
            "var url=downloadFetchPath+'?token='+encodeURIComponent(sessionToken)+'&_='+(Date.now());"
            "setLogStatus(T.transferring+((fileSize>0)?(' '+formatSize(fileSize)):'') );"
            "var a=document.createElement('a');"
            "a.href=url;"
            "a.download=downloadFileName;"
            "a.style.display='none';"
            "document.body.appendChild(a);"
            "a.click();"
            "a.remove();"
            "setTimeout(function(){"
            "logZipLoading=false;"
            "setBusy(false);"
            "if(logBtn) logBtn.style.display='none';"
            "setLogStatus('');"
            "var done=document.getElementById('logDownloadDone');"
            "if(done){ done.style.display='block'; }"
            "var cxhr=new XMLHttpRequest();"
            "cxhr.open('GET','/download/complete?token='+encodeURIComponent(sessionToken)+'&_='+(Date.now()),true);"
            "cxhr.timeout=10000;"
            "cxhr.send();"
            "},3000);"
            "}"
            "function pollArchiveStatus(forceFresh){"
            "var statusUrl='/download/status?token='+encodeURIComponent(sessionToken)"
            "+'&fresh='+(forceFresh?1:0);"
            "var sxhr=new XMLHttpRequest();"
            "sxhr.open('GET',statusUrl,true);"
            "sxhr.timeout=30000;"
            "sxhr.onload=function(){"
            "if(sxhr.status===403){"
            "setBusy(false);"
            "setLogStatus(T.sessionExpired);"
            "return;"
            "}"
            "if(sxhr.status<200||sxhr.status>=300){"
            "setBusy(false);"
            "setLogStatus(T.prepError+' ('+sxhr.status+')');"
            "return;"
            "}"
            "var data=null;"
            "try{ data=JSON.parse(sxhr.responseText); }catch(e){"
            "setBusy(false);"
            "setLogStatus(T.serverError);"
            "return;"
            "}"
            "if(data.state==='building'){"
            "setLogStatus(T.preparing);"
            "setTimeout(function(){ pollArchiveStatus(false); },1500);"
            "return;"
            "}"
            "if(data.state==='error'){"
            "if(!forceFresh){ pollArchiveStatus(true); return; }"
            "setBusy(false);"
            "setLogStatus(data.message||T.prepError);"
            "return;"
            "}"
            "if(data.state==='ready'){"
            "var sz=(data.size!=null)?data.size:0;"
            "setLogStatus(T.ready);"
            "setTimeout(function(){ fetchPreparedZip(sz); },200);"
            "return;"
            "}"
            "setBusy(false);"
            "setLogStatus(T.unknown);"
            "};"
            "sxhr.onerror=function(){ setBusy(false); setLogStatus(T.netPrep); };"
            "sxhr.ontimeout=function(){ setBusy(false); setLogStatus(T.timeoutPrep); };"
            "sxhr.send();"
            "}"
            "function downloadLogArchive(){"
            "if(!sessionToken){ setLogStatus(T.noSession); return; }"
            "setBusy(true);"
            "setLogStatus(T.preparingWait);"
            "pollArchiveStatus(false);"
            "}"
            "if(logBtn){ logBtn.addEventListener('click',downloadLogArchive); }"
            "})();"
            "</script>"
            "</body></html>"), ph);
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
                if (isUserProgUploadMode()) {
                    if (!isUserProgFileName(safe)) {
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
                    const QString tmpPath = QDir::temp().filePath(
                                QStringLiteral("onyx-userprog-upload-%1.db").arg(m_sessionToken));
                    QFile::remove(tmpPath);
                    QFile tmpFile(tmpPath);
                    if (!tmpFile.open(QIODevice::WriteOnly)) {
                        *errorMessage = QStringLiteral("userprog-import");
                        m_userProgImportError = tr("Не удалось сохранить загруженный файл.");
                        return false;
                    }
                    if (tmpFile.write(partBody) != partBody.size()) {
                        tmpFile.close();
                        QFile::remove(tmpPath);
                        *errorMessage = QStringLiteral("userprog-import");
                        m_userProgImportError = tr("Не удалось записать загруженный файл.");
                        return false;
                    }
                    tmpFile.close();
                    QString summary;
                    QString importError;
                    int importedPrograms = 0;
                    int newFolders = 0;
                    int existingFolders = 0;
                    if (!m_userProgTransfer
                            || !m_userProgTransfer->importFromFile(tmpPath, &summary, &importError,
                                                                  &importedPrograms, &newFolders,
                                                                  &existingFolders)) {
                        QFile::remove(tmpPath);
                        *errorMessage = QStringLiteral("userprog-import");
                        m_userProgImportError = importError.isEmpty()
                                ? tr("Не удалось импортировать программы пользователя.")
                                : importError;
                        return false;
                    }
                    QFile::remove(tmpPath);
                    m_userProgImportSummary = summary;
                    m_userProgImportedPrograms = importedPrograms;
                    m_userProgImportedNewFolders = newFolders;
                    m_userProgImportedExistingFolders = existingFolders;
                    ++(*filesSaved);
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
                ? (isUserProgUploadMode()
                   ? QStringLiteral("invalid userprog name")
                   : QStringLiteral("invalid release name"))
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
                sendTranslatedHtml(m_client, 400, "http.title.error", "http.headersTooLong");
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
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.badRequest");
            if (m_client) {
                m_client->disconnectFromHost();
            }
            return;
        }

        const QByteArray reqLine = lines.first().trimmed();
        const QList<QByteArray> parts = reqLine.split(' ');
        if (parts.size() < 2) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.badRequestLine");
            if (m_client) {
                m_client->disconnectFromHost();
            }
            return;
        }
        m_method = QString::fromLatin1(parts[0]);
        m_path = decodeHttpRequestPath(parts[1]);
        const QString requestTarget = QString::fromLatin1(parts[1]);

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
                bindAuthorizedClient(peerAddress, true);
                sendHttpResponse(m_client, 200, "text/html; charset=utf-8",
                                 isPreparedDownloadMode() ? buildLogDownloadPageHtml() : buildUploadPageHtml());
            } else if (isPreparedDownloadMode() && m_path.startsWith(QStringLiteral("/download/status"))) {
                if (!isValidTokenInPath(requestTarget)) {
                    qWarning() << "HttpUploadController: /download/status invalid token from" << peerText
                               << "got=" << requestQueryValue(requestTarget, QStringLiteral("token"))
                               << "expected=" << m_sessionToken;
                    sendDownloadForbidden(m_client, QStringLiteral("token"),
                                          pageText("http.sessionExpiredQr"));
                } else if (!ensureDownloadClientAccess(peerAddress, true)) {
                    qWarning() << "HttpUploadController: /download/status client mismatch from" << peerText
                               << "bound=" << m_authorizedClientAddress.toString();
                    sendDownloadForbidden(m_client, QStringLiteral("client"),
                                          pageText("http.downloadWrongClient"));
                } else {
                    const bool forceRestart = requestQueryValue(requestTarget, QStringLiteral("fresh"))
                            == QStringLiteral("1");
                    qWarning() << "HttpUploadController: GET /download/status from" << peerText
                               << "fresh=" << forceRestart
                               << "cache=" << logArchiveCacheDebugText();
                    startLogArchiveBuildIfNeeded(forceRestart);
                    sendJsonResponse(m_client, 200, logArchiveStatusJson());
                }
            } else if (isPreparedDownloadMode() && m_path.startsWith(QStringLiteral("/download/complete"))) {
                if (!isValidTokenInPath(requestTarget)) {
                    sendDownloadForbidden(m_client, QStringLiteral("token"),
                                          pageText("http.sessionExpiredQr"));
                } else if (!ensureDownloadClientAccess(peerAddress, true)) {
                    sendDownloadForbidden(m_client, QStringLiteral("client"),
                                          pageText("http.confirmWrongClient"));
                } else {
                    qWarning() << "HttpUploadController: log archive download completed by" << peerText;
                    scheduleAccessPointShutdownAfterLogDownload();
                    sendJsonResponse(m_client, 200, QByteArrayLiteral("{\"ok\":true}"));
                }
            } else if (isPreparedDownloadMode()
                       && m_path.startsWith(QStringLiteral("/download/"))
                       && !m_path.startsWith(QStringLiteral("/download/status"))
                       && !m_path.startsWith(QStringLiteral("/download/complete"))) {
                if (!isValidTokenInPath(requestTarget)) {
                    qWarning() << "HttpUploadController: GET download zip invalid token from" << peerText;
                    sendDownloadForbidden(m_client, QStringLiteral("token"),
                                          pageText("http.sessionExpiredQr"));
                } else if (!ensureDownloadClientAccess(peerAddress, true)) {
                    qWarning() << "HttpUploadController: rejected log download from unauthorized client" << peerText
                               << "bound=" << m_authorizedClientAddress.toString();
                    sendDownloadForbidden(m_client, QStringLiteral("client"),
                                          pageText("http.downloadWrongClient"));
                } else {
                    qWarning() << "HttpUploadController: GET download zip from" << peerText
                               << "path=" << m_path
                               << "cache=" << logArchiveCacheDebugText();
                    if (!servePreparedLogArchive(m_client)) {
                        qWarning() << "HttpUploadController: GET download zip not ready, 503";
                        sendJsonResponse(m_client, 503, logArchiveStatusJson());
                    }
                }
            } else if (m_path == QStringLiteral("/favicon.ico")) {
                if (!m_faviconData.isEmpty()) {
                    sendHttpResponse(m_client, 200, "image/png", m_faviconData);
                } else {
                    sendHttpResponse(m_client, 204, QByteArray(), QByteArray());
                }
            } else {
                sendTranslatedHtml(m_client, 404, "http.title.notFound", "http.pageNotFound");
            }
            releaseClientSocket();
            return;
        }

        if (m_method != QStringLiteral("POST")) {
            sendTranslatedHtml(m_client, 404, "http.title.notFound", "http.methodNotSupported");
            releaseClientSocket();
            return;
        }

        if (m_path != QStringLiteral("/upload") || isPreparedDownloadMode()) {
            sendTranslatedHtml(m_client, 404, "http.title.notFound", "http.pageNotFound");
            releaseClientSocket();
            return;
        }
        if (!m_active || m_sessionToken.isEmpty()) {
            qWarning() << "HttpUploadController: rejected upload without active session from" << peerText;
            sendTranslatedHtml(m_client, 403, "http.title.forbidden", "http.uploadSessionInactive");
            releaseClientSocket();
            return;
        }
        if (!ensureDownloadClientAccess(peerAddress, true)) {
            qWarning() << "HttpUploadController: rejected upload from unauthorized client" << peerText;
            sendTranslatedHtml(m_client, 403, "http.title.forbidden", "http.uploadWrongClient");
            releaseClientSocket();
            return;
        }

        const QString cl = m_requestHeaders.value(QStringLiteral("content-length"));
        if (cl.isEmpty()) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.needContentLength");
            releaseClientSocket();
            return;
        }
        bool okLen = false;
        m_contentLength = cl.toLongLong(&okLen);
        if (!okLen || m_contentLength < 0 || m_contentLength > kMaxBodyBytes) {
            sendTranslatedHtml(m_client, 413, "http.title.tooLarge", "http.bodyTooLarge");
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
        sendTranslatedHtml(m_client, 400, "http.title.error", "http.extraBody");
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
        sendTranslatedHtml(m_client, 400, "http.title.error", "http.expectMultipart");
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
            sendTranslatedHtml(m_client, 403, "http.title.forbidden", "http.badToken");
        } else if (err == QStringLiteral("no files")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.noFiles");
        } else if (err == QStringLiteral("too many files")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.tooManyFiles");
        } else if (err == QStringLiteral("invalid release name")) {
            sendSimpleHtml(m_client, 400, pageText("http.title.error"),
                           QStringLiteral("<p>%1</p>").arg(pageText("upload.badName").toHtmlEscaped()));
        } else if (err == QStringLiteral("manifest-missing")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.manifestMissing");
        } else if (err == QStringLiteral("manifest-invalid")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.manifestInvalid");
        } else if (err == QStringLiteral("manifest-deploypaths")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.manifestDeploypaths");
        } else if (err == QStringLiteral("manifest-sha-missing")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.manifestShaMissing");
        } else if (err == QStringLiteral("manifest-path")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.manifestPath");
        } else if (err == QStringLiteral("checksum")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.checksum");
        } else if (err == QStringLiteral("unzip")) {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.unzip");
        } else if (err == QStringLiteral("invalid userprog name")) {
            sendSimpleHtml(m_client, 400, pageText("http.title.error"),
                           QStringLiteral("<p>%1</p>").arg(pageText("userprog.ul.badName").toHtmlEscaped()));
        } else if (err == QStringLiteral("userprog-import")) {
            const QString importText = m_userProgImportError.isEmpty()
                    ? pageText("http.userprogImportFail")
                    : m_userProgImportError;
            sendSimpleHtml(m_client, 400, pageText("http.title.error"),
                           QStringLiteral("<p>%1</p>").arg(importText.toHtmlEscaped()));
        } else {
            sendTranslatedHtml(m_client, 400, "http.title.error", "http.parseForm");
        }
        releaseClientSocket();
        return;
    }

    emit filesReceived(nFiles);
    m_uploadInProgress = false;
    m_uploadProgress = 1.0;
    m_uploadStatusText = tr("Загрузка завершена");
    emit uploadProgressChanged();
    if (isUserProgUploadMode()) {
        scheduleAccessPointShutdownAfterUserProgUpload();
    }
    sendSimpleHtml(m_client, 200, pageText("upload.readyTitle"),
                   QStringLiteral("<div style=\"display:inline-flex;align-items:center;gap:12px;"
                                  "padding:14px 18px;border-radius:12px;background:#edf2ff;"
                                  "border:2px solid #264093;color:#264093;font-size:22px;font-weight:bold;\">"
                                  "<span style=\"display:inline-flex;align-items:center;justify-content:center;"
                                  "width:34px;height:34px;border-radius:50%%;background:#faa731;color:#fff;"
                                  "font-size:24px;line-height:1;\">✓</span>"
                                  "<span>%1</span>"
                                  "</div>").arg(pageText(isUserProgUploadMode()
                                                         ? "userprog.ul.success"
                                                         : "upload.success")));
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
