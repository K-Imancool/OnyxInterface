#ifndef TOUCHUSBMONITOR_H
#define TOUCHUSBMONITOR_H

#include <QObject>
#include <QString>

class DeviceLogManager;
class PeriphHandler;
class QProcess;
class QTimer;

/**
 * Мониторинг USB-тача ILITEK (222a:0001 / WINSTARTP).
 *
 * Проверка — только чтение /proc/bus/input/devices (как список устройств в evtest),
 * без сброса порта. Power-cycle — только если WINSTARTP стабильно отсутствует.
 */
class TouchUsbMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool touchOk READ touchOk NOTIFY touchOkChanged)
    Q_PROPERTY(bool faultActive READ faultActive NOTIFY faultActiveChanged)
    Q_PROPERTY(bool recovering READ recovering NOTIFY recoveringChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)

public:
    static constexpr quint8 FaultWarningCode = 0x09;

    explicit TouchUsbMonitor(DeviceLogManager *deviceLog, QObject *parent = nullptr);

    void setPeriphHandler(PeriphHandler *periph);

    bool touchOk() const { return m_touchOk; }
    bool faultActive() const { return m_faultActive; }
    bool recovering() const { return m_recovering; }
    QString state() const { return m_state; }
    QString detail() const { return m_detail; }

    Q_INVOKABLE void check();

signals:
    void touchOkChanged();
    void faultActiveChanged();
    void recoveringChanged();
    void stateChanged();
    void detailChanged();
    void touchUsbFault(const QString &state, const QString &detail);

private:
    void setRecovering(bool on);
    void applyState(const QString &state, const QString &detail, bool logFault);
    void startRecover(const QString &detail);
    void onRecoverFinished(int exitCode);
    void schedulePostRecoverRecheck();
    void raiseScreenFault();
    void clearScreenFault();
    void updatePollInterval();
    static bool touchInputPresent();
    static bool touchUsbDevicePresent(QString *sysfsPath = nullptr);

    DeviceLogManager *m_deviceLog = nullptr;
    PeriphHandler *m_periph = nullptr;
    QTimer *m_timer = nullptr;
    QProcess *m_process = nullptr;
    QString m_state = QStringLiteral("ok");
    QString m_detail;
    QString m_lastLoggedKey;
    int m_recoverGeneration = 0;
    int m_recoverAttempts = 0;
    int m_missingConfirmCount = 0;
    bool m_touchOk = true;
    bool m_faultActive = false;
    bool m_recovering = false;
    bool m_screenFaultShown = false;
};

#endif // TOUCHUSBMONITOR_H
