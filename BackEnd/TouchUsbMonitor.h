#ifndef TOUCHUSBMONITOR_H
#define TOUCHUSBMONITOR_H

#include <QElapsedTimer>
#include <QEvent>
#include <QObject>
#include <QPointF>
#include <QString>

class DeviceLogManager;
class PeriphHandler;
class QMouseEvent;
class QProcess;
class QTimer;
class QTouchEvent;

/**
 * Мониторинг USB-тача ILITEK + простой EMI-фильтр:
 * - первое касание всегда проходит;
 * - во время удержания режем скачки |Δx| > 50;
 * - после release коротко режем повторный press с |Δx| > 50;
 * - во время активации глушим pointer-события.
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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onActivationChanged(bool active);
    void onLiveTouchEvidence();
    bool touchEventRecent() const;
    bool isActivationMute() const;
    bool isPointerEvent(QEvent::Type type) const;
    bool filterMouseEvent(QMouseEvent *mouse);
    bool filterTouchEvent(QTouchEvent *touch);
    void beginContact(const QPointF &screenPos);
    void endContact(const QPointF &screenPos);
    void clearContactState();
    bool postReleaseGuardActive() const;
    bool shouldRejectPress(const QPointF &screenPos) const;
    bool shouldRejectMove(const QPointF &screenPos) const;
    void expireStalePointerState();
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
    QElapsedTimer m_lastTouchEvent;
    QElapsedTimer m_pointerDownSince;
    QElapsedTimer m_postReleaseSince;
    QPointF m_contactPos;
    QPointF m_lastAcceptedPos;
    QPointF m_postReleasePos;
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
    bool m_pointerDown = false;
    bool m_postReleaseArmed = false;
};

#endif // TOUCHUSBMONITOR_H
