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
class QTimer;
class QTouchEvent;

/**
 * Мониторинг USB-тача ILITEK + простой EMI-фильтр:
 * - первое касание всегда проходит;
 * - во время удержания режем скачки |Δx| > 50;
 * - после release коротко режем повторный press с |Δx| > 50;
 * - во время активации глушим pointer-события, кроме AutoStartStop (ACC):
 *   тогда касание доходит до UI и останавливает активацию (stopIfAccTouch).
 * При старте и пока 222a:0001 виден, запоминается его USB-порт.
 * После каждой активации проверяется WINSTARTP Mouse. Если его нет —
 * запомненный порт перезапускается, пока мышь не появится.
 * Во время сброса и коротко после появления мыши pointer-события глушатся
 * (ложный press от usbhid при перечислении не доходит до UI).
 * Ошибка 0x09 показывается через 15 с и снимается вместе с восстановлением.
 * Release в UI всегда сразу (без debounce), чтобы holdRepeat гас мгновенно.
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
    void beginPostActivationCheck();
    void probeTouchPort();
    bool rememberTouchPort();
    void loadRememberedPort();
    void saveRememberedPort() const;
    void kickPortCycle();
    void onPortStep();
    void onRecoverDeadline();
    void noteOngoingFault(const QString &detail);
    void onSettlePoll();
    void finishRecovered(const QString &detail);
    QString resolvePortDisablePath();
    bool writePortDisable(const QString &path, bool disabled);
    bool isActivationMute() const;
    bool isRecoverMute() const;
    void armPostRecoverMute();
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
    void raiseScreenFault();
    void clearScreenFault();
    static bool touchInputPresent();
    static bool touchUsbDevicePresent(QString *sysfsPath = nullptr);
    static QString hubDisablePath(int bus);
    static bool isAllowedDisablePath(const QString &path);

    DeviceLogManager *m_deviceLog = nullptr;
    PeriphHandler *m_periph = nullptr;
    QTimer *m_stepTimer = nullptr;
    QTimer *m_deadlineTimer = nullptr;
    QTimer *m_portProbeTimer = nullptr;
    QTimer *m_settleTimer = nullptr;
    QElapsedTimer m_checkStarted;
    QElapsedTimer m_portOnSince;
    QElapsedTimer m_pointerDownSince;
    QElapsedTimer m_postReleaseSince;
    QElapsedTimer m_postRecoverMuteSince;
    QString m_activeDisablePath;
    QString m_lastDisablePath;
    QPointF m_contactPos;
    QPointF m_lastAcceptedPos;
    QPointF m_postReleasePos;
    QString m_state = QStringLiteral("ok");
    QString m_detail;
    QString m_lastLoggedKey;
    int m_cycle = 0;
    int m_portPhase = 0;
    int m_portProbeLeft = 0;
    bool m_touchOk = true;
    bool m_faultActive = false;
    bool m_recovering = false;
    bool m_screenFaultShown = false;
    bool m_activationWasOn = false;
    bool m_pointerDown = false;
    bool m_postReleaseArmed = false;
};

#endif // TOUCHUSBMONITOR_H
