#ifndef HTTPUPLOADCONTROLLER_H
#define HTTPUPLOADCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QMutex>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QVariantList>
#include <QList>

class JsonStorage;
class LinkStm;
class UserProgTransferController;

/// Минимальный HTTP-приём файлов по Wi‑Fi (GET форма + POST multipart) для Qt 5.15.
class HttpUploadController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(QString sessionToken READ sessionToken NOTIFY sessionTokenChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int listenPort READ listenPort NOTIFY listenPortChanged)
    Q_PROPERTY(QString uploadSavePath READ uploadSavePath NOTIFY uploadSavePathChanged)
    Q_PROPERTY(QString qrImagePath READ qrImagePath NOTIFY qrImagePathChanged)
    Q_PROPERTY(QString qrStatusText READ qrStatusText NOTIFY qrStatusTextChanged)
    Q_PROPERTY(bool accessPointActive READ accessPointActive NOTIFY accessPointChanged)
    Q_PROPERTY(bool accessPointClientConnected READ accessPointClientConnected NOTIFY accessPointClientConnectedChanged)
    Q_PROPERTY(QString accessPointSsid READ accessPointSsid NOTIFY accessPointChanged)
    Q_PROPERTY(QString accessPointPassword READ accessPointPassword NOTIFY accessPointChanged)
    Q_PROPERTY(QString accessPointStatusText READ accessPointStatusText NOTIFY accessPointStatusTextChanged)
    Q_PROPERTY(QString logDownloadUrl READ logDownloadUrl NOTIFY logDownloadUrlChanged)
    Q_PROPERTY(bool logDownloadMode READ isLogDownloadMode NOTIFY sessionModeChanged)
    Q_PROPERTY(bool userProgDownloadMode READ isUserProgDownloadMode NOTIFY sessionModeChanged)
    Q_PROPERTY(bool userProgUploadMode READ isUserProgUploadMode NOTIFY sessionModeChanged)
    Q_PROPERTY(bool userProgImportCompleted READ userProgImportCompleted NOTIFY userProgImportCompletedChanged)
    Q_PROPERTY(QString userProgImportSummary READ userProgImportSummary NOTIFY userProgImportCompletedChanged)
    Q_PROPERTY(int userProgImportedPrograms READ userProgImportedPrograms NOTIFY userProgImportCompletedChanged)
    Q_PROPERTY(int userProgImportedNewFolders READ userProgImportedNewFolders NOTIFY userProgImportCompletedChanged)
    Q_PROPERTY(int userProgImportedExistingFolders READ userProgImportedExistingFolders NOTIFY userProgImportCompletedChanged)
    Q_PROPERTY(bool logArchiveReady READ logArchiveReady NOTIFY logArchiveStateChanged)
    Q_PROPERTY(bool logArchiveBuilding READ logArchiveBuilding NOTIFY logArchiveStateChanged)
    Q_PROPERTY(QString logArchiveError READ logArchiveError NOTIFY logArchiveStateChanged)
    Q_PROPERTY(bool logDownloadCompleted READ logDownloadCompleted NOTIFY logDownloadCompletedChanged)
    Q_PROPERTY(QString userProgDownloadFileName READ userProgDownloadFileName NOTIFY userProgDownloadFileNameChanged)
    Q_PROPERTY(bool uploadInProgress READ uploadInProgress NOTIFY uploadProgressChanged)
    Q_PROPERTY(double uploadProgress READ uploadProgress NOTIFY uploadProgressChanged)
    Q_PROPERTY(QString uploadStatusText READ uploadStatusText NOTIFY uploadProgressChanged)
    Q_PROPERTY(QString detectedReleaseVersion READ detectedReleaseVersion NOTIFY detectedReleaseChanged)
    Q_PROPERTY(QString detectedBinaryVersion READ detectedBinaryVersion NOTIFY detectedReleaseChanged)
    Q_PROPERTY(QString detectedMediaVersion READ detectedMediaVersion NOTIFY detectedReleaseChanged)
    Q_PROPERTY(QString detectedComVersion READ detectedComVersion NOTIFY detectedReleaseChanged)
    Q_PROPERTY(QString detectedArgVersion READ detectedArgVersion NOTIFY detectedReleaseChanged)
    Q_PROPERTY(QString detectedGenVersion READ detectedGenVersion NOTIFY detectedReleaseChanged)
    Q_PROPERTY(QString currentMediaVersion READ currentMediaVersion NOTIFY currentMediaVersionChanged)
    Q_PROPERTY(QStringList availableMainVersions READ availableMainVersions NOTIFY releaseVersionsChanged)
    Q_PROPERTY(QStringList availableMediaVersions READ availableMediaVersions NOTIFY releaseVersionsChanged)
    Q_PROPERTY(QStringList availableComVersions READ availableComVersions NOTIFY releaseVersionsChanged)
    Q_PROPERTY(QStringList availableArgVersions READ availableArgVersions NOTIFY releaseVersionsChanged)
    Q_PROPERTY(QStringList availableGenVersions READ availableGenVersions NOTIFY releaseVersionsChanged)
    Q_PROPERTY(int mcFirmwareUpdateProgress READ mcFirmwareUpdateProgress NOTIFY mcFirmwareUpdateProgressChanged)

public:
    explicit HttpUploadController(QObject *parent = nullptr);

    void setJsonStorage(JsonStorage *storage);
    void setLinkStm(LinkStm *linkStm);
    void setUserProgTransfer(UserProgTransferController *transfer);

    bool isActive() const { return m_active; }
    QString sessionToken() const { return m_sessionToken; }
    QString baseUrl() const { return m_baseUrl; }
    QString lastError() const { return m_lastError; }
    int listenPort() const { return m_port; }
    QString uploadSavePath() const;
    QString qrImagePath() const { return m_qrImagePath; }
    QString qrStatusText() const { return m_qrStatusText; }
    bool accessPointActive() const { return m_apActive; }
    bool accessPointClientConnected() const { return m_apClientConnected; }
    QString accessPointSsid() const { return m_apSsid; }
    QString accessPointPassword() const { return m_apPassword; }
    QString accessPointStatusText() const { return m_apStatusText; }
    QString logDownloadUrl() const { return m_logDownloadUrl; }
    bool isLogDownloadMode() const;
    bool isUserProgDownloadMode() const;
    bool isUserProgUploadMode() const;
    bool userProgImportCompleted() const { return m_userProgImportCompleted; }
    QString userProgImportSummary() const { return m_userProgImportSummary; }
    int userProgImportedPrograms() const { return m_userProgImportedPrograms; }
    int userProgImportedNewFolders() const { return m_userProgImportedNewFolders; }
    int userProgImportedExistingFolders() const { return m_userProgImportedExistingFolders; }
    bool logArchiveReady() const;
    bool logArchiveBuilding() const;
    QString logArchiveError() const;
    bool logDownloadCompleted() const { return m_logDownloadCompleted; }
    QString userProgDownloadFileName() const { return m_userProgDownloadFileName; }
    bool uploadInProgress() const { return m_uploadInProgress; }
    double uploadProgress() const { return m_uploadProgress; }
    QString uploadStatusText() const { return m_uploadStatusText; }
    QString detectedReleaseVersion() const { return m_detectedReleaseVersion; }
    QString detectedBinaryVersion() const { return m_detectedBinaryVersion; }
    QString detectedMediaVersion() const { return m_detectedMediaVersion; }
    QString detectedComVersion() const { return m_detectedComVersion; }
    QString detectedArgVersion() const { return m_detectedArgVersion; }
    QString detectedGenVersion() const { return m_detectedGenVersion; }
    QString currentMediaVersion() const { return m_currentMediaVersion; }
    QStringList availableMainVersions() const { return m_availableMainVersions; }
    QStringList availableMediaVersions() const { return m_availableMediaVersions; }
    QStringList availableComVersions() const { return m_availableComVersions; }
    QStringList availableArgVersions() const { return m_availableArgVersions; }
    QStringList availableGenVersions() const { return m_availableGenVersions; }
    int mcFirmwareUpdateProgress() const { return m_mcFirmwareUpdateProgress; }

    Q_INVOKABLE void startSession();
    Q_INVOKABLE void startLogDownloadSession();
    Q_INVOKABLE void startUserProgDownloadSession(const QVariantList &scopeIds,
                                                  const QString &downloadFileName = QString());
    Q_INVOKABLE QString buildUserProgExportFileName(const QString &namePrefix) const;
    Q_INVOKABLE void startUserProgUploadSession();
    Q_INVOKABLE void stopSession();
    Q_INVOKABLE QStringList localIpv4Addresses() const;
    Q_INVOKABLE void refreshReleaseVersions();
    Q_INVOKABLE bool applyMainVersion(const QString &version);
    Q_INVOKABLE bool applyMediaVersion(const QString &version);
    Q_INVOKABLE bool applyComVersion(const QString &version);
    Q_INVOKABLE bool applyArgVersion(const QString &version);
    Q_INVOKABLE bool applyGenVersion(const QString &version);
    Q_INVOKABLE void abortMcFirmwareUpdate();
    Q_INVOKABLE bool restartDemo1UserService();

public slots:
    void setMcFirmwareUpdateProgress(int progress);
    void onMcFirmwareParseError(const QString &message);

signals:
    void activeChanged();
    void sessionTokenChanged();
    void baseUrlChanged();
    void lastErrorChanged();
    void listenPortChanged();
    void uploadSavePathChanged();
    void qrImagePathChanged();
    void qrStatusTextChanged();
    void accessPointChanged();
    void accessPointClientConnectedChanged();
    void accessPointStatusTextChanged();
    void logDownloadUrlChanged();
    void sessionModeChanged();
    void logArchiveStateChanged();
    void logDownloadCompletedChanged();
    void userProgDownloadFileNameChanged();
    void userProgImportCompletedChanged();
    void uploadProgressChanged();
    void detectedReleaseChanged();
    void currentMediaVersionChanged();
    void releaseVersionsChanged();
    void filesReceived(int count);
    void mcFirmwareUpdateProgressChanged();

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onSessionTimeout();
    void pollAccessPointClient();

private:
    void setActive(bool v);
    void setLastError(const QString &e);
    void setAccessPointStatusText(const QString &text);
    void setAccessPointClientConnected(bool connected);
    void updateBaseUrl();
    QHostAddress preferredListenAddress() const;
    QHostAddress effectiveClientAddress() const;
    QString selectWifiInterface() const;
    QHostAddress addressForInterface(const QString &ifaceName) const;
    struct AccessPointSetupResult {
        bool ok = false;
        bool profileCreated = false;
        QString errorText;
        QString interfaceName;
        QHostAddress address;
    };
    bool startAccessPoint(QString *errorText);
    void stopAccessPoint();
    AccessPointSetupResult runAccessPointSetup(const QString &interfaceName,
                                               const QString &ssid,
                                               const QString &connectionName,
                                               const QString &configuredAddress,
                                               const QString &password) const;
    void startLogDownloadSessionInternal();
    void startDeferredAccessPointSession();
    void launchAccessPointThenActivate(quint64 generation);
    bool activateHttpAfterAccessPoint();
    void setLogDownloadCompleted(bool completed);
    void scheduleAccessPointShutdownAfterLogDownload();
    bool runCommandWithSudoFallback(const QString &program, const QStringList &args,
                                    int timeoutMs, QString *stdoutText = nullptr,
                                    QString *stderrText = nullptr) const;
    bool runNmcli(const QStringList &args, int timeoutMs,
                  QString *stdoutText = nullptr, QString *stderrText = nullptr) const;
    bool generateQrCodeImage(const QString &payload, const QString &fileName,
                             QString *imagePath, QString *errorText) const;
    QString accessPointQrPayload() const;
    QString makeAccessPointPassword() const;
    bool hasConnectedAccessPointClient() const;
    QString accessPointIpAddressString() const;
    void loadNetworkSettings();
    bool wifiAlwaysEnabled() const;
    bool ensureWifiReadyForSession(QString *errorText = nullptr);
    void cleanupWifiAfterSession();
    bool invokeUploadFirewallGuard(const QString &action, QString *errorText = nullptr) const;
    void tryProcessBuffer();
    void sendHttpResponse(QTcpSocket *socket, int statusCode, const QByteArray &contentType, const QByteArray &body);
    void sendFileDownloadResponse(QTcpSocket *socket, const QByteArray &downloadFileName, const QByteArray &body);
    void sendJsonResponse(QTcpSocket *socket, int statusCode, const QByteArray &jsonBody);
    void sendSimpleHtml(QTcpSocket *socket, int statusCode, const QString &title, const QString &bodyHtml);
    enum class SessionKind {
        FirmwareUpload,
        LogDownload,
        UserProgDownload,
        UserProgUpload
    };
    void beginSession(SessionKind kind);
    void setSessionKind(SessionKind kind);
    bool isPreparedDownloadMode() const;
    void setUserProgImportCompleted(bool completed);
    QString sanitizeUserProgNamePrefix(const QString &raw) const;
    QString userProgExportSuffix() const;
    QString sessionDownloadFileName() const;
    QString sessionPreparedDownloadFetchPath() const;
    QByteArray sessionDownloadContentType() const;
    void scheduleAccessPointShutdownAfterUserProgUpload();
    void fillDeviceIdentityHtml(QString *serialHtml, QString *typeHtml) const;
    QString uiLanguage() const;
    QString pageText(const char *key) const;
    QString deviceTypeFileTag() const;
    QByteArray buildUploadPageHtml() const;
    QByteArray buildLogDownloadPageHtml() const;
    bool isAuthorizedClient(const QHostAddress &peer) const;
    void bindAuthorizedClient(const QHostAddress &peer, bool allowRebind = false);
    bool ensureDownloadClientAccess(const QHostAddress &peer, bool allowRebind);
    void sendDownloadForbidden(QTcpSocket *socket, const QString &reason, const QString &message);
    bool parseMultipartAndSave(const QByteArray &body, const QString &contentType, int *filesSaved, QString *errorMessage);
    bool processReleaseArchiveBytes(const QString &sourceFileName, const QByteArray &archiveBytes, QString *errorMessage);
    static QString normalizeRelPath(const QString &rawRelPath);
    static bool copyFileReplace(const QString &srcPath, const QString &dstPath, QString *errorMessage);
    void releaseClientSocket();
    void setDetectedReleaseInfo(const QString &releaseVer,
                                const QString &binaryVer,
                                const QString &mediaVer,
                                const QString &comVer,
                                const QString &argVer,
                                const QString &genVer);

    static bool extractMultipartBoundary(const QString &contentType, QByteArray *boundaryPrefixOut);
    static QString sanitizeFileName(const QString &rawName);
    QString makeUniquePath(const QString &fileName) const;
    QString effectiveUploadDir() const;
    void updateQrCode();
    void updateLogDownloadUrl();
    bool isValidTokenInPath(const QString &path) const;
    bool buildLogArchiveBundle(const QString &sessionToken, QString *outFilePath, qint64 *outFileSize,
                               QString *errorHtml) const;
    QString logArchiveDownloadFileName() const;
    bool sendFileDownloadFromPath(QTcpSocket *socket, const QString &filePath);
    void drainPendingConnections();
    void resetLogArchiveCache();
    void startLogArchiveBuildIfNeeded(bool forceRestart);
    QByteArray logArchiveStatusJson() const;
    QString logArchiveCacheDebugText() const;
    QString logArchiveCacheDebugTextUnlocked() const;
    bool servePreparedLogArchive(QTcpSocket *socket);
    static bool hasVersionListChanged(const QStringList &oldList, const QStringList &newList);
    static bool copyDirectoryContentsReplace(const QString &srcDirPath, const QString &dstDirPath, QString *errorMessage);
    void setCurrentMediaVersion(const QString &version);
    bool applyMcFirmwareFromReleases(const QString &version, const QString &releasesSubdir,
                                     const QString &filePrefixUpper, int mcUnitRaw);

    JsonStorage *m_json = nullptr;
    LinkStm *m_linkStm = nullptr;
    int m_mcFirmwareUpdateProgress = -1;
    QTcpServer *m_server = nullptr;
    QTcpSocket *m_client = nullptr;
    QByteArray m_rxBuffer;
    bool m_headerComplete = false;
    QString m_method;
    QString m_path;
    QMap<QString, QString> m_requestHeaders;
    qint64 m_contentLength = -1;
    bool m_active = false;
    QString m_sessionToken;
    QString m_baseUrl;
    QString m_lastError;
    QString m_qrImagePath;
    QString m_qrStatusText;
    bool m_apActive = false;
    bool m_apClientConnected = false;
    QString m_apSsid = QStringLiteral("ONYX-SERVICE");
    QString m_apPassword = QStringLiteral("Electrosurgical");
    QString m_apInterfaceName;
    QString m_apConnectionName = QStringLiteral("ONYX-SERVICE-update");
    QString m_apConfiguredAddress = QStringLiteral("192.168.50.1/24");
    QHostAddress m_apAddress;
    QString m_apStatusText;
    QString m_logDownloadUrl;
    SessionKind m_sessionKind = SessionKind::FirmwareUpload;
    bool m_logDownloadCompleted = false;
    bool m_userProgImportCompleted = false;
    QString m_userProgImportSummary;
    QString m_userProgImportError;
    int m_userProgImportedPrograms = 0;
    int m_userProgImportedNewFolders = 0;
    int m_userProgImportedExistingFolders = 0;
    QList<int> m_userProgExportScopeIds;
    QString m_userProgDownloadFileName;
    UserProgTransferController *m_userProgTransfer = nullptr;
    bool m_uploadInProgress = false;
    double m_uploadProgress = 0.0;
    QString m_uploadStatusText;
    QString m_detectedReleaseVersion;
    QString m_detectedBinaryVersion;
    QString m_detectedMediaVersion;
    QString m_detectedComVersion;
    QString m_detectedArgVersion;
    QString m_detectedGenVersion;
    QString m_currentMediaVersion;
    QStringList m_availableMainVersions;
    QStringList m_availableMediaVersions;
    QStringList m_availableComVersions;
    QStringList m_availableArgVersions;
    QStringList m_availableGenVersions;
    int m_port = 57891;
    QString m_uploadDir; // из JSON или по умолчанию
    QString m_publicBaseUrl;
    bool m_trustProxyHeaders = false;
    QHostAddress m_listenAddress;
    QHostAddress m_authorizedClientAddress;
    QTimer m_sessionTimer;
    QTimer m_apClientPollTimer;
    quint64 m_sessionGeneration = 0;

    enum class LogArchiveState {
        Idle,
        Building,
        Ready,
        Error
    };
    mutable QMutex m_logArchiveMutex;
    LogArchiveState m_logArchiveState = LogArchiveState::Idle;
    QByteArray m_logArchivePayload;
    QString m_logArchiveFilePath;
    qint64 m_logArchiveFileSize = 0;
    QString m_logArchiveErrorText;

    static constexpr int kSessionTimeoutMs = 15 * 60 * 1000;
    static constexpr qint64 kMaxBodyBytes = 500LL * 1024 * 1024;
    static constexpr int kMaxFilesPerRequest = 20;
};

#endif // HTTPUPLOADCONTROLLER_H
