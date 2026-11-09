#ifndef TOUCHUSBMONITOR_H
#define TOUCHUSBMONITOR_H

#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QString>

class DeviceLogManager;
class PeriphHandler;
class QMouseEvent;
class QTimer;
class QTouchDevice;
class QTouchEvent;

/**
 * Мониторинг USB-тача ILITEK/PIC + EMI-фильтр:
 * - одиночный тап, даже короткий, сразу в UI;
 * - пока палец зажат, скачок (больше живого свайпа за кадр) не двигает точку;
 * - краткий второй слот вырезается и не задерживает отпускание;
 * - после ACT — повышенное внимание не меньше минуты; гаснет позже минуты,
 *   только когда ложных касаний уже нет. Новый Begin на той же строке вскоре после End
 *   не становится кликом, пока не видно, что это живой палец;
 * - дубликат Begin в ту же точку не глотаем (иначе окно не получает press);
 * - во время активации глушим pointer, кроме AutoStartStop (ACC).
 * При старте и пока виден USB-тач (ILITEK 222a:0001 или PIC16F1459 04d8:4931),
 * запоминается его USB-порт.
 * После старта UI и после каждой активации проверяется вход
 * ("WINSTARTP Mouse" либо "PIC16F1459"). Если узла нет — порт перезапускается.
 * Ошибка 0x09 через 15 с без входа; снимается, когда вход снова есть.
 */
class TouchUsbMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool touchOk READ touchOk NOTIFY touchOkChanged)
    Q_PROPERTY(bool faultActive READ faultActive NOTIFY faultActiveChanged)
    Q_PROPERTY(bool recovering READ recovering NOTIFY recoveringChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY detailChanged)
    // TEST: квадрат в углу экрана. Удалить вместе с main.qml после теста восстановления тача.
    Q_PROPERTY(int testRecoverAttempts READ testRecoverAttempts NOTIFY testRecoverAttemptsChanged)
    Q_PROPERTY(bool testRecoverVisible READ testRecoverVisible NOTIFY testRecoverVisibleChanged)

public:
    static constexpr quint8 FaultWarningCode = 0x09;

    explicit TouchUsbMonitor(DeviceLogManager *deviceLog, QObject *parent = nullptr);

    void setPeriphHandler(PeriphHandler *periph);

    bool touchOk() const { return m_touchOk; }
    bool faultActive() const { return m_faultActive; }
    bool recovering() const { return m_recovering; }
    QString state() const { return m_state; }
    QString detail() const { return m_detail; }
    // TEST: см. testRecoverAttempts. Удалить после теста.
    int testRecoverAttempts() const { return m_testRecoverAttempts; }
    bool testRecoverVisible() const { return m_testRecoverVisible; }

    Q_INVOKABLE void check();
    // Обнуляет touch-trace.log. Вызывается при входе в проверку тача.
    Q_INVOKABLE void clearTouchTrace();
    // Снимает зажатый жест и захват. Выход из проверки тача зовёт это
    // до уничтожения экрана, иначе отпускание падает на кнопку меню.
    Q_INVOKABLE void releaseHeldPress();

signals:
    void touchOkChanged();
    void faultActiveChanged();
    void recoveringChanged();
    void stateChanged();
    void detailChanged();
    // TEST: удалить после теста.
    void testRecoverAttemptsChanged();
    void testRecoverVisibleChanged();
    void touchUsbFault(const QString &state, const QString &detail);
    // Точка, которую фильтр не отдал в UI. Проверка тача рисует её бледным кругом.
    void falsePoint(qreal x, qreal y);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onActivationChanged(bool active);
    void onLiveTouchEvidence();
    void beginStartupCheck();
    void beginPostActivationCheck();
    void beginMouseCheck(const QString &missingDetail);
    void abortRecoverForActivation();
    void releaseHeldPort();
    void probeTouchPort();
    bool rememberTouchPort();
    void loadRememberedPort();
    void saveRememberedPort() const;
    void kickPortCycle();
    void onPortStep();
    void onRecoverDeadline();
    void noteOngoingFault(const QString &detail);
    void finishRecovered(const QString &detail);
    QString resolvePortDisablePath();
    bool writePortDisable(const QString &path, bool disabled);
    bool isActivationMute() const;
    bool isRecoverMute() const;
    void armPostRecoverMute();
    bool isPointerEvent(QEvent::Type type) const;
    // Фильтрация выключена. Только помечает прыжок и лишний палец для TouchTest.
    void observeGhosts(QEvent *event);
    bool filterMouseEvent(QObject *watched, QMouseEvent *mouse);
    bool filterTouchEvent(QObject *watched, QTouchEvent *touch);
    void beginContact(const QPointF &screenPos);
    void endContact();
    void clearContactState();
    bool attentionActive() const;
    bool inAttentionWindow() const;
    void noteFalseTouch();
    void pollAttention();
    bool releaseStitchPending() const;
    void markGestureDirty();
    qint64 releaseStitchDelayMs() const;
    void armReleaseStitch(QObject *watched, bool oweTouchEnd, bool oweMouseRelease,
                          QTouchDevice *device, int touchId);
    void keepReleaseStitch(const QPointF &screenPos, int touchId);
    void keepReleaseStitch(const QTouchEvent *touch);
    bool releaseDeadlineSlides(const QPointF &screenPos, int touchId) const;
    void noteLiveTouchReleased(QTouchDevice *device);
    void commitReleaseStitch();
    bool absorbContinuingTouch(QTouchEvent *touch);
    void sendDelayedTouchEnd(QObject *target, QTouchDevice *device, int touchId,
                             const QPointF &screenPos);
    void sendDelayedMouseRelease(QObject *target, const QPointF &screenPos);
    bool isJumpMove(const QPointF &screenPos) const;
    bool isSameAsLast(const QPointF &screenPos) const;
    bool isEmiRebegin(const QPointF &screenPos, int touchCount) const;
    bool isFastRebegin(const QPointF &screenPos) const;
    bool isAttentionSameLineJump(const QPointF &screenPos) const;
    bool glueFastRebegin(QObject *watched, QTouchDevice *device, int touchId,
                         const QPointF &screenPos);
    bool consumeFreshContact(QObject *watched, QTouchDevice *device, int touchId,
                             const QPointF &screenPos);
    bool attentionNear(const QPointF &pos) const;
    bool attentionWide(const QPointF &pos) const;
    bool recentFarContact(const QPointF &screenPos) const;
    bool rejectRecentFarContact(const QPointF &screenPos);
    void armReleaseWait(QObject *watched, QTouchDevice *device, int touchId);
    bool armBeginWait(QObject *watched, QTouchDevice *device, int touchId,
                      const QPointF &screenPos);
    void commitReleaseWait();
    void confirmBeginWait(bool alsoRelease);
    void rejectBeginWait(const QPointF &screenPos);
    void onAttentionHoldTimeout();
    bool handleAttentionMouse(QObject *watched, QMouseEvent *mouse);
    bool handleAttentionTouch(QObject *watched, QTouchEvent *touch);
    void clearStableSlots();
    void dropStalePointer();
    bool noteStableMove(QObject *watched, QTouchEvent *touch);
    void beginPrimaryPick(QObject *watched, QTouchDevice *device, const QTouchEvent *touch);
    bool handlePrimaryPick(QObject *watched, QTouchEvent *touch);
    void deliverPickedPress(int touchId, const QPointF &screenPos, bool alsoRelease);
    void clearPrimaryPick();
    void freezePrimaryToLastAccepted(QTouchEvent *touch);
    void startQuarantine(QObject *watched, QTouchDevice *device, int touchId,
                         const QPointF &screenPos);
    bool handleQuarantineTouch(QObject *watched, QTouchEvent *touch);
    void dropQuarantine();
    void flushQuarantine();
    void sendDelayedTouchBegin(QObject *target, QTouchDevice *device, int touchId,
                               const QPointF &screenPos);
    void sendDelayedMousePress(QObject *target, const QPointF &screenPos);
    void expireStalePointerState();
    void writeTouchTrace(const QString &line);
    void writeTouchTrace(const QString &line, const QPointF &pos);
    void writeTouchTraceAt(const QString &line, const QPointF &pos, bool hasPos);
    void noteTrace(const QString &tag);
    void reportFalsePoint(const QPointF &screenPos);
    void setRecovering(bool on);
    // TEST: удалить после теста.
    void setTestRecoverAttempts(int count);
    void noteTestTouchLoss();
    void hideTestRecoverBadge();
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
    QTimer *m_startupCheckTimer = nullptr;
    QTimer *m_stitchTimer = nullptr;
    QTimer *m_confirmTimer = nullptr;
    QTimer *m_attnTimer = nullptr;
    // TEST: гасит квадрат попыток. Удалить после теста.
    QTimer *m_testRecoverHideTimer = nullptr;
    QElapsedTimer m_checkStarted;
    QElapsedTimer m_portOnSince;
    QElapsedTimer m_pointerDownSince;
    // Когда в интерфейс последний раз ушло живое движение. Старый захват
    // без таких кадров не должен глотать новое касание.
    QElapsedTimer m_lastLiveAt;
    QElapsedTimer m_postRecoverMuteSince;
    QElapsedTimer m_postActivationSince;
    QElapsedTimer m_releasedAt;
    QElapsedTimer m_lastFalseAt;
    QElapsedTimer m_traceClock;
    QFile m_traceFile;
    qint64 m_traceBytes = 0;
    QString m_traceTag;
    QString m_tracePrevLine;
    QPointF m_tracePrevPos;
    QElapsedTimer m_tracePrevAt;
    QElapsedTimer m_traceTouchAt;
    QPointF m_traceTouchPos;
    bool m_traceHasPrev = false;
    bool m_traceTouchPassed = false;
    bool m_traceInEvent = false;
    QString m_activeDisablePath;
    QString m_lastDisablePath;
    QPointF m_contactPos;
    QPointF m_lastAcceptedPos;
    QPointer<QObject> m_stitchTarget;
    QPointer<QObject> m_lastWindow;
    QPointer<QObject> m_quarantineTarget;
    QTouchDevice *m_stitchDevice = nullptr;
    QTouchDevice *m_quarantineDevice = nullptr;
    QPointF m_quarantinePos;
    QString m_state = QStringLiteral("ok");
    QString m_detail;
    QString m_lastLoggedKey;
    int m_cycle = 0;
    int m_portPhase = 0;
    int m_portProbeLeft = 0;
    int m_primaryTouchId = -1;
    // Слот, который реально зажал кнопку. Чужой id после обрыва его не подменяет.
    int m_liveTouchId = -1;
    int m_stitchTouchId = -1;
    int m_quarantineId = -1;
    bool m_ignoreForeignUntilClear = false;
    bool m_touchOk = true;
    bool m_faultActive = false;
    bool m_recovering = false;
    // TEST: попытки сброса порта и флаг «потеря уже была». Удалить после теста.
    int m_testRecoverAttempts = 0;
    bool m_testRecoverVisible = false;
    bool m_screenFaultShown = false;
    bool m_activationWasOn = false;
    bool m_pointerDown = false;
    bool m_ghostDown = false;
    QPointF m_ghostPos;
    bool m_gestureDirty = false;
    bool m_stitchPending = false;
    bool m_stitchOwesTouchEnd = false;
    bool m_stitchOwesMouseRelease = false;
    bool m_deliveringStitchRelease = false;
    bool m_quarantine = false;
    bool m_quarantineEnded = false;
    bool m_quarantineReject = false;
    bool m_falseSeen = false;
    // Предыдущий жест оборвался сбоем. Следующий Begin на той же полосе
    // не становится кликом, пока цепочка не затихнет. Чистый тап флаг снимает.
    bool m_chainRisk = false;
    bool m_attentionAnnounced = false;
    // На время внимания: Release — ждём, не ложный ли End; Begin — ещё не
    // решили, живое ли это нажатие.
    enum class AttnPhase { None, Release, Begin };
    AttnPhase m_attnPhase = AttnPhase::None;
    QPointF m_attnOrigin;
    QPointF m_attnPos;
    int m_attnTouchId = -1;
    QPointer<QObject> m_attnTarget;
    QTouchDevice *m_attnDevice = nullptr;
    // Пока палец ещё не зажат: слот, который два кадра подряд стоит на месте.
    struct StableSlot {
        int id = -1;
        QPointF pos;
        int seen = 0;
    };
    static constexpr int kStableSlotCount = 4;
    StableSlot m_stableSlot[kStableSlotCount];
    int m_stableCount = 0;
    QElapsedTimer m_stableAt;
    bool m_uiHasPress = false;
    // Живой слот уже отпущен. Дальше срок отпускания сдвигают только прыжки.
    bool m_liveTouchReleased = false;
    // После глушения на стекле ещё висит чужой кадр. Первая точка — только новый Begin.
    bool m_waitForLift = false;
    // В первом кадре два слота. Не берём p0, пока не видно, какой из них палец.
    bool m_pickingPrimary = false;
    int m_pickCount = 0;
    int m_pickId[2] = {-1, -1};
    QPointF m_pickStart[2];
    QPointF m_pickPos[2];
    QPointer<QObject> m_pickTarget;
    QTouchDevice *m_pickDevice = nullptr;
    QElapsedTimer m_pickSince;
    QPointF m_falsePoint;
    QElapsedTimer m_falsePointClock;
};

#endif // TOUCHUSBMONITOR_H
