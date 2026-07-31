#ifndef DEVICELOGMANAGER_H
#define DEVICELOGMANAGER_H

#include <QObject>
#include <QElapsedTimer>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QStringList>
#include <QTimer>

class JsonStorage;
class SocketModel;

class DeviceLogManager : public QObject
{
    Q_OBJECT
public:
    enum PowerOffReason : quint8 {
        UartPowerOff = 1,
        ServicePowerOff = 2,
        ServiceReboot = 3,
        PowerOffCancelled = 4,
        PowerOffFailed = 5,
        RebootFailed = 6
    };

    explicit DeviceLogManager(JsonStorage *jsonStorage,
                              SocketModel *socketModel,
                              QObject *parent = nullptr);

    Q_INVOKABLE QStringList readLogLines(const QString &filter, int maxLines = 1000) const;
    Q_INVOKABLE QStringList readLogLines(const QString &filter, const QString &date, int maxLines = 1000) const;
    Q_INVOKABLE QStringList availableLogDates() const;
    Q_INVOKABLE QString todayLogDate() const;
    Q_INVOKABLE QString adjacentLogDate(const QString &date, int direction) const;
    Q_INVOKABLE int logLineCount(const QString &filter, const QString &date) const;
    Q_INVOKABLE QString logFilePath() const;

public slots:
    void beginSession();
    void finalizeSession();
    void persistCounters();
    void onActivationStarted(quint8 socketId, bool isCut, quint16 mode, quint16 power,
                             bool autoMode, quint8 sourceCode);
    void onActivationStopped(quint8 stopReason);
    void onWarningCode(quint8 warningCode);
    void logPowerOff(quint8 reasonCode);

private:
    struct ActivationInfo {
        bool active = false;
        quint8 socketId = 0;
        bool isCut = false;
        quint16 mode = 0;
        quint16 power = 0;
        int modeId = 0;
        int instrumentId = 0;
        bool autoMode = false;
        quint8 sourceCode = 0;
        QDateTime startedAt;
        QElapsedTimer timer;
        qint64 persistedMs = 0;
    };

    struct WarningAccumulator {
        QDateTime lastSeen;
        QDateTime summaryStartedAt;
        qint64 suppressedCount = 0;
    };

    QString logDirPath() const;
    bool ensureLogDir() const;
    QString logFilePathForDate(const QDate &date) const;
    QString logFileNameForDate(const QDate &date) const;
    QDate dateFromLogFileName(const QString &fileName) const;
    QString formatLogDate(const QDate &date) const;
    QDate normalizedLogDate(const QString &date) const;
    QStringList sortedLogDates() const;
    void migrateLegacyLogFile() const;
    bool isDailyLogOverSizeLimit(const QString &filePath) const;
    void appendEvent(const QString &category, const QString &message);
    void appendCompactEvent(const QStringList &fields);
    void appendBootEvent(const QString &deviceType,
                         const QString &serialNumber,
                         qint64 runtimeMs,
                         qint64 activationMs);
    void appendActivationEvent(const ActivationInfo &activation, qint64 durationMs);
    void appendPowerOffEvent(quint8 reasonCode);
    void appendWarningEvent(quint8 warningCode,
                            qint64 repeatedCount = 0,
                            qint64 periodSeconds = 0);
    QString warningMessage(quint8 warningCode) const;
    QString expandedLogLine(const QString &line, const QDate &date) const;
    void flushWarningSummaries(bool force);
    bool lineMatchesFilter(const QString &line, const QString &filter) const;
    QString formatDuration(qint64 milliseconds) const;
    QString formatTotalDuration(qint64 milliseconds) const;
    QString formatCompactDuration(qint64 milliseconds) const;
    QString formatCompactTotalDuration(qint64 milliseconds) const;
    QString activationOutputCode(quint8 socketId) const;
    QString activationSourceCode(bool autoMode, quint8 sourceCode) const;
    QString warningTextForCode(int code) const;
    QString powerOffTextForCode(int code) const;
    QString sourceText(bool autoMode, quint8 sourceCode) const;
    QString socketData(int socketId, int role) const;
    qint64 readCounter(const QString &key) const;
    void saveCounter(const QString &key, qint64 value);

    JsonStorage *m_jsonStorage = nullptr;
    SocketModel *m_socketModel = nullptr;
    mutable QMutex m_mutex;
    QTimer m_persistTimer;
    QTimer m_warningFlushTimer;
    QElapsedTimer m_sessionTimer;
    ActivationInfo m_activation;
    QHash<quint8, WarningAccumulator> m_pendingWarnings;
    qint64 m_runtimeBaseMs = 0;
    bool m_sessionFinalized = false;
};

#endif // DEVICELOGMANAGER_H
