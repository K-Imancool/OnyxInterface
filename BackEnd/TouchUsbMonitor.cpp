#include "TouchUsbMonitor.h"
#include "DeviceLogManager.h"
#include "apppaths.h"
#include "periphhandler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QMouseEvent>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTouchDevice>
#include <QTouchEvent>
#include <QTimer>
#include <QWindow>

namespace {

struct TouchUsbId {
    const char *vid;
    const char *pid;
};

constexpr TouchUsbId kTouchUsbIds[] = {
    {"222a", "0001"}, // ILITEK WINSTARTP
    {"04d8", "4931"}, // Microchip PIC16F1459 HID M-Touch
};

constexpr char kPortFileName[] = "/.config/UserInterface/touch-usb-port";
constexpr char kTraceFileName[] = "touch-trace.log";
constexpr char kTraceFallbackPath[] = "/tmp/onyx-touch-trace.log";
constexpr qint64 kTraceRotateBytes = 8 * 1024 * 1024;
constexpr int kHubBusFirst = 1;
constexpr int kHubBusLast = 7;

constexpr int kFaultDeadlineMs = 15000;
constexpr int kPortToggleMs = 2000;
constexpr int kPortPhaseOff = 1;
constexpr int kPortPhaseOn = 2;
constexpr int kPortPollMs = 250;
constexpr int kPortProbeIntervalMs = 500;
constexpr int kPortProbeTries = 20;
// Пауза после старта UI: на исправной плате 222a успевает перечислиться.
// Если мыши всё ещё нет — тот же сброс порта, что после активации.
constexpr int kStartupCheckDelayMs = 3000;
// TEST: сколько квадрат попыток виден после появления. Удалить после теста.
constexpr int kTestRecoverHideMs = 4000;
// После появления WINSTARTP Mouse usbhid часто шлёт ложный press.
constexpr qint64 kPostRecoverMuteMs = 500;
// Живой свайп в логе — не больше ~55 px и ~9 px по Y за кадр.
// Выше этого при зажатом пальце точку не двигаем.
constexpr qreal kMaxDxPx = 60.0;
constexpr qreal kMaxDyPx = 40.0;
// Цепочка EMI 02.11.26 (11:45): после обрыва первого слота новые Begin
// идут по полосе y±34 px, Δx часто 60…145, пауза до ~280 мс.
// Δy 16…25 рвал склейку, отдельный призрак был с Δy 51.
// «+»/«−» ~250 px по X. Их не держим, если предыдущий жест был чистым.
constexpr qreal kSameLineDxPx = 50.0;
constexpr qreal kSameLineDyPx = 50.0;
// Точка ещё нажата и отпускается примерно через это время после конца
// живого слота или последнего прыжка. Кадры чужого слота срок не сдвигают,
// иначе залипший призрак держит кнопку, пока помехи не кончатся.
constexpr qint64 kDirtyReleaseStitchMs = 400;
// Эксперимент: false — не держать палец и не отпускать его по таймеру.
// Прыжок и настоящий End уходят в интерфейс сразу. Вернуть склейку: true.
constexpr bool kStitchEnabled = false;
// Не раньше минуты после ACT. После минуты режим ещё держится,
// пока ложные касания не прекратились.
constexpr qint64 kAttentionMs = 60000;
constexpr qint64 kFalseQuietMs = 3000;
// Следующее звено приходило позже 80 мс, и карантин успевал подтвердить призрак.
// Ждём дольше максимальной паузы в том логе и только потом отдаём одиночный тап.
constexpr qint64 kWatchRejoinMs = 400;
// End и новый Begin за 1…30 мс — тот же палец. Человек так «+»/«−» не нажимает.
constexpr qint64 kFastRebeginMs = 50;
constexpr qreal kFastRebeginMinDxPx = 30.0;
// Повышенная готовность после ACT. End не отпускает палец сразу:
// за этот срок на той же полосе может вернуться тот же контакт.
// Короткий срок, чтобы «+»/«−» после отпускания не делали лишние шаги.
// Move дальше 25 px по X — помеха. Новый Begin подтверждаем по следующему кадру.
constexpr qint64 kAttentionHoldMs = 40;
constexpr qreal kAttentionNearPx = 10.0;
constexpr qreal kAttentionMaxDxPx = 25.0;
// Если release потерян — не держим состояние вечно.
constexpr qint64 kPointerLockTimeoutMs = 30000;

QString readSysfsText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed().toLower();
}

QString rememberedPortFile()
{
    return QDir::homePath() + QString::fromLatin1(kPortFileName);
}

QByteArray inputDevicesRaw()
{
    QFile devices(QStringLiteral("/proc/bus/input/devices"));
    if (!devices.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return devices.readAll();
}

bool isKnownTouchUsb(const QString &vid, const QString &pid)
{
    for (const TouchUsbId &id : kTouchUsbIds) {
        if (vid == QLatin1String(id.vid) && pid == QLatin1String(id.pid)) {
            return true;
        }
    }
    return false;
}

bool touchPointHeld(Qt::TouchPointState state)
{
    return state == Qt::TouchPointPressed
            || state == Qt::TouchPointMoved
            || state == Qt::TouchPointStationary;
}

// Ближайший по X удержанный слот на той же горизонтали. Чужой слот выше
// порога по Y сюда не попадает: это уже другое место, не возврат пальца.
const QTouchEvent::TouchPoint *closestSameLineHeld(
        const QList<QTouchEvent::TouchPoint> &points,
        const QPointF &origin,
        int skipId,
        qreal *dxOut)
{
    const QTouchEvent::TouchPoint *keep = nullptr;
    qreal keepDx = 0;
    for (const auto &point : points) {
        if (!touchPointHeld(point.state())) {
            continue;
        }
        if (skipId >= 0 && point.id() == skipId) {
            continue;
        }
        const qreal dx = qAbs(point.screenPos().x() - origin.x());
        const qreal dy = qAbs(point.screenPos().y() - origin.y());
        if (dy > kSameLineDyPx) {
            continue;
        }
        if (!keep || dx < keepDx) {
            keep = &point;
            keepDx = dx;
        }
    }
    if (dxOut) {
        *dxOut = keepDx;
    }
    return keep;
}

QList<QTouchEvent::TouchPoint> &mutableTouchPoints(QTouchEvent *touch)
{
    return const_cast<QList<QTouchEvent::TouchPoint> &>(touch->touchPoints());
}

bool keepOnlyTouchId(QTouchEvent *touch, int id)
{
    auto &points = mutableTouchPoints(touch);
    bool found = false;
    for (int i = points.size() - 1; i >= 0; --i) {
        if (points.at(i).id() == id) {
            found = true;
        } else {
            points.removeAt(i);
        }
    }
    return found;
}

const QTouchEvent::TouchPoint *firstHeldPoint(const QList<QTouchEvent::TouchPoint> &points)
{
    for (const auto &point : points) {
        if (touchPointHeld(point.state())) {
            return &point;
        }
    }
    return nullptr;
}

// Прижимаем точку к последней хорошей (X и Y): EMI не должен уводить Y.
void clampTouchPointTo(QTouchEvent::TouchPoint &point, const QPointF &accepted)
{
    const QPointF delta = accepted - point.screenPos();
    if (qAbs(delta.x()) < 0.5 && qAbs(delta.y()) < 0.5) {
        return;
    }
    point.setScreenPos(point.screenPos() + delta);
    point.setScenePos(point.scenePos() + delta);
    point.setPos(point.pos() + delta);
    point.setLastScreenPos(point.lastScreenPos() + delta);
    point.setLastScenePos(point.lastScenePos() + delta);
    point.setLastPos(point.lastPos() + delta);
}

void clampTouchEventTo(QTouchEvent *touch, int id, const QPointF &accepted)
{
    if (id < 0) {
        return;
    }
    auto &points = mutableTouchPoints(touch);
    for (QTouchEvent::TouchPoint &point : points) {
        if (point.id() == id) {
            clampTouchPointTo(point, accepted);
        }
    }
}

const QTouchEvent::TouchPoint *eventPrimaryPoint(const QList<QTouchEvent::TouchPoint> &points)
{
    const QTouchEvent::TouchPoint *first = firstHeldPoint(points);
    if (!first && !points.isEmpty()) {
        first = &points.first();
    }
    return first;
}

QPointF windowPosFromScreen(QObject *target, const QPointF &screenPos)
{
    if (auto *window = qobject_cast<QWindow *>(target)) {
        return QPointF(window->mapFromGlobal(screenPos.toPoint()));
    }
    return screenPos;
}

QString pointerTraceLabel(const QEvent *event, QPointF *pos, bool *hasPos)
{
    *hasPos = false;
    if (!event) {
        return {};
    }
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel: {
        const auto *touch = static_cast<const QTouchEvent *>(event);
        const auto &points = touch->touchPoints();
        const QTouchEvent::TouchPoint *primary = eventPrimaryPoint(points);
        if (primary) {
            *pos = primary->screenPos();
            *hasPos = true;
        }
        QString label;
        switch (event->type()) {
        case QEvent::TouchBegin:
            label = QStringLiteral("Begin");
            break;
        case QEvent::TouchUpdate:
            label = QStringLiteral("Move");
            break;
        case QEvent::TouchEnd:
            label = QStringLiteral("End");
            break;
        default:
            label = QStringLiteral("Cancel");
            break;
        }
        for (const auto &point : points) {
            if (primary && point.id() == primary->id()) {
                continue;
            }
            label += QStringLiteral(" p %1,%2")
                    .arg(qRound(point.screenPos().x()))
                    .arg(qRound(point.screenPos().y()));
        }
        return label;
    }
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove: {
        const auto *mouse = static_cast<const QMouseEvent *>(event);
        *pos = mouse->screenPos();
        *hasPos = true;
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            return QStringLiteral("Press");
        case QEvent::MouseButtonDblClick:
            return QStringLiteral("Dbl");
        case QEvent::MouseButtonRelease:
            return QStringLiteral("Release");
        default:
            return QStringLiteral("Move");
        }
    }
    default:
        return {};
    }
}

QString touchInputPresentDetail()
{
    const QByteArray text = inputDevicesRaw();
    if (text.contains("PIC16F1459")) {
        return QStringLiteral("PIC16F1459 present");
    }
    if (text.contains("WINSTARTP Mouse")) {
        return QStringLiteral("WINSTARTP Mouse present");
    }
    return {};
}

} // namespace

void releaseSceneGrab(QObject *target);
void sendTouchCancel(QObject *target, QTouchDevice *device);

TouchUsbMonitor::TouchUsbMonitor(DeviceLogManager *deviceLog, QObject *parent)
    : QObject(parent)
    , m_deviceLog(deviceLog)
    , m_stepTimer(new QTimer(this))
    , m_deadlineTimer(new QTimer(this))
    , m_portProbeTimer(new QTimer(this))
    , m_startupCheckTimer(new QTimer(this))
    , m_stitchTimer(new QTimer(this))
    , m_confirmTimer(new QTimer(this))
    , m_attnTimer(new QTimer(this))
    , m_testRecoverHideTimer(new QTimer(this))
{
    m_stepTimer->setSingleShot(true);
    connect(m_stepTimer, &QTimer::timeout, this, &TouchUsbMonitor::onPortStep);
    m_deadlineTimer->setSingleShot(true);
    connect(m_deadlineTimer, &QTimer::timeout, this, &TouchUsbMonitor::onRecoverDeadline);

    m_portProbeTimer->setInterval(kPortProbeIntervalMs);
    connect(m_portProbeTimer, &QTimer::timeout, this, &TouchUsbMonitor::probeTouchPort);

    m_startupCheckTimer->setSingleShot(true);
    connect(m_startupCheckTimer, &QTimer::timeout, this, &TouchUsbMonitor::beginStartupCheck);

    m_stitchTimer->setSingleShot(true);
    connect(m_stitchTimer, &QTimer::timeout, this, &TouchUsbMonitor::commitReleaseStitch);

    m_confirmTimer->setSingleShot(true);
    connect(m_confirmTimer, &QTimer::timeout, this, &TouchUsbMonitor::flushQuarantine);

    m_attnTimer->setSingleShot(true);
    connect(m_attnTimer, &QTimer::timeout, this, &TouchUsbMonitor::onAttentionHoldTimeout);

    // TEST: удалить после теста вместе с квадратом в main.qml.
    m_testRecoverHideTimer->setSingleShot(true);
    connect(m_testRecoverHideTimer, &QTimer::timeout, this, &TouchUsbMonitor::hideTestRecoverBadge);

    if (QCoreApplication *app = QCoreApplication::instance()) {
        app->installEventFilter(this);
    }

    m_traceClock.start();
    const QString logDir = AppPaths::instance().onyxLogDir();
    QDir().mkpath(logDir);
    QString tracePath = logDir + QLatin1Char('/') + QLatin1String(kTraceFileName);
    m_traceFile.setFileName(tracePath);
    if (!m_traceFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text
                          | QIODevice::Unbuffered)) {
        qWarning().noquote() << "TouchUsbMonitor: cannot open" << tracePath
                             << m_traceFile.errorString();
        tracePath = QString::fromLatin1(kTraceFallbackPath);
        m_traceFile.setFileName(tracePath);
        if (!m_traceFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text
                              | QIODevice::Unbuffered)) {
            qWarning().noquote() << "TouchUsbMonitor: cannot open" << tracePath
                                 << m_traceFile.errorString();
        }
    }
    if (m_traceFile.isOpen()) {
        m_traceBytes = m_traceFile.size();
        qWarning().noquote() << "TouchUsbMonitor: touch trace" << m_traceFile.fileName();
        writeTouchTrace(QStringLiteral("START file=%1 present=%2")
                        .arg(m_traceFile.fileName(), touchInputPresentDetail()));
    }

    loadRememberedPort();
    m_portProbeLeft = kPortProbeTries;
    probeTouchPort();
    if (m_portProbeTimer && m_portProbeLeft > 0) {
        m_portProbeTimer->start();
    }

    // Узел уже есть — порт не трогаем. Иначе даём USB/HID доперечислиться,
    // затем тот же сброс, что после активации (ILITEK 222a иногда «молчит» до power-cycle).
    const QString present = touchInputPresentDetail();
    if (!present.isEmpty()) {
        applyState(QStringLiteral("ok"), present, false);
    } else {
        qWarning("TouchUsbMonitor: touch input missing at startup, waiting to settle");
        m_startupCheckTimer->start(kStartupCheckDelayMs);
    }
}

void TouchUsbMonitor::setPeriphHandler(PeriphHandler *periph)
{
    if (m_periph == periph) {
        return;
    }
    if (m_periph) {
        disconnect(m_periph, &PeriphHandler::activationChanged,
                   this, &TouchUsbMonitor::onActivationChanged);
    }
    m_periph = periph;
    if (m_periph) {
        connect(m_periph, &PeriphHandler::activationChanged,
                this, &TouchUsbMonitor::onActivationChanged);
        onActivationChanged(m_periph->activation());
    }
}

void TouchUsbMonitor::onActivationChanged(bool active)
{
    // На старте/стопе активации сбрасываем состояние фильтра.
    clearContactState();
    if (active) {
        m_activationWasOn = true;
        m_postActivationSince.invalidate();
        m_falseSeen = false;
        m_attentionAnnounced = false;
        // Не крутим USB-порт во время RF: если recover уже в disable=1 — сразу включить.
        abortRecoverForActivation();
        if (isActivationMute()) {
            qWarning("TouchUsbMonitor: mute pointer events during activation");
            writeTouchTrace(QStringLiteral("ACT on mute=1"));
        } else {
            qWarning("TouchUsbMonitor: AutoStartStop — pointer allowed (touch stops activation)");
            writeTouchTrace(QStringLiteral("ACT on mute=0 AutoStartStop"));
        }
        return;
    }
    if (!m_activationWasOn) {
        return;
    }
    writeTouchTrace(QStringLiteral("ACT off"));
    writeTouchTrace(QStringLiteral("WATCH on"));
    // Касание, начатое во время глушения, первой точкой не считаем.
    // WINSTARTP Mouse тач-кадров не шлёт, поэтому срок ждём по часам, не по слоту.
    m_waitForLift = true;
    if (!m_releasedAt.isValid()) {
        m_releasedAt.start();
    }
    m_activationWasOn = false;
    m_falseSeen = false;
    m_lastFalseAt.invalidate();
    m_attentionAnnounced = true;
    m_postActivationSince.start();
    beginPostActivationCheck();
}

bool TouchUsbMonitor::isRecoverMute() const
{
    if (m_recovering) {
        return true;
    }
    return m_postRecoverMuteSince.isValid()
            && m_postRecoverMuteSince.elapsed() < kPostRecoverMuteMs;
}

void TouchUsbMonitor::armPostRecoverMute()
{
    clearContactState();
    m_postRecoverMuteSince.start();
}

bool TouchUsbMonitor::isActivationMute() const
{
    if (!m_periph || !m_periph->activation()) {
        return false;
    }
    // AutoStartStop (ACC = 2): касание должно дойти до Activation.stopIfAccTouch().
    // В остальных режимах глушим весь pointer-ввод на время активации.
    constexpr int kAutoModeAutoStartStop = 2;
    for (int socketId = 0; socketId < 4; ++socketId) {
        if (m_periph->autoMode(socketId) == kAutoModeAutoStartStop) {
            return false;
        }
    }
    return true;
}

bool TouchUsbMonitor::isPointerEvent(QEvent::Type type) const
{
    switch (type) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        return true;
    default:
        return false;
    }
}

void TouchUsbMonitor::beginContact(const QPointF &screenPos)
{
    m_pointerDown = true;
    m_gestureDirty = false;
    m_liveTouchId = -1;
    m_liveTouchReleased = false;
    m_contactPos = screenPos;
    m_lastAcceptedPos = screenPos;
    m_pointerDownSince.start();
    m_lastLiveAt.start();
    m_uiHasPress = true;
    clearStableSlots();
}

void TouchUsbMonitor::endContact()
{
    m_pointerDown = false;
    m_uiHasPress = false;
    m_liveTouchId = -1;
    m_liveTouchReleased = false;
    m_releasedAt.start();
    m_lastLiveAt.invalidate();
    // Чистое отпускание: следующий тап не звено цепочки и не ждёт карантин.
    m_chainRisk = false;
    clearStableSlots();
}

void TouchUsbMonitor::clearContactState()
{
    if (m_stitchPending && !m_deliveringStitchRelease) {
        commitReleaseStitch();
    }
    if (m_stitchTimer) {
        m_stitchTimer->stop();
    }
    if (m_confirmTimer) {
        m_confirmTimer->stop();
    }
    const bool releaseUi = (m_pointerDown || m_uiHasPress)
            && m_attnPhase != AttnPhase::Release
            && !m_deliveringStitchRelease;
    QObject *releaseTarget = m_attnTarget.data();
    if (!releaseTarget) {
        releaseTarget = m_lastWindow.data();
    }
    const QPointF releasePos = m_lastAcceptedPos;
    if (m_attnPhase == AttnPhase::Release && !m_deliveringStitchRelease) {
        commitReleaseWait();
    } else if (m_attnTimer) {
        m_attnTimer->stop();
        m_attnPhase = AttnPhase::None;
    }
    if (releaseUi && releaseTarget) {
        m_deliveringStitchRelease = true;
        const int touchId = m_primaryTouchId >= 0 ? m_primaryTouchId : m_liveTouchId;
        if (touchId >= 0) {
            sendDelayedTouchEnd(releaseTarget, m_attnDevice, touchId, releasePos);
        }
        sendDelayedMouseRelease(releaseTarget, releasePos);
        releaseSceneGrab(releaseTarget);
        sendTouchCancel(releaseTarget, m_attnDevice);
        m_deliveringStitchRelease = false;
    }
    m_quarantine = false;
    m_quarantineEnded = false;
    m_quarantineReject = false;
    m_quarantineTarget.clear();
    m_quarantineDevice = nullptr;
    m_quarantineId = -1;
    m_pointerDown = false;
    m_gestureDirty = false;
    m_chainRisk = false;
    m_primaryTouchId = -1;
    m_liveTouchId = -1;
    m_liveTouchReleased = false;
    m_waitForLift = false;
    clearPrimaryPick();
    m_ignoreForeignUntilClear = false;
    m_contactPos = {};
    m_lastAcceptedPos = {};
    m_releasedAt.invalidate();
    m_lastLiveAt.invalidate();
    m_stitchTarget.clear();
    m_stitchDevice = nullptr;
    m_stitchTouchId = -1;
    m_stitchPending = false;
    m_stitchOwesTouchEnd = false;
    m_stitchOwesMouseRelease = false;
    m_attnPhase = AttnPhase::None;
    m_attnOrigin = {};
    m_attnPos = {};
    m_attnTouchId = -1;
    m_attnTarget.clear();
    m_attnDevice = nullptr;
    clearStableSlots();
}

bool TouchUsbMonitor::inAttentionWindow() const
{
    return m_postActivationSince.isValid()
            && m_postActivationSince.elapsed() < kAttentionMs;
}

bool TouchUsbMonitor::attentionActive() const
{
    if (!m_postActivationSince.isValid()) {
        return false;
    }
    // Минуту после активации режим не гаснет, даже если сбоя не было.
    if (m_postActivationSince.elapsed() < kAttentionMs) {
        return true;
    }
    // Дальше — только пока ложные касания ещё есть.
    return m_falseSeen
            && m_lastFalseAt.isValid()
            && m_lastFalseAt.elapsed() < kFalseQuietMs;
}

void TouchUsbMonitor::noteFalseTouch()
{
    if (!m_postActivationSince.isValid()) {
        return;
    }
    // После минуты учитываем ложь только пока режим ещё не погас.
    if (m_postActivationSince.elapsed() >= kAttentionMs && !attentionActive()) {
        return;
    }
    const bool first = !m_falseSeen;
    m_falseSeen = true;
    m_lastFalseAt.start();
    if (first) {
        writeTouchTrace(QStringLiteral("FALSE"));
    }
}

void TouchUsbMonitor::pollAttention()
{
    const bool on = attentionActive();
    if (on && !m_attentionAnnounced) {
        m_attentionAnnounced = true;
        writeTouchTrace(QStringLiteral("WATCH on"));
    } else if (!on && m_attentionAnnounced) {
        m_attentionAnnounced = false;
        writeTouchTrace(QStringLiteral("WATCH off"));
        if (m_quarantineReject) {
            dropQuarantine();
        } else if (m_quarantine) {
            flushQuarantine();
        }
        if (m_attnPhase == AttnPhase::Release) {
            commitReleaseWait();
        } else if (m_attnPhase == AttnPhase::Begin) {
            // Неподтверждённое нажатие не отдаём: отпускания после него уже не будет.
            rejectBeginWait(m_attnPos);
        } else if (m_pointerDown || m_uiHasPress) {
            // Минута кончилась на зажатом жесте. Иначе палец так и висит,
            // а короткий сброс уже не работает.
            releaseHeldPress();
        }
        clearStableSlots();
    }
}

bool TouchUsbMonitor::releaseStitchPending() const
{
    return m_stitchPending;
}

qint64 TouchUsbMonitor::releaseStitchDelayMs() const
{
    return kDirtyReleaseStitchMs;
}

void TouchUsbMonitor::markGestureDirty()
{
    const bool wasDirty = m_gestureDirty;
    m_gestureDirty = true;
    // Пока этот флаг жив, новый Begin на той же полосе не кликает сразу.
    m_chainRisk = true;
    noteFalseTouch();
    if (m_liveTouchId < 0 && m_primaryTouchId >= 0) {
        m_liveTouchId = m_primaryTouchId;
    }
    // Первый прыжок только заводит срок. Чужой кадр его больше не отодвигает:
    // иначе точка не отпускается, пока контроллер шлёт призрак.
    if (m_pointerDown && !m_deliveringStitchRelease && !m_stitchPending) {
        const int id = m_liveTouchId >= 0 ? m_liveTouchId : m_primaryTouchId;
        armReleaseStitch(m_lastWindow.data(), true, true, nullptr, id);
    }
    if (!wasDirty) {
        noteTrace(QStringLiteral("DIRTY"));
    }
}

void TouchUsbMonitor::armReleaseStitch(QObject *watched, bool oweTouchEnd, bool oweMouseRelease,
                                      QTouchDevice *device, int touchId)
{
    if (!kStitchEnabled) {
        return;
    }
    m_stitchPending = true;
    // Склейка — сбойный обрыв. После неё та же полоса ещё под подозрением.
    m_chainRisk = true;
    if (watched) {
        m_stitchTarget = watched;
    }
    if (device) {
        m_stitchDevice = device;
    }
    // Id, который уже держит UI, не подменяем слотом призрака.
    if (m_stitchTouchId < 0) {
        if (m_liveTouchId >= 0) {
            m_stitchTouchId = m_liveTouchId;
        } else if (touchId >= 0) {
            m_stitchTouchId = touchId;
        } else if (m_primaryTouchId >= 0) {
            m_stitchTouchId = m_primaryTouchId;
        }
    }
    if (oweTouchEnd) {
        m_stitchOwesTouchEnd = true;
    }
    if (oweMouseRelease) {
        m_stitchOwesMouseRelease = true;
    }
    const qint64 delayMs = releaseStitchDelayMs();
    m_stitchTimer->start(delayMs);
    noteTrace(QStringLiteral("STITCH"));
}

void TouchUsbMonitor::freezePrimaryToLastAccepted(QTouchEvent *touch)
{
    if (m_primaryTouchId < 0) {
        return;
    }
    keepOnlyTouchId(touch, m_primaryTouchId);
    clampTouchEventTo(touch, m_primaryTouchId, m_lastAcceptedPos);
}

bool TouchUsbMonitor::absorbContinuingTouch(QTouchEvent *touch)
{
    const auto &points = touch->touchPoints();
    const QTouchEvent::TouchPoint *first = eventPrimaryPoint(points);
    if (points.size() > 1) {
        noteFalseTouch();
    }
    if (first && !isSameAsLast(first->screenPos())) {
        reportFalsePoint(first->screenPos());
    }
    if (first && isJumpMove(first->screenPos())) {
        markGestureDirty();
    }
    // Begin призрака в UI не отдаём: MouseArea проверки уже держит первый
    // палец, и вторая точка проваливается в кнопку меню под оверлеем.
    if (releaseStitchPending()) {
        keepReleaseStitch(touch);
    }
    return true;
}

bool TouchUsbMonitor::releaseDeadlineSlides(const QPointF &screenPos, int touchId) const
{
    if (isJumpMove(screenPos)) {
        return true;
    }
    // Мышь без слота и чужой id удержание не продлевают.
    if (touchId < 0 || m_liveTouchReleased) {
        return false;
    }
    if (m_liveTouchId >= 0 && touchId != m_liveTouchId) {
        return false;
    }
    return true;
}

void TouchUsbMonitor::keepReleaseStitch(const QPointF &screenPos, int touchId)
{
    if (!m_stitchPending || !m_stitchTimer) {
        return;
    }
    if (!releaseDeadlineSlides(screenPos, touchId)) {
        return;
    }
    m_stitchTimer->start(kDirtyReleaseStitchMs);
}

void TouchUsbMonitor::keepReleaseStitch(const QTouchEvent *touch)
{
    if (!touch || !m_stitchPending || !m_stitchTimer) {
        return;
    }
    QPointF slidePos;
    int slideId = -1;
    bool slide = false;
    bool jumpChosen = false;
    for (const auto &point : touch->touchPoints()) {
        if (isJumpMove(point.screenPos())) {
            reportFalsePoint(point.screenPos());
            if (!jumpChosen) {
                slidePos = point.screenPos();
                slideId = point.id();
                slide = true;
                jumpChosen = true;
            }
            continue;
        }
        if (jumpChosen) {
            continue;
        }
        if (touchPointHeld(point.state())
                && releaseDeadlineSlides(point.screenPos(), point.id())) {
            slidePos = point.screenPos();
            slideId = point.id();
            slide = true;
        }
    }
    if (!slide) {
        return;
    }
    keepReleaseStitch(slidePos, slideId);
}

void TouchUsbMonitor::noteLiveTouchReleased(QTouchDevice *device)
{
    // Живой слот кончился. Срок один раз отсчитывается от этого кадра.
    // Следующие призраки его не отодвигают, прыжок — отодвигает.
    if (device && !m_stitchDevice) {
        m_stitchDevice = device;
    }
    if (!m_liveTouchReleased) {
        m_liveTouchReleased = true;
        if (m_stitchPending && m_stitchTimer) {
            m_stitchTimer->start(kDirtyReleaseStitchMs);
        }
    }
    if (m_pointerDown && !m_stitchPending && !m_deliveringStitchRelease) {
        const int id = m_liveTouchId >= 0 ? m_liveTouchId : m_primaryTouchId;
        armReleaseStitch(m_lastWindow.data(), true, true, device, id);
    }
}

void TouchUsbMonitor::sendDelayedTouchEnd(QObject *target, QTouchDevice *device, int touchId,
                                         const QPointF &screenPos)
{
    const QPointF local = windowPosFromScreen(target, screenPos);
    QTouchEvent::TouchPoint point(touchId);
    point.setState(Qt::TouchPointReleased);
    point.setScreenPos(screenPos);
    point.setScenePos(local);
    point.setPos(local);
    point.setLastScreenPos(screenPos);
    point.setLastScenePos(local);
    point.setLastPos(local);
    point.setStartScreenPos(m_contactPos);
    point.setPressure(0);

    QList<QTouchEvent::TouchPoint> points;
    points.append(point);
    QTouchEvent endEvent(QEvent::TouchEnd, device, Qt::NoModifier,
                         Qt::TouchPointReleased, points);
    if (auto *window = qobject_cast<QWindow *>(target)) {
        endEvent.setWindow(window);
    }
    QCoreApplication::sendEvent(target, &endEvent);
}

void TouchUsbMonitor::sendDelayedMouseRelease(QObject *target, const QPointF &screenPos)
{
    const QPointF local = windowPosFromScreen(target, screenPos);
    // Тот же источник, что у press, который Qt собрал из тача. Иначе
    // MouseArea проверки не снимает захват и следующий призрак проваливается вниз.
    // Press мог прийти и от Qt, и от нашего повтора. Снимаем оба, иначе
    // кружок остаётся зажатым и больше не перерисовывается.
    const Qt::MouseEventSource sources[] = {
        Qt::MouseEventSynthesizedByQt,
        Qt::MouseEventSynthesizedByApplication
    };
    for (Qt::MouseEventSource source : sources) {
        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, local, local, screenPos,
                                 Qt::LeftButton, Qt::NoButton, Qt::NoModifier,
                                 source);
        QCoreApplication::sendEvent(target, &releaseEvent);
    }
}

void releaseSceneGrab(QObject *target)
{
    auto *window = qobject_cast<QQuickWindow *>(target);
    if (!window) {
        return;
    }
    QQuickItem *grabber = window->mouseGrabberItem();
    if (!grabber) {
        return;
    }
    grabber->ungrabTouchPoints();
    // mouseUngrabEvent гасит pressed у MouseArea, даже если Release до неё не дошёл.
    grabber->ungrabMouse();
}

void sendTouchCancel(QObject *target, QTouchDevice *device)
{
    auto *window = qobject_cast<QWindow *>(target);
    if (!window) {
        return;
    }
    // TouchEnd по одному id не снимает остальные захваты. Cancel гасит все.
    QTouchEvent cancel(QEvent::TouchCancel, device, Qt::NoModifier);
    cancel.setWindow(window);
    QCoreApplication::sendEvent(window, &cancel);
}

void TouchUsbMonitor::commitReleaseStitch()
{
    if (m_deliveringStitchRelease) {
        return;
    }
    const bool oweTouch = m_stitchOwesTouchEnd;
    const bool oweMouse = m_stitchOwesMouseRelease;
    QObject *target = m_stitchTarget.data();
    if (!target) {
        target = m_lastWindow.data();
    }
    QTouchDevice *device = m_stitchDevice;
    const int touchId = m_stitchTouchId;
    const QPointF screen = m_lastAcceptedPos;

    m_stitchTimer->stop();
    m_stitchPending = false;
    m_stitchOwesTouchEnd = false;
    m_stitchOwesMouseRelease = false;
    m_stitchTarget.clear();
    m_stitchDevice = nullptr;
    m_stitchTouchId = -1;

    writeTouchTrace(QStringLiteral("STITCH commit"), screen);

    m_deliveringStitchRelease = true;
    if (target && oweTouch && touchId >= 0) {
        sendDelayedTouchEnd(target, device, touchId, screen);
    }
    // Вшитый TouchEnd сам по себе не снимает синтезированную кнопку.
    if (target && (oweMouse || m_uiHasPress || oweTouch)) {
        sendDelayedMouseRelease(target, screen);
    }
    // Release мог не дойти до MouseArea. Снимаем захват явно, иначе
    // следующий призрак проваливается в кнопку под оверлеем.
    releaseSceneGrab(target);
    m_deliveringStitchRelease = false;

    m_pointerDown = false;
    m_uiHasPress = false;
    m_gestureDirty = false;
    m_primaryTouchId = -1;
    m_liveTouchId = -1;
    m_liveTouchReleased = false;
    m_ignoreForeignUntilClear = false;
    m_releasedAt.start();
}

void TouchUsbMonitor::expireStalePointerState()
{
    if (m_pointerDown
            && m_pointerDownSince.isValid()
            && m_pointerDownSince.elapsed() > kPointerLockTimeoutMs) {
        releaseHeldPress();
        // Хвост того же слота не должен сразу зажать экран снова.
        m_waitForLift = true;
    }
}

bool TouchUsbMonitor::isJumpMove(const QPointF &screenPos) const
{
    if (!m_pointerDown) {
        return false;
    }
    return qAbs(screenPos.x() - m_lastAcceptedPos.x()) > kMaxDxPx
            || qAbs(screenPos.y() - m_lastAcceptedPos.y()) > kMaxDyPx;
}

bool TouchUsbMonitor::isSameAsLast(const QPointF &screenPos) const
{
    return qAbs(screenPos.x() - m_lastAcceptedPos.x()) < 1.0
            && qAbs(screenPos.y() - m_lastAcceptedPos.y()) < 1.0;
}

bool TouchUsbMonitor::isEmiRebegin(const QPointF &screenPos, int touchCount) const
{
    // Продолжение грязного удержания. Палец логически ещё зажат.
    if (!m_pointerDown) {
        return false;
    }
    // Пока склейка не отпустила палец, новый Begin — не клик, в любой точке.
    // Иначе Δy 77 (548,390 при замороженных 754,467) рвёт склейку и нажимает
    // кнопку меню, которая лежит под оверлеем проверки.
    if (m_stitchPending) {
        return true;
    }
    // Второй слот — не новый палец.
    if (touchCount > 1) {
        return true;
    }
    const qreal dy = qAbs(screenPos.y() - m_lastAcceptedPos.y());
    if (dy > kSameLineDyPx) {
        return false;
    }
    // Повтор доставки того же Begin (Δ≈0) нельзя глотать: окно его ещё ждёт.
    return !isSameAsLast(screenPos);
}

bool TouchUsbMonitor::isFastRebegin(const QPointF &screenPos) const
{
    // End и сразу новый Begin на той же полосе. До активации так не режем.
    if (!attentionActive() || m_pointerDown || !m_releasedAt.isValid()) {
        return false;
    }
    if (m_releasedAt.elapsed() > kFastRebeginMs) {
        return false;
    }
    const qreal dy = qAbs(screenPos.y() - m_lastAcceptedPos.y());
    return dy <= kSameLineDyPx && !isSameAsLast(screenPos);
}

bool TouchUsbMonitor::isAttentionSameLineJump(const QPointF &screenPos) const
{
    // Цепочка кликов вдоль полосы — только если предыдущий жест уже был сбойным.
    // Чистый «+»/«−» (chainRisk снят) проходит сразу, даже на той же высоте.
    if (!attentionActive() || !m_chainRisk || m_pointerDown || !m_releasedAt.isValid()) {
        return false;
    }
    if (m_releasedAt.elapsed() > kWatchRejoinMs) {
        return false;
    }
    const qreal dx = qAbs(screenPos.x() - m_lastAcceptedPos.x());
    const qreal dy = qAbs(screenPos.y() - m_lastAcceptedPos.y());
    return dx >= kSameLineDxPx && dy <= kSameLineDyPx;
}

bool TouchUsbMonitor::glueFastRebegin(QObject *watched, QTouchDevice *device, int touchId,
                                      const QPointF &screenPos)
{
    if (!isFastRebegin(screenPos)) {
        return false;
    }
    const qreal dx = qAbs(screenPos.x() - m_lastAcceptedPos.x());
    noteFalseTouch();
    m_chainRisk = true;
    // Почти та же точка сразу после End — дребезг, второй клик не делаем.
    if (dx < kFastRebeginMinDxPx) {
        noteTrace(QStringLiteral("BOUNCE"));
        return true;
    }
    // Телепорт сразу после End — не палец. Кружок заново не зажимаем:
    // иначе он зависает на старой точке и перестаёт следовать за касанием.
    Q_UNUSED(watched);
    Q_UNUSED(device);
    Q_UNUSED(touchId);
    noteTrace(QStringLiteral("SKIP"));
    reportFalsePoint(screenPos);
    return true;
}

void TouchUsbMonitor::clearPrimaryPick()
{
    m_pickingPrimary = false;
    m_pickCount = 0;
    m_pickId[0] = -1;
    m_pickId[1] = -1;
    m_pickTarget.clear();
    m_pickDevice = nullptr;
}

void TouchUsbMonitor::beginPrimaryPick(QObject *watched, QTouchDevice *device, const QTouchEvent *touch)
{
    clearPrimaryPick();
    m_pickingPrimary = true;
    m_pickSince.start();
    m_pickTarget = qobject_cast<QWindow *>(watched) ? watched : m_lastWindow.data();
    m_pickDevice = device;
    if (!touch) {
        return;
    }
    for (const auto &point : touch->touchPoints()) {
        if (!touchPointHeld(point.state()) || m_pickCount >= 2) {
            continue;
        }
        if (attentionActive()
                && (recentFarContact(point.screenPos()) || isFastRebegin(point.screenPos())
                    || isAttentionSameLineJump(point.screenPos()))) {
            reportFalsePoint(point.screenPos());
            continue;
        }
        m_pickId[m_pickCount] = point.id();
        m_pickStart[m_pickCount] = point.screenPos();
        m_pickPos[m_pickCount] = point.screenPos();
        ++m_pickCount;
    }
    noteTrace(QStringLiteral("PICK"));
}

void TouchUsbMonitor::deliverPickedPress(int touchId, const QPointF &screenPos, bool alsoRelease)
{
    if (attentionActive()
            && (recentFarContact(screenPos) || isFastRebegin(screenPos)
                || isAttentionSameLineJump(screenPos))) {
        reportFalsePoint(screenPos);
        noteFalseTouch();
        noteTrace(QStringLiteral("GHOST"));
        clearPrimaryPick();
        return;
    }
    QObject *target = m_pickTarget.data();
    if (!target) {
        target = m_lastWindow.data();
    }
    QTouchDevice *device = m_pickDevice;
    clearPrimaryPick();
    noteTrace(QStringLiteral("PICK"));
    beginContact(screenPos);
    m_primaryTouchId = touchId;
    m_liveTouchId = touchId;
    m_uiHasPress = true;
    m_deliveringStitchRelease = true;
    if (target) {
        if (touchId >= 0) {
            sendDelayedTouchBegin(target, device, touchId, screenPos);
        }
        sendDelayedMousePress(target, screenPos);
        if (alsoRelease) {
            if (touchId >= 0) {
                sendDelayedTouchEnd(target, device, touchId, screenPos);
            }
            sendDelayedMouseRelease(target, screenPos);
        }
    }
    m_deliveringStitchRelease = false;
    if (alsoRelease) {
        endContact();
        m_primaryTouchId = -1;
    }
}

bool TouchUsbMonitor::handlePrimaryPick(QObject *watched, QTouchEvent *touch)
{
    Q_UNUSED(watched);
    if (!m_pickingPrimary || !touch) {
        return false;
    }
    if (touch->type() == QEvent::TouchBegin) {
        // Тот же Begin уходит нескольким объектам. Это ещё не выбор слота.
        return true;
    }
    if (touch->device()) {
        m_pickDevice = touch->device();
    }

    bool held[2] = {false, false};
    for (const auto &point : touch->touchPoints()) {
        for (int i = 0; i < m_pickCount; ++i) {
            if (point.id() != m_pickId[i]) {
                continue;
            }
            m_pickPos[i] = point.screenPos();
            held[i] = touchPointHeld(point.state());
        }
    }

    const auto travelJump = [this](int index) {
        return qAbs(m_pickPos[index].x() - m_pickStart[index].x()) > kMaxDxPx
                || qAbs(m_pickPos[index].y() - m_pickStart[index].y()) > kMaxDyPx;
    };
    const auto travel = [this](int index) {
        const qreal dx = m_pickPos[index].x() - m_pickStart[index].x();
        const qreal dy = m_pickPos[index].y() - m_pickStart[index].y();
        return dx * dx + dy * dy;
    };

    int alive = -1;
    int aliveCount = 0;
    for (int i = 0; i < m_pickCount; ++i) {
        if (!held[i]) {
            continue;
        }
        alive = i;
        ++aliveCount;
    }

    const auto reportOthers = [this](int keep) {
        for (int i = 0; i < m_pickCount; ++i) {
            if (i == keep) {
                continue;
            }
            reportFalsePoint(m_pickPos[i]);
        }
    };

    if (aliveCount == 1) {
        reportOthers(alive);
        deliverPickedPress(m_pickId[alive], m_pickPos[alive], false);
        return true;
    }
    if (aliveCount >= 2) {
        const bool jump0 = travelJump(0);
        const bool jump1 = m_pickCount > 1 && travelJump(1);
        if (jump0 != jump1) {
            const int keep = jump0 ? 1 : 0;
            reportOthers(keep);
            deliverPickedPress(m_pickId[keep], m_pickPos[keep], false);
            return true;
        }
        if (m_pickSince.isValid() && m_pickSince.elapsed() >= 80) {
            const int keep = (m_pickCount > 1 && travel(1) < travel(0)) ? 1 : 0;
            reportOthers(keep);
            deliverPickedPress(m_pickId[keep], m_pickPos[keep], false);
        }
        return true;
    }

    // Оба слота уже отпущены, палец так и не выделился.
    if (m_pickCount > 0) {
        const int best = (m_pickCount > 1 && travel(1) < travel(0)) ? 1 : 0;
        if (!travelJump(best)) {
            reportOthers(best);
            deliverPickedPress(m_pickId[best], m_pickPos[best], true);
        } else {
            reportOthers(-1);
            noteTrace(QStringLiteral("PICK drop"));
            clearPrimaryPick();
        }
    } else {
        clearPrimaryPick();
    }
    return true;
}

bool TouchUsbMonitor::consumeFreshContact(QObject *watched, QTouchDevice *device, int touchId,
                                          const QPointF &screenPos)
{
    if (m_pointerDown) {
        return false;
    }
    // Обычный путь, не только ожидание нового нажатия: сразу после End
    // точка дальше ±10 не становится пальцем.
    if (rejectRecentFarContact(screenPos)) {
        return true;
    }
    // После грязного жеста далеко прыгнувшая точка в ближайшие ~400 мс — не палец.
    // Близкий кадр не глотаем: это тот же палец, кружок должен за ним идти.
    if (attentionActive() && m_chainRisk && m_releasedAt.isValid()) {
        if (m_releasedAt.elapsed() <= kWatchRejoinMs) {
            const qreal dx = qAbs(screenPos.x() - m_lastAcceptedPos.x());
            const qreal dy = qAbs(screenPos.y() - m_lastAcceptedPos.y());
            if (dx > kMaxDxPx || dy > kMaxDyPx) {
                noteFalseTouch();
                m_releasedAt.start();
                noteTrace(QStringLiteral("GHOST"));
                reportFalsePoint(screenPos);
                return true;
            }
        } else {
            m_chainRisk = false;
        }
    }
    if (glueFastRebegin(watched, device, touchId, screenPos)) {
        return true;
    }
    if (isAttentionSameLineJump(screenPos)) {
        startQuarantine(watched, device, touchId, screenPos);
        return true;
    }
    return false;
}

void TouchUsbMonitor::sendDelayedTouchBegin(QObject *target, QTouchDevice *device, int touchId,
                                           const QPointF &screenPos)
{
    const QPointF local = windowPosFromScreen(target, screenPos);
    QTouchEvent::TouchPoint point(touchId);
    point.setState(Qt::TouchPointPressed);
    point.setScreenPos(screenPos);
    point.setScenePos(local);
    point.setPos(local);
    point.setLastScreenPos(screenPos);
    point.setLastScenePos(local);
    point.setLastPos(local);
    point.setStartScreenPos(screenPos);
    point.setPressure(1);

    QList<QTouchEvent::TouchPoint> points;
    points.append(point);
    QTouchEvent beginEvent(QEvent::TouchBegin, device, Qt::NoModifier,
                           Qt::TouchPointPressed, points);
    if (auto *window = qobject_cast<QWindow *>(target)) {
        beginEvent.setWindow(window);
    }
    QCoreApplication::sendEvent(target, &beginEvent);
}

void TouchUsbMonitor::sendDelayedMousePress(QObject *target, const QPointF &screenPos)
{
    const QPointF local = windowPosFromScreen(target, screenPos);
    QMouseEvent pressEvent(QEvent::MouseButtonPress, local, local, screenPos,
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier,
                           Qt::MouseEventSynthesizedByApplication);
    QCoreApplication::sendEvent(target, &pressEvent);
}

void TouchUsbMonitor::startQuarantine(QObject *watched, QTouchDevice *device, int touchId,
                                     const QPointF &screenPos)
{
    if (qobject_cast<QWindow *>(watched)) {
        m_quarantineTarget = watched;
    } else if (!m_quarantineTarget) {
        m_quarantineTarget = m_lastWindow;
    }
    if (device) {
        m_quarantineDevice = device;
    }
    m_quarantineId = touchId;
    m_quarantinePos = screenPos;
    m_quarantine = true;
    m_quarantineEnded = false;
    m_quarantineReject = false;
    m_confirmTimer->start(kWatchRejoinMs);
    noteTrace(QStringLiteral("QUAR"));
    reportFalsePoint(screenPos);
}

void TouchUsbMonitor::dropQuarantine()
{
    if (!m_quarantine) {
        return;
    }
    m_confirmTimer->stop();
    m_quarantine = false;
    m_quarantineEnded = false;
    m_quarantineReject = false;
    const QPointF ghostPos = m_quarantinePos;
    m_releasedAt.start();
    if (m_traceInEvent) {
        noteTrace(QStringLiteral("GHOST"));
    } else {
        writeTouchTrace(QStringLiteral("GHOST"), ghostPos);
    }
    reportFalsePoint(ghostPos);
    m_quarantineTarget.clear();
    m_quarantineDevice = nullptr;
    m_quarantineId = -1;
    noteFalseTouch();
}

void TouchUsbMonitor::flushQuarantine()
{
    if (!m_quarantine || m_deliveringStitchRelease) {
        return;
    }
    if (m_quarantineReject) {
        if (!m_quarantineEnded) {
            m_confirmTimer->start(kWatchRejoinMs);
            return;
        }
        dropQuarantine();
        return;
    }
    const QPointF pos = m_quarantinePos;
    const int touchId = m_quarantineId;
    const bool ended = m_quarantineEnded;
    QObject *target = m_quarantineTarget.data();
    if (!target) {
        target = m_lastWindow.data();
    }
    QTouchDevice *device = m_quarantineDevice;

    m_confirmTimer->stop();
    m_quarantine = false;
    m_quarantineEnded = false;
    m_quarantineTarget.clear();
    m_quarantineDevice = nullptr;
    m_quarantineId = -1;

    writeTouchTrace(ended ? QStringLiteral("QUAR ok end") : QStringLiteral("QUAR ok"), pos);

    beginContact(pos);
    if (touchId >= 0) {
        m_primaryTouchId = touchId;
    }
    m_uiHasPress = true;
    m_deliveringStitchRelease = true;
    if (target) {
        if (touchId >= 0) {
            sendDelayedTouchBegin(target, device, touchId, pos);
        }
        sendDelayedMousePress(target, pos);
        if (ended) {
            if (touchId >= 0) {
                sendDelayedTouchEnd(target, device, touchId, pos);
            }
            sendDelayedMouseRelease(target, pos);
        }
    }
    m_deliveringStitchRelease = false;
    if (ended) {
        endContact();
        m_primaryTouchId = -1;
    }
}

bool TouchUsbMonitor::handleQuarantineTouch(QObject *watched, QTouchEvent *touch)
{
    if (qobject_cast<QWindow *>(watched)) {
        m_quarantineTarget = watched;
    }
    const QEvent::Type type = touch->type();
    const auto &points = touch->touchPoints();
    const QTouchEvent::TouchPoint *point = eventPrimaryPoint(points);

    if (!point) {
        if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
            m_quarantineEnded = true;
        }
        return true;
    }
    if (touch->device()) {
        m_quarantineDevice = touch->device();
    }

    const QPointF pos = point->screenPos();
    if (type == QEvent::TouchBegin) {
        const bool sameDelivery = qAbs(pos.x() - m_quarantinePos.x()) <= 1.0
                && qAbs(pos.y() - m_quarantinePos.y()) <= 1.0
                && (m_quarantineId < 0 || point->id() == m_quarantineId);
        if (sameDelivery) {
            if (m_quarantineId < 0) {
                m_quarantineId = point->id();
            }
            return true;
        }
        // Следующий Begin, пока кандидат ещё ждёт паузу цепочки, — предыдущий
        // был звеном, не тапом. Новый разберётся уже после сброса.
        dropQuarantine();
        return false;
    }
    if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
        if (m_quarantineReject) {
            dropQuarantine();
        } else {
            m_quarantineEnded = true;
        }
        return true;
    }
    if (m_quarantineId < 0) {
        m_quarantineId = point->id();
    }

    const qreal dyLast = qAbs(pos.y() - m_lastAcceptedPos.y());
    const qreal dyQuar = qAbs(pos.y() - m_quarantinePos.y());
    // Другая строка — живой тап, призрак выбрасываем.
    if (dyLast > kSameLineDyPx && dyQuar > kSameLineDyPx) {
        dropQuarantine();
        return false;
    }

    const qreal dx = qAbs(pos.x() - m_quarantinePos.x());
    const qreal dy = qAbs(pos.y() - m_quarantinePos.y());
    if (dx > kMaxDxPx || dy > kMaxDyPx) {
        // Внутри кандидата снова телепорт — это не палец, клик не отдаём.
        m_quarantineReject = true;
        noteFalseTouch();
    }
    return true;
}

bool TouchUsbMonitor::attentionNear(const QPointF &pos) const
{
    return qAbs(pos.x() - m_attnOrigin.x()) <= kAttentionNearPx
            && qAbs(pos.y() - m_attnOrigin.y()) <= kAttentionNearPx;
}

bool TouchUsbMonitor::attentionWide(const QPointF &pos) const
{
    return qAbs(pos.x() - m_attnOrigin.x()) > kAttentionMaxDxPx
            || qAbs(pos.y() - m_attnOrigin.y()) > kMaxDyPx;
}

bool TouchUsbMonitor::recentFarContact(const QPointF &screenPos) const
{
    if (!attentionActive() || m_pointerDown || !m_releasedAt.isValid()) {
        return false;
    }
    if (m_releasedAt.elapsed() > kAttentionHoldMs) {
        return false;
    }
    return qAbs(screenPos.x() - m_lastAcceptedPos.x()) > kAttentionNearPx
            || qAbs(screenPos.y() - m_lastAcceptedPos.y()) > kAttentionNearPx;
}

bool TouchUsbMonitor::rejectRecentFarContact(const QPointF &screenPos)
{
    if (!recentFarContact(screenPos)) {
        return false;
    }
    reportFalsePoint(screenPos);
    if (m_traceInEvent) {
        noteTrace(QStringLiteral("GHOST"));
    } else {
        writeTouchTrace(QStringLiteral("GHOST"), screenPos);
    }
    return true;
}

void TouchUsbMonitor::armReleaseWait(QObject *watched, QTouchDevice *device, int touchId)
{
    if (m_attnPhase == AttnPhase::Release) {
        return;
    }
    m_attnPhase = AttnPhase::Release;
    m_attnOrigin = m_lastAcceptedPos;
    m_attnPos = m_lastAcceptedPos;
    m_attnTouchId = touchId >= 0 ? touchId : m_primaryTouchId;
    if (qobject_cast<QWindow *>(watched)) {
        m_attnTarget = watched;
    } else if (!m_attnTarget) {
        m_attnTarget = m_lastWindow;
    }
    if (device) {
        m_attnDevice = device;
    }
    m_attnTimer->start(kAttentionHoldMs);
}

bool TouchUsbMonitor::armBeginWait(QObject *watched, QTouchDevice *device, int touchId,
                                   const QPointF &screenPos)
{
    // Сразу после настоящего тапа далёкая точка — не второй палец.
    if (rejectRecentFarContact(screenPos)) {
        noteFalseTouch();
        return false;
    }
    // Тот же палец после End приходит сюда, не в consumeFreshContact:
    // одиночный Begin на внимании до него не доходит. Чистый «+»/«−»
    // chainRisk не держит и проходит. Сбойная полоса — нет.
    if (glueFastRebegin(watched, device, touchId, screenPos)) {
        return false;
    }
    if (isAttentionSameLineJump(screenPos)) {
        reportFalsePoint(screenPos);
        noteFalseTouch();
        if (m_traceInEvent) {
            noteTrace(QStringLiteral("GHOST"));
        } else {
            writeTouchTrace(QStringLiteral("GHOST"), screenPos);
        }
        return false;
    }
    m_attnPhase = AttnPhase::Begin;
    m_attnOrigin = screenPos;
    m_attnPos = screenPos;
    m_attnTouchId = touchId;
    if (qobject_cast<QWindow *>(watched)) {
        m_attnTarget = watched;
    } else if (!m_attnTarget) {
        m_attnTarget = m_lastWindow;
    }
    if (device) {
        m_attnDevice = device;
    }
    m_attnTimer->start(kAttentionHoldMs);
    return true;
}

void TouchUsbMonitor::commitReleaseWait()
{
    if (m_attnPhase != AttnPhase::Release || m_deliveringStitchRelease) {
        return;
    }
    m_attnTimer->stop();
    m_attnPhase = AttnPhase::None;
    const QPointF pos = m_attnOrigin;
    const int touchId = m_attnTouchId >= 0 ? m_attnTouchId : m_primaryTouchId;
    QObject *target = m_attnTarget.data();
    if (!target) {
        target = m_lastWindow.data();
    }
    QTouchDevice *device = m_attnDevice;
    m_attnTouchId = -1;
    m_attnTarget.clear();
    m_attnDevice = nullptr;

    writeTouchTrace(QStringLiteral("HOLD release"), pos);

    m_deliveringStitchRelease = true;
    if (target && touchId >= 0) {
        sendDelayedTouchEnd(target, device, touchId, pos);
    }
    if (target) {
        sendDelayedMouseRelease(target, pos);
    }
    releaseSceneGrab(target);
    sendTouchCancel(target, device);
    m_deliveringStitchRelease = false;

    // endContact снимает chainRisk. После сбойного удержания полоса
    // ещё под подозрением: иначе следующий Begin на ней становится кликом.
    const bool keepChain = m_chainRisk;
    endContact();
    if (keepChain) {
        m_chainRisk = true;
    }
    m_primaryTouchId = -1;
    m_ignoreForeignUntilClear = false;
}

void TouchUsbMonitor::confirmBeginWait(bool alsoRelease)
{
    if (m_attnPhase != AttnPhase::Begin || m_deliveringStitchRelease) {
        return;
    }
    m_attnTimer->stop();
    m_attnPhase = AttnPhase::None;
    const QPointF pos = m_attnPos;
    const int touchId = m_attnTouchId;
    QObject *target = m_attnTarget.data();
    if (!target) {
        target = m_lastWindow.data();
    }
    QTouchDevice *device = m_attnDevice;
    m_attnTouchId = -1;
    m_attnTarget.clear();
    m_attnDevice = nullptr;

    if (!m_traceInEvent) {
        writeTouchTrace(alsoRelease ? QStringLiteral("TAP") : QStringLiteral("HOLD ok"), pos);
    }

    beginContact(pos);
    if (touchId >= 0) {
        m_primaryTouchId = touchId;
        m_liveTouchId = touchId;
    }
    m_uiHasPress = true;
    m_deliveringStitchRelease = true;
    // Только тач: из него Qt сам собирает одну мышь. Вторая мышь рядом
    // попадала уже в другой виджет — меню и ящик нейтрали открывались вместе.
    if (target && touchId >= 0) {
        sendDelayedTouchBegin(target, device, touchId, pos);
        if (alsoRelease) {
            sendDelayedTouchEnd(target, device, touchId, pos);
        }
    } else if (target) {
        sendDelayedMousePress(target, pos);
        if (alsoRelease) {
            sendDelayedMouseRelease(target, pos);
        }
    }
    m_deliveringStitchRelease = false;
    if (alsoRelease) {
        endContact();
        m_primaryTouchId = -1;
    }
}

void TouchUsbMonitor::rejectBeginWait(const QPointF &screenPos)
{
    if (m_attnPhase != AttnPhase::Begin) {
        return;
    }
    m_attnTimer->stop();
    m_attnPhase = AttnPhase::None;
    m_attnTouchId = -1;
    m_attnTarget.clear();
    m_attnDevice = nullptr;
    reportFalsePoint(screenPos);
    if (m_traceInEvent) {
        noteTrace(QStringLiteral("GHOST"));
    } else {
        writeTouchTrace(QStringLiteral("GHOST"), screenPos);
    }
}

void TouchUsbMonitor::onAttentionHoldTimeout()
{
    if (m_attnPhase == AttnPhase::Release) {
        commitReleaseWait();
    } else if (m_attnPhase == AttnPhase::Begin) {
        confirmBeginWait(false);
    }
}

void TouchUsbMonitor::dropStalePointer()
{
    // Жест числится зажатым, а в интерфейс нажатие не попадало.
    // Иначе проверка тача молчит, кнопка «Назад» при этом ещё нажимается.
    if (!m_pointerDown || m_uiHasPress || m_attnPhase == AttnPhase::Begin) {
        return;
    }
    if (m_attnTimer) {
        m_attnTimer->stop();
    }
    m_attnPhase = AttnPhase::None;
    endContact();
    m_releasedAt.invalidate();
    m_primaryTouchId = -1;
}

void TouchUsbMonitor::clearStableSlots()
{
    m_stableCount = 0;
    m_stableAt.invalidate();
}

bool TouchUsbMonitor::noteStableMove(QObject *watched, QTouchEvent *touch)
{
    // Палец ещё не зажат. Два кадра подряд в пределах порога прыжка — это палец.
    // Шаг дальше 25 px по X или 40 px по Y в выбор не входит.
    const auto &points = touch->touchPoints();
    // Повтор того же кадра на соседний QObject не считается вторым кадром.
    const bool echo = m_stableAt.isValid() && m_stableAt.elapsed() < 8;
    bool used[kStableSlotCount] = {};
    StableSlot next[kStableSlotCount];
    int nextCount = 0;
    int best = -1;
    qreal bestStep = 0;
    bool anyHeld = false;
    bool anyFresh = false;

    for (const auto &point : points) {
        if (!touchPointHeld(point.state())) {
            continue;
        }
        anyHeld = true;
        const QPointF at = point.screenPos();
        int prev = -1;
        for (int i = 0; i < m_stableCount; ++i) {
            if (!used[i] && m_stableSlot[i].id == point.id()) {
                prev = i;
                break;
            }
        }
        if (prev >= 0) {
            const qreal dx = qAbs(at.x() - m_stableSlot[prev].pos.x());
            const qreal dy = qAbs(at.y() - m_stableSlot[prev].pos.y());
            if (dx > kAttentionMaxDxPx || dy > kMaxDyPx) {
                used[prev] = true;
                reportFalsePoint(at);
                anyFresh = true;
                continue;
            }
        } else {
            qreal bestDist = qMax(kAttentionMaxDxPx, kMaxDyPx);
            for (int i = 0; i < m_stableCount; ++i) {
                if (used[i]) {
                    continue;
                }
                const qreal dx = qAbs(at.x() - m_stableSlot[i].pos.x());
                const qreal dy = qAbs(at.y() - m_stableSlot[i].pos.y());
                const qreal dist = qMax(dx, dy);
                if (dx <= kAttentionMaxDxPx && dy <= kMaxDyPx && dist <= bestDist) {
                    prev = i;
                    bestDist = dist;
                }
            }
        }

        int seen = 1;
        bool fresh = prev < 0;
        qreal step = 0;
        if (prev >= 0) {
            const qreal dx = qAbs(at.x() - m_stableSlot[prev].pos.x());
            const qreal dy = qAbs(at.y() - m_stableSlot[prev].pos.y());
            step = qMax(dx, dy);
            if (echo && dx < 1.0 && dy < 1.0) {
                seen = m_stableSlot[prev].seen;
            } else {
                // Дрожь больше ±10, но меньше порога прыжка — тот же палец.
                seen = m_stableSlot[prev].seen + 1;
                fresh = true;
            }
            used[prev] = true;
        }
        if (fresh) {
            anyFresh = true;
        }
        if (nextCount >= kStableSlotCount) {
            reportFalsePoint(at);
            continue;
        }
        next[nextCount] = StableSlot{point.id(), at, seen};
        if (seen >= 2 && !recentFarContact(at) && !isFastRebegin(at)
                && !isAttentionSameLineJump(at)
                && (best < 0 || seen > next[best].seen
                    || (seen == next[best].seen && step < bestStep))) {
            best = nextCount;
            bestStep = step;
        }
        ++nextCount;
    }

    if (best >= 0) {
        const int keepId = next[best].id;
        const QPointF pos = next[best].pos;
        for (const auto &point : points) {
            if (touchPointHeld(point.state()) && point.id() != keepId) {
                reportFalsePoint(point.screenPos());
            }
        }
        m_attnPhase = AttnPhase::Begin;
        m_attnOrigin = pos;
        m_attnPos = pos;
        m_attnTouchId = keepId;
        if (qobject_cast<QWindow *>(watched)) {
            m_attnTarget = watched;
        } else if (!m_attnTarget) {
            m_attnTarget = m_lastWindow;
        }
        if (touch->device()) {
            m_attnDevice = touch->device();
        }
        confirmBeginWait(false);
        noteTrace(QStringLiteral("STABLE"));
        return true;
    }

    if (anyFresh) {
        m_stableAt.start();
    }
    m_stableCount = nextCount;
    for (int i = 0; i < nextCount; ++i) {
        m_stableSlot[i] = next[i];
    }
    return anyHeld;
}

bool TouchUsbMonitor::handleAttentionMouse(QObject *watched, QMouseEvent *mouse)
{
    const QEvent::Type type = mouse->type();
    const QPointF pos = mouse->screenPos();
    const bool press = type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick;
    const bool release = type == QEvent::MouseButtonRelease;
    const bool move = type == QEvent::MouseMove;

    if (m_attnPhase == AttnPhase::Release) {
        if (release) {
            noteTrace(QStringLiteral("HOLD"));
            return true;
        }
        const qreal dx = qAbs(pos.x() - m_attnOrigin.x());
        const qreal dy = qAbs(pos.y() - m_attnOrigin.y());
        // Рядом — тот же палец, срок отпускания отменяем. Сдвиг по полосе
        // срок не трогает: иначе кружок остаётся зажатым, пока идут помехи.
        if ((press || move) && dx <= kAttentionMaxDxPx && dy <= kMaxDyPx) {
            m_attnTimer->stop();
            m_attnPhase = AttnPhase::None;
            m_pointerDown = true;
            m_uiHasPress = true;
            m_liveTouchReleased = false;
            m_lastAcceptedPos = pos;
            noteTrace(QStringLiteral("CONT"));
            return press;
        }
        if (press || move) {
            reportFalsePoint(pos);
            noteFalseTouch();
            noteTrace(press ? QStringLiteral("GHOST") : QStringLiteral("JUMP"));
            return true;
        }
        return true;
    }

    if (m_attnPhase == AttnPhase::Begin) {
        if (press) {
            noteTrace(QStringLiteral("HOLD"));
            return true;
        }
        if (release) {
            // Далёкий Release не оставляем висеть: иначе таймер потом
            // отдаст нажатие уже без пальца, и кружок не погаснет.
            if (attentionWide(pos)) {
                rejectBeginWait(pos);
                noteFalseTouch();
            } else {
                m_attnPos = pos;
                confirmBeginWait(true);
                noteTrace(QStringLiteral("TAP"));
            }
            return true;
        }
        if (move) {
            if (attentionWide(pos)) {
                reportFalsePoint(pos);
                noteFalseTouch();
                noteTrace(QStringLiteral("JUMP"));
                return true;
            }
            m_attnPos = pos;
            if (attentionNear(pos)) {
                confirmBeginWait(false);
            }
            return true;
        }
        return true;
    }

    if (!attentionActive()) {
        return false;
    }
    if (release && mouse->button() == Qt::LeftButton && m_pointerDown) {
        const qreal dx = qAbs(pos.x() - m_lastAcceptedPos.x());
        const qreal dy = qAbs(pos.y() - m_lastAcceptedPos.y());
        if (dx > kAttentionMaxDxPx || dy > kMaxDyPx) {
            reportFalsePoint(pos);
        }
        armReleaseWait(watched, nullptr, m_primaryTouchId);
        noteTrace(QStringLiteral("HOLD"));
        return true;
    }
    if (m_pointerDown && (press || move)) {
        const qreal dx = qAbs(pos.x() - m_lastAcceptedPos.x());
        const qreal dy = qAbs(pos.y() - m_lastAcceptedPos.y());
        if (dx > kAttentionMaxDxPx || dy > kMaxDyPx) {
            reportFalsePoint(pos);
            noteFalseTouch();
            m_chainRisk = true;
            armReleaseWait(watched, nullptr, m_primaryTouchId);
            noteTrace(QStringLiteral("JUMP"));
            return true;
        }
        if (press) {
            noteTrace(QStringLiteral("CONT"));
            return true;
        }
    }
    if (press && !m_pointerDown) {
        if (!armBeginWait(watched, nullptr, -1, pos)) {
            return true;
        }
        noteTrace(QStringLiteral("HOLD"));
        return true;
    }
    return false;
}

bool TouchUsbMonitor::handleAttentionTouch(QObject *watched, QTouchEvent *touch)
{
    dropStalePointer();
    const QEvent::Type type = touch->type();
    const auto &points = touch->touchPoints();
    const QTouchEvent::TouchPoint *held = firstHeldPoint(points);

    if (m_attnPhase == AttnPhase::Release) {
        // Смотрим все слоты, не только первый: призрак в начале списка
        // раньше прятал палец у исходной точки. Отпускание отменяет только
        // точка в пределах порога прыжка. Дальше по полосе срок не сдвигаем.
        qreal keepDx = 0;
        const QTouchEvent::TouchPoint *keep =
                closestSameLineHeld(points, m_attnOrigin, -1, &keepDx);
        const qreal keepDy = keep ? qAbs(keep->screenPos().y() - m_attnOrigin.y()) : 0;
        if (keep && keepDx <= kAttentionMaxDxPx && keepDy <= kMaxDyPx) {
            for (const auto &point : points) {
                if (touchPointHeld(point.state()) && point.id() != keep->id()) {
                    reportFalsePoint(point.screenPos());
                }
            }
            m_attnTimer->stop();
            m_attnPhase = AttnPhase::None;
            m_pointerDown = true;
            m_uiHasPress = true;
            m_primaryTouchId = keep->id();
            m_liveTouchId = keep->id();
            m_liveTouchReleased = false;
            m_lastAcceptedPos = keep->screenPos();
            noteTrace(QStringLiteral("CONT"));
            return type == QEvent::TouchBegin;
        }
        if (type == QEvent::TouchBegin && held) {
            reportFalsePoint(held->screenPos());
            noteFalseTouch();
            noteTrace(QStringLiteral("GHOST"));
            return true;
        }
        if (held) {
            reportFalsePoint(held->screenPos());
            noteFalseTouch();
            noteTrace(QStringLiteral("JUMP"));
            return true;
        }
        noteTrace(QStringLiteral("HOLD"));
        return true;
    }

    if (m_attnPhase == AttnPhase::Begin) {
        if (type == QEvent::TouchBegin) {
            if (m_attnTouchId >= 0) {
                for (const auto &point : points) {
                    if (point.id() != m_attnTouchId && touchPointHeld(point.state())) {
                        reportFalsePoint(point.screenPos());
                    }
                }
                noteTrace(QStringLiteral("HOLD"));
                return true;
            }
            if (held && m_attnTouchId < 0) {
                m_attnTouchId = held->id();
            }
            noteTrace(QStringLiteral("HOLD"));
            return true;
        }
        const QTouchEvent::TouchPoint *ours = nullptr;
        if (m_attnTouchId >= 0) {
            for (const auto &point : points) {
                if (point.id() == m_attnTouchId) {
                    ours = &point;
                    break;
                }
            }
        }
        for (const auto &point : points) {
            if (ours && point.id() == ours->id()) {
                continue;
            }
            if (touchPointHeld(point.state())) {
                reportFalsePoint(point.screenPos());
            }
        }
        const bool oursEnded = type == QEvent::TouchEnd || type == QEvent::TouchCancel
                || (ours && !touchPointHeld(ours->state()));
        if (ours && oursEnded) {
            const QPointF at = ours->screenPos();
            if (attentionWide(at)) {
                rejectBeginWait(at);
                noteFalseTouch();
            } else {
                m_attnPos = at;
                confirmBeginWait(true);
                noteTrace(QStringLiteral("TAP"));
            }
            return true;
        }
        if (!ours && (type == QEvent::TouchEnd || type == QEvent::TouchCancel)) {
            if (m_attnTouchId < 0 && held && attentionNear(held->screenPos())) {
                m_attnTouchId = held->id();
                m_attnPos = held->screenPos();
                confirmBeginWait(true);
                noteTrace(QStringLiteral("TAP"));
                return true;
            }
            // Слота, который ждали, в кадре нет. Нажатие без будущего
            // Release оставило бы кружок включённым.
            rejectBeginWait(held ? held->screenPos() : m_attnPos);
            noteFalseTouch();
            return true;
        }
        if (ours && touchPointHeld(ours->state())) {
            const QPointF at = ours->screenPos();
            if (attentionWide(at)) {
                rejectBeginWait(at);
                noteFalseTouch();
                return true;
            }
            m_attnPos = at;
            if (attentionNear(at)) {
                confirmBeginWait(false);
            }
            return true;
        }
        return true;
    }

    if (!attentionActive()) {
        return false;
    }
    if (m_pointerDown && (type == QEvent::TouchEnd || type == QEvent::TouchCancel)) {
        // Пока удержанный слот ещё в кадре, это не отпускание.
        // Если его уже нет — даже чужой End обязан запустить сброс,
        // иначе кружок в проверке тача остаётся гореть.
        bool ourStillHeld = false;
        const QTouchEvent::TouchPoint *nearHeld = nullptr;
        for (const auto &point : points) {
            const bool heldPoint = touchPointHeld(point.state());
            if (m_primaryTouchId >= 0 && point.id() == m_primaryTouchId) {
                ourStillHeld = heldPoint;
                continue;
            }
            if (!heldPoint) {
                reportFalsePoint(point.screenPos());
                continue;
            }
            const QPointF at = point.screenPos();
            const bool near = qAbs(at.x() - m_lastAcceptedPos.x()) <= kAttentionNearPx
                    && qAbs(at.y() - m_lastAcceptedPos.y()) <= kAttentionNearPx;
            if (near && !nearHeld) {
                nearHeld = &point;
            } else {
                reportFalsePoint(at);
            }
        }
        if (ourStillHeld) {
            noteFalseTouch();
            m_chainRisk = true;
            noteTrace(QStringLiteral("JUMP"));
            return true;
        }
        qreal lineDx = 0;
        const QTouchEvent::TouchPoint *lineHeld =
                closestSameLineHeld(points, m_lastAcceptedPos, m_primaryTouchId, &lineDx);
        if (!lineHeld) {
            lineHeld = nearHeld;
            if (lineHeld) {
                lineDx = qAbs(lineHeld->screenPos().x() - m_lastAcceptedPos.x());
            }
        }
        if (lineHeld) {
            m_primaryTouchId = lineHeld->id();
            m_liveTouchId = lineHeld->id();
            const qreal lineDy = qAbs(lineHeld->screenPos().y() - m_lastAcceptedPos.y());
            if (lineDx <= kAttentionMaxDxPx && lineDy <= kMaxDyPx) {
                m_lastAcceptedPos = lineHeld->screenPos();
                noteTrace(QStringLiteral("CONT"));
                return true;
            }
            reportFalsePoint(lineHeld->screenPos());
            noteFalseTouch();
            m_chainRisk = true;
            noteTrace(QStringLiteral("CONT"));
            return true;
        }
        armReleaseWait(watched, touch->device(), m_primaryTouchId);
        noteTrace(QStringLiteral("HOLD"));
        return true;
    }
    if (m_pointerDown && type == QEvent::TouchUpdate) {
        const QTouchEvent::TouchPoint *our = nullptr;
        for (const auto &point : points) {
            if (point.id() == m_primaryTouchId) {
                our = &point;
                break;
            }
        }
        if (!our || !touchPointHeld(our->state())) {
            qreal replDx = 0;
            const QTouchEvent::TouchPoint *repl =
                    closestSameLineHeld(points, m_lastAcceptedPos, m_primaryTouchId, &replDx);
            if (repl) {
                const qreal replDy = qAbs(repl->screenPos().y() - m_lastAcceptedPos.y());
                if (replDx <= kAttentionMaxDxPx && replDy <= kMaxDyPx) {
                    m_primaryTouchId = repl->id();
                    m_liveTouchId = repl->id();
                    m_lastAcceptedPos = repl->screenPos();
                    noteTrace(QStringLiteral("CONT"));
                    return false;
                }
                reportFalsePoint(repl->screenPos());
                noteFalseTouch();
                m_chainRisk = true;
                armReleaseWait(watched, touch->device(), m_primaryTouchId);
                noteTrace(QStringLiteral("JUMP"));
                return true;
            }
            armReleaseWait(watched, touch->device(), m_primaryTouchId);
            noteTrace(QStringLiteral("HOLD"));
            return true;
        }
        const qreal dx = qAbs(our->screenPos().x() - m_lastAcceptedPos.x());
        const qreal dy = qAbs(our->screenPos().y() - m_lastAcceptedPos.y());
        if (dx > kAttentionMaxDxPx || dy > kMaxDyPx) {
            qreal replDx = 0;
            const QTouchEvent::TouchPoint *repl =
                    closestSameLineHeld(points, m_lastAcceptedPos, our->id(), &replDx);
            if (repl) {
                const qreal replDy = qAbs(repl->screenPos().y() - m_lastAcceptedPos.y());
                if (replDx <= kAttentionMaxDxPx && replDy <= kMaxDyPx) {
                    reportFalsePoint(our->screenPos());
                    m_primaryTouchId = repl->id();
                    m_liveTouchId = repl->id();
                    m_lastAcceptedPos = repl->screenPos();
                    noteTrace(QStringLiteral("CONT"));
                    return false;
                }
            }
            reportFalsePoint(our->screenPos());
            noteFalseTouch();
            m_chainRisk = true;
            armReleaseWait(watched, touch->device(), m_primaryTouchId);
            noteTrace(QStringLiteral("JUMP"));
            return true;
        }
        // После сбоя цепочка коротких шагов не уводит якорь от места нажатия.
        if (m_chainRisk) {
            const qreal fromX = qAbs(our->screenPos().x() - m_contactPos.x());
            const qreal fromY = qAbs(our->screenPos().y() - m_contactPos.y());
            if (fromX > kSameLineDxPx && fromY <= kSameLineDyPx) {
                reportFalsePoint(our->screenPos());
                noteFalseTouch();
                armReleaseWait(watched, touch->device(), m_primaryTouchId);
                noteTrace(QStringLiteral("JUMP"));
                return true;
            }
        }
        return false;
    }
    if (type == QEvent::TouchBegin && !m_pointerDown) {
        int heldCount = 0;
        for (const auto &point : points) {
            if (touchPointHeld(point.state())) {
                ++heldCount;
            }
        }
        if (heldCount != 1 || !held) {
            // Два слота не обходят ожидание: хвост той же полосы нажатием не берём.
            const QTouchEvent::TouchPoint *good = nullptr;
            int goodCount = 0;
            for (const auto &point : points) {
                if (!touchPointHeld(point.state())) {
                    continue;
                }
                const QPointF at = point.screenPos();
                if (recentFarContact(at) || isFastRebegin(at) || isAttentionSameLineJump(at)) {
                    reportFalsePoint(at);
                    continue;
                }
                good = &point;
                ++goodCount;
            }
            if (goodCount == 0) {
                noteFalseTouch();
                noteTrace(QStringLiteral("GHOST"));
                return true;
            }
            if (goodCount == 1 && good) {
                if (!armBeginWait(watched, touch->device(), good->id(), good->screenPos())) {
                    return true;
                }
                noteTrace(QStringLiteral("HOLD"));
                return true;
            }
            return false;
        }
        if (!armBeginWait(watched, touch->device(), held->id(), held->screenPos())) {
            return true;
        }
        noteTrace(QStringLiteral("HOLD"));
        return true;
    }
    if (!m_pointerDown && type == QEvent::TouchUpdate && noteStableMove(watched, touch)) {
        return true;
    }
    return false;
}

bool TouchUsbMonitor::filterMouseEvent(QObject *watched, QMouseEvent *mouse)
{
    const QEvent::Type type = mouse->type();
    const QPointF pos = mouse->screenPos();
    expireStalePointerState();
    dropStalePointer();

    // Пока не выбрали слот или не дождались отпускания чужого кадра,
    // синтезированная мышь не должна зажать кружок в ложной точке.
    // WINSTARTP Mouse — это сама мышь, не хвост тача. Её клик глушить нельзя:
    // пустого тач-кадра, который снял бы ожидание, у этой прошивки нет.
    const bool hardwareMouse = mouse->source() == Qt::MouseEventNotSynthesized;
    if (m_waitForLift) {
        const bool pauseOver = !m_releasedAt.isValid()
                || m_releasedAt.elapsed() > kAttentionHoldMs;
        if (hardwareMouse || pauseOver || type == QEvent::MouseButtonRelease) {
            m_waitForLift = false;
        } else {
            return true;
        }
    }
    if (m_pickingPrimary) {
        if (!hardwareMouse) {
            return true;
        }
        clearPrimaryPick();
    }

    if (m_quarantine) {
        if (type == QEvent::MouseButtonRelease) {
            m_quarantineEnded = true;
            return true;
        }
        if ((type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick)
                && (qAbs(pos.x() - m_quarantinePos.x()) > 1.0
                    || qAbs(pos.y() - m_quarantinePos.y()) > 1.0)) {
            dropQuarantine();
        } else {
            return true;
        }
    }

    if (handleAttentionMouse(watched, mouse)) {
        return true;
    }

    if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick) {
        if (!(mouse->buttons() & Qt::LeftButton)
                && mouse->button() != Qt::LeftButton) {
            return false;
        }
        if (releaseStitchPending()) {
            if (isEmiRebegin(pos, 1)) {
                // Прыжок отодвигает отпуск, обычный MousePress призрака — нет.
                keepReleaseStitch(pos, -1);
                return true;
            }
            // Повтор доставки того же нажатия не должен рвать склейку.
            if (isSameAsLast(pos)) {
                return true;
            }
            // Сюда попадаем, только если палец уже не считается зажатым.
            commitReleaseStitch();
        }
        if (m_pointerDown) {
            if (isJumpMove(pos)) {
                reportFalsePoint(pos);
                markGestureDirty();
                if (kStitchEnabled) {
                    keepReleaseStitch(pos, -1);
                }
                armReleaseWait(watched, nullptr, m_primaryTouchId);
                return true;
            }
            onLiveTouchEvidence();
            return false;
        }
        if (consumeFreshContact(watched, nullptr, m_primaryTouchId, pos)) {
            return true;
        }
        beginContact(pos);
        m_uiHasPress = true;
        onLiveTouchEvidence();
        return false;
    }

    if (type == QEvent::MouseMove) {
        if (!(mouse->buttons() & Qt::LeftButton) || !m_pointerDown) {
            return false;
        }
        if (releaseStitchPending()) {
            // Сдвиг только если это прыжок. Чужой MouseMove иначе не даёт отпустить.
            if (isJumpMove(pos)) {
                reportFalsePoint(pos);
            }
            keepReleaseStitch(pos, -1);
            return true;
        }
        // QMouseEvent в Qt 5 не даёт поменять screenPos. Прыжок в UI не кладём
        // и якорь им не двигаем, даже когда склейка выключена.
        if (isJumpMove(pos) || (kStitchEnabled && m_gestureDirty)) {
            if (isJumpMove(pos)) {
                reportFalsePoint(pos);
            }
            markGestureDirty();
            armReleaseWait(watched, nullptr, m_primaryTouchId);
            return true;
        }
        m_lastAcceptedPos = pos;
        return false;
    }

    if (type == QEvent::MouseButtonRelease) {
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        // Чистый End сразу в UI, чтобы «+»/«−» и holdRepeat не ждали.
        // Мышь не помечает слот отпущенным: тач ещё может идти, и тогда
        // срок сдвигают его кадры, а не этот синтезированный Release.
        if (kStitchEnabled && m_pointerDown && (m_gestureDirty || m_stitchPending)) {
            if (!m_stitchPending) {
                const int id = m_liveTouchId >= 0 ? m_liveTouchId : m_primaryTouchId;
                armReleaseStitch(m_lastWindow.data(), true, true, nullptr, id);
            }
            return true;
        }
        // Чужой Release без зажатого пальца окно после End не открывает:
        // иначе следующий тап дальше ±10 съедается как призрак.
        if (!m_pointerDown) {
            return false;
        }
        endContact();
        return false;
    }

    return false;
}

bool TouchUsbMonitor::filterTouchEvent(QObject *watched, QTouchEvent *touch)
{
    const QEvent::Type type = touch->type();
    expireStalePointerState();

    if (m_waitForLift) {
        bool anyHeld = false;
        for (const auto &point : touch->touchPoints()) {
            if (touchPointHeld(point.state())) {
                anyHeld = true;
                break;
            }
        }
        // Пустой кадр или пауза после сброса. Begin в ту же миллисекунду
        // не снимает ожидание: иначе хвост сразу зажимает экран снова.
        const bool pauseOver = m_releasedAt.isValid()
                && m_releasedAt.elapsed() > kAttentionHoldMs;
        if (!anyHeld || (pauseOver && type == QEvent::TouchBegin)) {
            m_waitForLift = false;
        } else {
            return true;
        }
    }

    if (m_pickingPrimary) {
        return handlePrimaryPick(watched, touch);
    }

    if (m_quarantine) {
        if (handleQuarantineTouch(watched, touch)) {
            return true;
        }
    }

    if (handleAttentionTouch(watched, touch)) {
        return true;
    }

    const auto &points = touch->touchPoints();
    const QTouchEvent::TouchPoint *primary = nullptr;
    bool foreignHeld = false;
    for (const auto &point : points) {
        if (m_primaryTouchId >= 0 && point.id() == m_primaryTouchId) {
            primary = &point;
        } else if (touchPointHeld(point.state())) {
            foreignHeld = true;
        }
    }

    // QGuiApplication уже учёл кадр в activeTouchPoints до этого фильтра.
    // Глотаем только доставку в QQuickWindow. End без последующего replay
    // снова залипает grabber — поэтому отложенный End всегда уходит по таймеру.

    // Второй слот — признак сбоя, но сам по себе не задерживает отпускание:
    // иначе однокадровый призрак превращает обычный тап в «залипшую» кнопку.
    if (foreignHeld && m_pointerDown) {
        noteFalseTouch();
    }

    if (type == QEvent::TouchBegin) {
        const QTouchEvent::TouchPoint *first = eventPrimaryPoint(points);
        QPointF pos = first ? first->screenPos() : QPointF();
        // Склейка держит палец. Любой новый Begin до тишины — тот же контакт.
        // Отпускание делает только таймер склейки, не эта точка.
        if (releaseStitchPending()) {
            if (isEmiRebegin(pos, points.size())) {
                return absorbContinuingTouch(touch);
            }
            // Повтор уже принятой точки. Склейку не рвём и второй Begin не шлём.
            if (!first || isSameAsLast(pos)) {
                return true;
            }
            commitReleaseStitch();
        }
        // Фильтр на qApp видит один Begin у нескольких QObject. Дубликат
        // в ту же точку нужно пропустить в окно, иначе UI мёртвый.
        // Любая другая точка при уже зажатом пальце — не новый тап:
        // она проваливается в кнопку под оверлеем проверки.
        if (m_pointerDown && first
                && (!isSameAsLast(pos) || m_gestureDirty || m_stitchPending)) {
            const bool stale = !m_lastLiveAt.isValid()
                    || m_lastLiveAt.elapsed() > kWatchRejoinMs;
            if (!stale) {
                return absorbContinuingTouch(touch);
            }
            // Долго нет живого кадра: старый захват отпускаем и отдаём этот тап.
            // Иначе поле проверки молчит, а «Назад» над ним ещё нажимается.
            releaseHeldPress();
        }
        // Сразу после End далёкая точка не палец. Близкая в том же
        // кадре остаётся, чужие слоты из кадра убираем.
        if (!m_pointerDown && m_releasedAt.isValid()
                && m_releasedAt.elapsed() <= kAttentionHoldMs && attentionActive()) {
            const QTouchEvent::TouchPoint *nearPoint = nullptr;
            bool anyHeld = false;
            for (const auto &point : points) {
                if (!touchPointHeld(point.state())) {
                    continue;
                }
                anyHeld = true;
                const QPointF at = point.screenPos();
                const bool far = qAbs(at.x() - m_lastAcceptedPos.x()) > kAttentionNearPx
                        || qAbs(at.y() - m_lastAcceptedPos.y()) > kAttentionNearPx;
                if (!far) {
                    if (!nearPoint) {
                        nearPoint = &point;
                    }
                } else {
                    reportFalsePoint(at);
                }
            }
            if (anyHeld && !nearPoint) {
                noteTrace(QStringLiteral("GHOST"));
                return true;
            }
            if (nearPoint) {
                const int nearId = nearPoint->id();
                const QPointF nearPos = nearPoint->screenPos();
                keepOnlyTouchId(touch, nearId);
                pos = nearPos;
                m_primaryTouchId = -1;
                first = firstHeldPoint(touch->touchPoints());
            }
        }
        if (!m_pointerDown && first
                && consumeFreshContact(watched, touch->device(), first->id(), pos)) {
            return true;
        }
        int heldCount = 0;
        for (const auto &point : points) {
            if (touchPointHeld(point.state())) {
                ++heldCount;
            }
        }
        // Два слота в первом кадре: p0 часто ложный. Ждём, какой останется.
        if (!m_pointerDown && heldCount > 1) {
            beginPrimaryPick(watched, touch->device(), touch);
            return true;
        }
        m_ignoreForeignUntilClear = false;
        if (m_primaryTouchId >= 0 && !primary) {
            m_primaryTouchId = -1;
        }
        if (m_primaryTouchId < 0 && first) {
            m_primaryTouchId = first->id();
            if (!m_pointerDown) {
                beginContact(first->screenPos());
            }
            onLiveTouchEvidence();
        }
        if (points.size() > 1) {
            noteFalseTouch();
        }
        if (m_primaryTouchId >= 0) {
            keepOnlyTouchId(touch, m_primaryTouchId);
            if (m_gestureDirty) {
                freezePrimaryToLastAccepted(touch);
            }
        }
        return false;
    }

    if (type == QEvent::TouchUpdate) {
        if (releaseStitchPending()) {
            // Кадр не отдаём. Иначе Pressed чужого слота приходит в окно,
            // MouseArea проверки его не берёт, и кнопка меню под оверлеем нажимается.
            bool liveGone = m_liveTouchReleased;
            if (m_liveTouchId >= 0) {
                bool seenLive = false;
                for (const auto &point : points) {
                    if (point.id() != m_liveTouchId) {
                        continue;
                    }
                    seenLive = true;
                    if (!touchPointHeld(point.state())) {
                        liveGone = true;
                    }
                }
                if (!seenLive && foreignHeld) {
                    liveGone = true;
                }
            } else if (!firstHeldPoint(points)) {
                liveGone = true;
            }
            for (const auto &point : points) {
                if (!touchPointHeld(point.state())) {
                    continue;
                }
                if (m_liveTouchId >= 0 && point.id() == m_liveTouchId) {
                    continue;
                }
                reportFalsePoint(point.screenPos());
            }
            keepReleaseStitch(touch);
            if (liveGone) {
                noteLiveTouchReleased(touch->device());
            }
            return true;
        }
        if (m_ignoreForeignUntilClear && !primary) {
            const QTouchEvent::TouchPoint *pressed = nullptr;
            for (const auto &point : points) {
                if (point.state() == Qt::TouchPointPressed) {
                    pressed = &point;
                    break;
                }
            }
            if (pressed) {
                // Новый слот после отпускания может прийти Update'ом, не Begin'ом.
                // Во время внимания так первую точку не берём: это хвост призрака.
                if (!m_pointerDown && attentionActive()) {
                    reportFalsePoint(pressed->screenPos());
                    return true;
                }
                if (!m_pointerDown
                        && consumeFreshContact(watched, touch->device(), pressed->id(),
                                               pressed->screenPos())) {
                    return true;
                }
                m_ignoreForeignUntilClear = false;
                m_primaryTouchId = pressed->id();
                if (!m_pointerDown) {
                    beginContact(pressed->screenPos());
                }
                onLiveTouchEvidence();
                keepOnlyTouchId(touch, m_primaryTouchId);
                return false;
            }
            if (!foreignHeld) {
                m_ignoreForeignUntilClear = false;
            }
            return true;
        }
        if (m_primaryTouchId < 0) {
            const QTouchEvent::TouchPoint *first = firstHeldPoint(points);
            if (!first) {
                return false;
            }
            // То же, что для Begin: Pressed внутри Update не должен обходить карантин.
            // И не становится первой точкой, пока внимание включено.
            if (!m_pointerDown && attentionActive()) {
                for (const auto &point : points) {
                    if (touchPointHeld(point.state())) {
                        reportFalsePoint(point.screenPos());
                    }
                }
                return true;
            }
            if (!m_pointerDown
                    && consumeFreshContact(watched, touch->device(), first->id(),
                                           first->screenPos())) {
                return true;
            }
            m_primaryTouchId = first->id();
            if (!m_pointerDown) {
                beginContact(first->screenPos());
            }
            onLiveTouchEvidence();
            keepOnlyTouchId(touch, m_primaryTouchId);
            return false;
        }
        if (!primary) {
            // Первого слота уже нет, второй ещё держится — это обрыв EMI, не отпускание.
            if (kStitchEnabled && m_pointerDown && (m_gestureDirty || foreignHeld)) {
                if (foreignHeld) {
                    markGestureDirty();
                }
                noteLiveTouchReleased(touch->device());
                return true;
            }
            if (m_pointerDown) {
                endContact();
            }
            m_primaryTouchId = -1;
            m_ignoreForeignUntilClear = foreignHeld;
            return true;
        }
        if (primary->state() == Qt::TouchPointReleased) {
            // Короткий второй слот, который уже пропал, отпускание не задерживает.
            // Если он ещё в кадре — контроллер оборвал первый слот, палец на стекле.
            if (kStitchEnabled && m_pointerDown && (m_gestureDirty || foreignHeld)) {
                if (foreignHeld) {
                    markGestureDirty();
                }
                noteLiveTouchReleased(touch->device());
                return true;
            }
            if (m_pointerDown) {
                endContact();
            }
            keepOnlyTouchId(touch, m_primaryTouchId);
            m_primaryTouchId = -1;
            m_ignoreForeignUntilClear = foreignHeld;
            return false;
        }
        for (const auto &point : points) {
            if (point.id() == m_primaryTouchId || !touchPointHeld(point.state())) {
                continue;
            }
            reportFalsePoint(point.screenPos());
        }
        keepOnlyTouchId(touch, m_primaryTouchId);
        if (isJumpMove(primary->screenPos()) || (kStitchEnabled && m_gestureDirty)) {
            markGestureDirty();
            if (kStitchEnabled) {
                keepReleaseStitch(touch);
            }
            // Прыжок в UI не кладём. Срок один, повторные кадры его не двигают:
            // иначе после минуты внимания сброс уже не наступает.
            armReleaseWait(watched, touch->device(), m_primaryTouchId);
            return true;
        }
        m_lastAcceptedPos = primary->screenPos();
        return false;
    }

    if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
        // Живой второй слот в этом кадре — обрыв первого, не отпускание пальца.
        if (kStitchEnabled && m_pointerDown && (m_gestureDirty || foreignHeld || m_stitchPending)) {
            if (foreignHeld) {
                markGestureDirty();
            }
            noteLiveTouchReleased(touch->device());
            return true;
        }
        if (m_pointerDown) {
            endContact();
        }
        if (m_primaryTouchId >= 0) {
            keepOnlyTouchId(touch, m_primaryTouchId);
        }
        m_primaryTouchId = -1;
        m_ignoreForeignUntilClear = false;
        return false;
    }

    return false;
}

void TouchUsbMonitor::releaseHeldPress()
{
    if (m_deliveringStitchRelease) {
        return;
    }
    QObject *target = m_attnTarget.data();
    if (!target) {
        target = m_lastWindow.data();
    }
    const bool held = m_pointerDown || m_uiHasPress || m_attnPhase != AttnPhase::None;
    if (!held) {
        // Фильтр уже отпустил палец, а область могла остаться с захватом.
        // mouseGrabber бывает пуст, когда держит именно тач.
        sendTouchCancel(target, m_attnDevice);
        releaseSceneGrab(target);
        return;
    }
    if (m_attnPhase != AttnPhase::Release) {
        m_attnPhase = AttnPhase::Release;
        m_attnOrigin = m_lastAcceptedPos;
        m_attnPos = m_lastAcceptedPos;
        if (m_attnTouchId < 0) {
            m_attnTouchId = m_primaryTouchId >= 0 ? m_primaryTouchId : m_liveTouchId;
        }
        if (!m_attnTarget) {
            m_attnTarget = m_lastWindow;
        }
    }
    if (m_attnTimer) {
        m_attnTimer->stop();
    }
    writeTouchTrace(QStringLiteral("UI release"), m_lastAcceptedPos);
    commitReleaseWait();
}

void TouchUsbMonitor::clearTouchTrace()
{
    // Вход в проверку тача. Зависший жест с прошлого экрана сюда не переносим.
    clearContactState();
    if (!m_traceFile.isOpen()) {
        return;
    }
    if (!m_traceFile.resize(0)) {
        qWarning().noquote() << "TouchUsbMonitor: cannot clear touch trace"
                             << m_traceFile.fileName() << m_traceFile.errorString();
        return;
    }
    m_traceFile.seek(0);
    m_traceBytes = 0;
    m_traceHasPrev = false;
    m_tracePrevAt.invalidate();
    m_tracePrevLine.clear();
    writeTouchTrace(QStringLiteral("TRACE clear"));
}

void TouchUsbMonitor::noteTrace(const QString &tag)
{
    if (tag.isEmpty()) {
        return;
    }
    if (!m_traceInEvent) {
        writeTouchTrace(tag);
        return;
    }
    if (m_traceTag == tag || m_traceTag.contains(QLatin1Char(' ') + tag)
            || m_traceTag.startsWith(tag + QLatin1Char(' '))) {
        return;
    }
    if (!m_traceTag.isEmpty()) {
        m_traceTag += QLatin1Char(' ');
    }
    m_traceTag += tag;
}

void TouchUsbMonitor::writeTouchTrace(const QString &line)
{
    writeTouchTraceAt(line, QPointF(), false);
}

void TouchUsbMonitor::writeTouchTrace(const QString &line, const QPointF &pos)
{
    writeTouchTraceAt(line, pos, true);
}

void TouchUsbMonitor::reportFalsePoint(const QPointF &screenPos)
{
    // Один кадр приходит нескольким объектам. Повтор той же точки не рисуем.
    if (m_falsePointClock.isValid()
            && m_falsePointClock.elapsed() < 40
            && qAbs(screenPos.x() - m_falsePoint.x()) < 8.0
            && qAbs(screenPos.y() - m_falsePoint.y()) < 8.0) {
        return;
    }
    m_falsePoint = screenPos;
    m_falsePointClock.start();

    QPointF local = screenPos;
    if (QWindow *window = qobject_cast<QWindow *>(m_lastWindow.data())) {
        local = QPointF(window->mapFromGlobal(screenPos.toPoint()));
    }
    emit falsePoint(local.x(), local.y());
}

void TouchUsbMonitor::writeTouchTraceAt(const QString &line, const QPointF &pos, bool hasPos)
{
    if (!m_traceFile.isOpen() || line.isEmpty()) {
        return;
    }
    const bool samePlace = hasPos && m_traceHasPrev
            && qRound(pos.x()) == qRound(m_tracePrevPos.x())
            && qRound(pos.y()) == qRound(m_tracePrevPos.y());
    // Повтор той же точки и того же статуса — кадр у нескольких объектов или палец стоит.
    if (samePlace && line == m_tracePrevLine) {
        return;
    }

    if (m_traceBytes >= kTraceRotateBytes) {
        const QString path = m_traceFile.fileName();
        m_traceFile.close();
        QFile::remove(path + QLatin1String(".prev"));
        QFile::rename(path, path + QLatin1String(".prev"));
        if (!m_traceFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            qWarning().noquote() << "TouchUsbMonitor: cannot rotate touch trace" << path;
            return;
        }
        m_traceBytes = 0;
    }

    const auto field = [](const QString &text, int width) {
        return text.rightJustified(width);
    };
    QString xy = QStringLiteral("—");
    QString dx = QStringLiteral("—");
    QString dy = QStringLiteral("—");
    QString dt = QStringLiteral("—");
    if (m_tracePrevAt.isValid()) {
        dt = QString::number(m_tracePrevAt.elapsed());
    }
    if (hasPos) {
        xy = QStringLiteral("%1,%2").arg(qRound(pos.x())).arg(qRound(pos.y()));
        if (m_traceHasPrev) {
            const int ddx = qRound(pos.x() - m_tracePrevPos.x());
            const int ddy = qRound(pos.y() - m_tracePrevPos.y());
            dx = ddx > 0 ? QStringLiteral("+%1").arg(ddx) : QString::number(ddx);
            dy = ddy > 0 ? QStringLiteral("+%1").arg(ddy) : QString::number(ddy);
        }
        m_tracePrevPos = pos;
        m_traceHasPrev = true;
    }
    m_tracePrevAt.start();
    m_tracePrevLine = line;

    const QString text = QStringLiteral("%1  %2  dx=%3 dy=%4 dt=%5  %6\n")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")))
            .arg(field(xy, 9))
            .arg(field(dx, 5))
            .arg(field(dy, 5))
            .arg(field(dt, 5))
            .arg(line);
    m_traceBytes += m_traceFile.write(text.toUtf8());
    // Пустая строка отделяет жест. Повтор того же End сюда уже не доходит.
    if (line == QLatin1String("End") || line.startsWith(QLatin1String("End "))
            || line.contains(QLatin1String(" End"))) {
        m_traceBytes += m_traceFile.write("\n", 1);
    }
    m_traceFile.flush();
}

void TouchUsbMonitor::observeGhosts(QEvent *event)
{
    // Событие не меняем и не глотаем. Бледный круг в проверке тача — только отметка.
    if (auto *mouse = dynamic_cast<QMouseEvent *>(event)) {
        const QPointF pos = mouse->screenPos();
        const bool press = mouse->type() == QEvent::MouseButtonPress
                || mouse->type() == QEvent::MouseButtonDblClick;
        if (press && mouse->button() == Qt::LeftButton) {
            m_ghostDown = true;
            m_ghostPos = pos;
            return;
        }
        if (mouse->type() == QEvent::MouseButtonRelease && mouse->button() == Qt::LeftButton) {
            m_ghostDown = false;
            return;
        }
        if (m_ghostDown && (mouse->buttons() & Qt::LeftButton)
                && (qAbs(pos.x() - m_ghostPos.x()) > kMaxDxPx
                    || qAbs(pos.y() - m_ghostPos.y()) > kMaxDyPx)) {
            reportFalsePoint(pos);
            return;
        }
        if (m_ghostDown && (mouse->buttons() & Qt::LeftButton)) {
            m_ghostPos = pos;
        }
        return;
    }

    auto *touch = dynamic_cast<QTouchEvent *>(event);
    if (!touch) {
        return;
    }
    const QTouchEvent::TouchPoint *primary = nullptr;
    for (const auto &point : touch->touchPoints()) {
        if (!touchPointHeld(point.state())) {
            continue;
        }
        if (!primary) {
            primary = &point;
            continue;
        }
        reportFalsePoint(point.screenPos());
    }
    if (!primary) {
        if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
            m_ghostDown = false;
        }
        return;
    }
    const QPointF pos = primary->screenPos();
    if (!m_ghostDown || primary->state() == Qt::TouchPointPressed) {
        m_ghostDown = true;
        m_ghostPos = pos;
        return;
    }
    if (qAbs(pos.x() - m_ghostPos.x()) > kMaxDxPx
            || qAbs(pos.y() - m_ghostPos.y()) > kMaxDyPx) {
        reportFalsePoint(pos);
        return;
    }
    m_ghostPos = pos;
}

bool TouchUsbMonitor::eventFilter(QObject *watched, QEvent *event)
{
    if (!isPointerEvent(event->type())) {
        return QObject::eventFilter(watched, event);
    }

    if (qobject_cast<QWindow *>(watched)) {
        m_lastWindow = watched;
    }
    // pollAttention(); // фильтрация выключена: таймер внимания не отпускает жест сам

    const bool nested = m_traceInEvent;
    const QString savedTag = m_traceTag;
    m_traceInEvent = true;
    if (!nested) {
        m_traceTag.clear();
    }

    QPointF pos;
    bool hasPos = false;
    const QString label = pointerTraceLabel(event, &pos, &hasPos);
    const bool touchEvent = event->type() == QEvent::TouchBegin
            || event->type() == QEvent::TouchUpdate
            || event->type() == QEvent::TouchEnd
            || event->type() == QEvent::TouchCancel;

    if (m_deliveringStitchRelease) {
        const QString line = label.isEmpty()
                ? QStringLiteral("replay")
                : QStringLiteral("replay %1").arg(label);
        if (hasPos) {
            writeTouchTrace(line, pos);
        } else {
            writeTouchTrace(line);
        }
        m_traceInEvent = nested;
        m_traceTag = savedTag;
        return QObject::eventFilter(watched, event);
    }

    // Фильтрация выключена: глушение при активации и разбор кадра не глотают касание.
    // if (isActivationMute() || isRecoverMute()) { ... return true; }
    observeGhosts(event);
    const bool filtered = false;
    /*
    bool filtered = false;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        filtered = filterMouseEvent(watched, static_cast<QMouseEvent *>(event));
        break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        filtered = filterTouchEvent(watched, static_cast<QTouchEvent *>(event));
        break;
    default:
        break;
    }
    */

    QString status = m_traceTag;
    if (status.isEmpty()) {
        if (filtered && releaseStitchPending()) {
            status = QStringLiteral("STITCH");
        } else {
            status = filtered ? QStringLiteral("DROP") : QStringLiteral("PASS");
        }
    }
    const QString line = label.isEmpty() ? status : QStringLiteral("%1 %2").arg(status, label);
    const bool passed = !filtered && m_traceTag.isEmpty();
    const auto *mouse = dynamic_cast<const QMouseEvent *>(event);
    const bool synthMouse = mouse && mouse->source() != Qt::MouseEventNotSynthesized;
    const bool echoOfTouch = synthMouse && hasPos && m_traceTouchAt.isValid()
            && m_traceTouchAt.elapsed() < 50
            && qAbs(pos.x() - m_traceTouchPos.x()) < 2.0
            && qAbs(pos.y() - m_traceTouchPos.y()) < 2.0
            && passed == m_traceTouchPassed;
    if (!echoOfTouch) {
        if (hasPos) {
            writeTouchTrace(line, pos);
        } else {
            writeTouchTrace(line);
        }
    }
    if (touchEvent && hasPos) {
        m_traceTouchAt.start();
        m_traceTouchPos = pos;
        m_traceTouchPassed = passed;
    }

    m_traceInEvent = nested;
    if (nested) {
        m_traceTag = savedTag;
    } else {
        m_traceTag.clear();
    }

    if (filtered) {
        return true;
    }
    if (m_pointerDown) {
        m_lastLiveAt.start();
    }
    return QObject::eventFilter(watched, event);
}

void TouchUsbMonitor::onLiveTouchEvidence()
{
    // Во время recover pointer глушится (isRecoverMute) — сюда не попадём.
    if (m_recovering) {
        return;
    }
    if (m_touchOk && !m_screenFaultShown) {
        return;
    }
    applyState(QStringLiteral("ok"),
               QStringLiteral("touch event received"),
               false);
}

bool TouchUsbMonitor::touchInputPresent()
{
    // ILITEK: Qt берёт координаты с "WINSTARTP Mouse" (if01), digitizer часто молчит.
    // PIC16F1459: один узел "PIC16F1459_HID HID M-Touch", отдельной мыши нет.
    return !touchInputPresentDetail().isEmpty();
}

bool TouchUsbMonitor::touchUsbDevicePresent(QString *sysfsPath)
{
    const QDir usbDir(QStringLiteral("/sys/bus/usb/devices"));
    const auto entries = usbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (entry.contains(QLatin1Char(':'))) {
            continue;
        }
        const QString base = usbDir.filePath(entry);
        const QString vid = readSysfsText(base + QStringLiteral("/idVendor"));
        const QString pid = readSysfsText(base + QStringLiteral("/idProduct"));
        if (!isKnownTouchUsb(vid, pid)) {
            continue;
        }
        if (sysfsPath) {
            *sysfsPath = base;
        }
        return true;
    }
    return false;
}

void TouchUsbMonitor::setRecovering(bool on)
{
    if (m_recovering == on) {
        return;
    }
    m_recovering = on;
    emit recoveringChanged();
}

// TEST: удалить после теста вместе с квадратом в main.qml.
void TouchUsbMonitor::setTestRecoverAttempts(int count)
{
    if (m_testRecoverAttempts == count) {
        return;
    }
    m_testRecoverAttempts = count;
    emit testRecoverAttemptsChanged();
}

// TEST: любая потеря входа (WINSTARTP Mouse, PIC16F1459 и прочие). Удалить после теста.
void TouchUsbMonitor::noteTestTouchLoss()
{
    if (!m_testRecoverVisible) {
        m_testRecoverVisible = true;
        emit testRecoverVisibleChanged();
    }
    m_testRecoverHideTimer->start(kTestRecoverHideMs);
}

// TEST: удалить после теста.
void TouchUsbMonitor::hideTestRecoverBadge()
{
    if (!m_testRecoverVisible) {
        return;
    }
    m_testRecoverVisible = false;
    emit testRecoverVisibleChanged();
}

void TouchUsbMonitor::raiseScreenFault()
{
    if (m_screenFaultShown) {
        return;
    }
    m_screenFaultShown = true;
    if (m_deviceLog) {
        m_deviceLog->onWarningCode(FaultWarningCode);
    }
    if (m_periph) {
        m_periph->showWarningCode(FaultWarningCode);
    }
}

void TouchUsbMonitor::clearScreenFault()
{
    if (!m_screenFaultShown) {
        return;
    }
    m_screenFaultShown = false;
    if (m_periph) {
        m_periph->dismissWarningCode(FaultWarningCode);
    }
}

void TouchUsbMonitor::applyState(const QString &state, const QString &detail, bool logFault)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged();
    }
    if (m_detail != detail) {
        m_detail = detail;
        emit detailChanged();
    }

    // ok/recovered — мышь есть. lost — идёт recover, 0x09 ещё нет.
    // failed — 15 с без мыши, на экране 0x09, порт продолжаем крутить.
    const bool ok = (state == QLatin1String("ok") || state == QLatin1String("recovered"));
    const bool fault = (state == QLatin1String("lost") || state == QLatin1String("failed"));

    if (m_touchOk != ok) {
        m_touchOk = ok;
        emit touchOkChanged();
    }
    if (m_faultActive != fault) {
        m_faultActive = fault;
        emit faultActiveChanged();
    }

    if (ok) {
        m_lastLoggedKey.clear();
        clearScreenFault();
        return;
    }

    if (logFault && fault) {
        const QString logKey = state + QLatin1Char('|') + detail;
        if (logKey != m_lastLoggedKey && m_deviceLog) {
            m_lastLoggedKey = logKey;
            m_deviceLog->logSystemError(
                        QStringLiteral("Тачскрин USB: %1 (%2)").arg(state, detail));
            emit touchUsbFault(state, detail);
        }
    }
}

void TouchUsbMonitor::check()
{
    beginPostActivationCheck();
}

void TouchUsbMonitor::beginStartupCheck()
{
    beginMouseCheck(QStringLiteral("no touch input at startup"));
}

void TouchUsbMonitor::beginPostActivationCheck()
{
    beginMouseCheck(QStringLiteral("no touch input after activation"));
}

void TouchUsbMonitor::releaseHeldPort()
{
    if (m_portPhase == kPortPhaseOff && !m_activeDisablePath.isEmpty()) {
        writePortDisable(m_activeDisablePath, false);
    }
}

void TouchUsbMonitor::abortRecoverForActivation()
{
    if (m_startupCheckTimer) {
        m_startupCheckTimer->stop();
    }
    if (!m_recovering) {
        return;
    }
    releaseHeldPort();
    m_stepTimer->stop();
    m_deadlineTimer->stop();
    m_portPhase = 0;
    setRecovering(false);
}

void TouchUsbMonitor::beginMouseCheck(const QString &missingDetail)
{
    if (m_startupCheckTimer) {
        m_startupCheckTimer->stop();
    }
    // Повторный вход (активация / check()) мог застать цикл в disable=1.
    releaseHeldPort();
    m_stepTimer->stop();
    m_deadlineTimer->stop();
    m_portPhase = 0;
    m_cycle = 0;
    rememberTouchPort();

    const QString present = touchInputPresentDetail();
    if (!present.isEmpty()) {
        setRecovering(false);
        applyState(QStringLiteral("ok"), present, false);
        return;
    }

    qWarning().noquote() << "TouchUsbMonitor: touch input missing,"
                         << missingDetail << ", port reset";
    // TEST: квадрат в углу. Удалить после теста.
    setTestRecoverAttempts(0);
    noteTestTouchLoss();
    writeTouchTrace(QStringLiteral("RECOVER start %1").arg(missingDetail));
    clearContactState();
    setRecovering(true);
    applyState(QStringLiteral("lost"), missingDetail, false);
    m_checkStarted.start();
    m_deadlineTimer->start(kFaultDeadlineMs);
    kickPortCycle();
}

void TouchUsbMonitor::kickPortCycle()
{
    // Фазы: 0 → disable=1 → (2 с) → disable=0 → опрос мыши 2 с → снова 0.
    if (!m_recovering) {
        return;
    }
    const QString present = touchInputPresentDetail();
    if (!present.isEmpty()) {
        finishRecovered(present);
        return;
    }
    if (m_checkStarted.isValid() && m_checkStarted.elapsed() >= kFaultDeadlineMs) {
        noteOngoingFault(QStringLiteral("no touch input, port reset continues"));
    }

    const QString path = resolvePortDisablePath();
    if (path.isEmpty()) {
        qWarning("TouchUsbMonitor: touch port unknown, retry");
        m_portPhase = 0;
        m_stepTimer->start(kPortToggleMs);
        return;
    }

    m_activeDisablePath = path;
    qWarning().noquote() << "TouchUsbMonitor: port off cycle" << (m_cycle + 1) << path;
    // TEST: цифра в квадрате = номер этого сброса порта. Удалить после теста.
    setTestRecoverAttempts(m_cycle + 1);
    if (!writePortDisable(path, true)) {
        ++m_cycle;
        m_portPhase = 0;
        m_stepTimer->start(kPortToggleMs);
        return;
    }

    m_portPhase = kPortPhaseOff;
    m_stepTimer->start(kPortToggleMs);
}

void TouchUsbMonitor::onPortStep()
{
    if (!m_recovering) {
        return;
    }
    if (m_portPhase == 0) {
        kickPortCycle();
        return;
    }
    if (m_portPhase == kPortPhaseOff) {
        qWarning().noquote() << "TouchUsbMonitor: port on" << m_activeDisablePath;
        writePortDisable(m_activeDisablePath, false);
        m_portPhase = kPortPhaseOn;
        m_portOnSince.start();
        m_stepTimer->start(kPortPollMs);
        return;
    }

    const QString present = touchInputPresentDetail();
    if (!present.isEmpty()) {
        finishRecovered(present);
        return;
    }

    const bool onLongEnough = m_portOnSince.isValid() && m_portOnSince.elapsed() >= kPortToggleMs;
    if (onLongEnough) {
        ++m_cycle;
        m_portPhase = 0;
        kickPortCycle();
        return;
    }
    m_stepTimer->start(kPortPollMs);
}

void TouchUsbMonitor::onRecoverDeadline()
{
    if (!m_recovering) {
        return;
    }
    const QString present = touchInputPresentDetail();
    if (!present.isEmpty()) {
        finishRecovered(present);
        return;
    }
    noteOngoingFault(QStringLiteral("no touch input within 15s, port reset continues"));
}

void TouchUsbMonitor::noteOngoingFault(const QString &detail)
{
    if (m_screenFaultShown) {
        return;
    }
    qWarning().noquote() << "TouchUsbMonitor:" << detail;
    applyState(QStringLiteral("failed"), detail, true);
    raiseScreenFault();
}

void TouchUsbMonitor::finishRecovered(const QString &detail)
{
    if (!m_recovering && m_state == QLatin1String("ok")) {
        return;
    }
    m_stepTimer->stop();
    m_deadlineTimer->stop();
    m_portPhase = 0;
    setRecovering(false);
    armPostRecoverMute();
    writeTouchTrace(QStringLiteral("RECOVER done %1").arg(detail));
    qWarning().noquote() << "TouchUsbMonitor:" << detail;
    rememberTouchPort();
    applyState(QStringLiteral("recovered"), detail, false);
}

QString TouchUsbMonitor::hubDisablePath(int bus)
{
    return QStringLiteral("/sys/bus/usb/devices/usb%1/%1-0:1.0/usb%1-port1/disable").arg(bus);
}

bool TouchUsbMonitor::isAllowedDisablePath(const QString &path)
{
    for (int bus = kHubBusFirst; bus <= kHubBusLast; ++bus) {
        if (path == hubDisablePath(bus)) {
            return true;
        }
    }
    return false;
}

void TouchUsbMonitor::loadRememberedPort()
{
    // sysfs-узел порта существует и без устройства. После клона путь может быть
    // с другой платы; если тач так и не появится — сбросим именно его.
    QFile file(rememberedPortFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    const QString path = QString::fromUtf8(file.readAll()).trimmed();
    if (isAllowedDisablePath(path) && QFile::exists(path)) {
        m_lastDisablePath = path;
        qWarning().noquote() << "TouchUsbMonitor: saved touch port" << path;
    }
}

void TouchUsbMonitor::saveRememberedPort() const
{
    if (!isAllowedDisablePath(m_lastDisablePath)) {
        return;
    }
    const QString path = rememberedPortFile();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning().noquote() << "TouchUsbMonitor: cannot save touch port" << path;
        return;
    }
    file.write(m_lastDisablePath.toUtf8());
    file.write("\n");
}

void TouchUsbMonitor::probeTouchPort()
{
    const bool foundPort = rememberTouchPort();
    if (foundPort) {
        m_portProbeTimer->stop();
        m_portProbeLeft = 0;
    }

    // Узел появился раньше 3 с — recover не нужен.
    if (m_startupCheckTimer && m_startupCheckTimer->isActive()) {
        const QString present = touchInputPresentDetail();
        if (!present.isEmpty()) {
            m_startupCheckTimer->stop();
            applyState(QStringLiteral("ok"), present, false);
            return;
        }
    }

    if (foundPort) {
        return;
    }
    if (--m_portProbeLeft <= 0) {
        m_portProbeTimer->stop();
        qWarning().noquote() << "TouchUsbMonitor: touch device not found at startup, last port"
                             << m_lastDisablePath;
    }
}

bool TouchUsbMonitor::rememberTouchPort()
{
    QString devicePath;
    if (!touchUsbDevicePresent(&devicePath)) {
        return false;
    }
    const QString portCanonical = QFileInfo(devicePath + QStringLiteral("/port")).canonicalFilePath();
    if (portCanonical.isEmpty()) {
        return false;
    }
    for (int bus = kHubBusFirst; bus <= kHubBusLast; ++bus) {
        const QString disable = hubDisablePath(bus);
        if (!QFile::exists(disable)) {
            continue;
        }
        const QString knownPort = QFileInfo(disable).dir().canonicalPath();
        if (knownPort.isEmpty() || knownPort != portCanonical) {
            continue;
        }
        if (m_lastDisablePath != disable) {
            m_lastDisablePath = disable;
            saveRememberedPort();
            qWarning().noquote() << "TouchUsbMonitor: touch port" << disable;
        }
        return true;
    }
    qWarning().noquote() << "TouchUsbMonitor: touch port is not a known hub port" << portCanonical;
    return false;
}

QString TouchUsbMonitor::resolvePortDisablePath()
{
    rememberTouchPort();
    // Пока USB-тач не виден, остаётся путь из файла — его и power-cycle.
    if (!m_lastDisablePath.isEmpty() && QFile::exists(m_lastDisablePath)) {
        return m_lastDisablePath;
    }
    return {};
}

bool TouchUsbMonitor::writePortDisable(const QString &path, bool disabled)
{
    const QByteArray payload = disabled ? QByteArrayLiteral("1\n") : QByteArrayLiteral("0\n");
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        if (file.write(payload) == payload.size()) {
            file.close();
            return true;
        }
        file.close();
    }

    if (!isAllowedDisablePath(path)) {
        qWarning().noquote() << "TouchUsbMonitor: cannot write" << path;
        return false;
    }

    // Fallback, если sysfs без записи (права). waitForFinished блокирует GUI-поток.
    QProcess proc;
    proc.start(QStringLiteral("sudo"), {
                   QStringLiteral("-n"),
                   QStringLiteral("/usr/bin/tee"),
                   path
               });
    if (!proc.waitForStarted(400)) {
        qWarning().noquote() << "TouchUsbMonitor: sudo tee failed to start" << path;
        return false;
    }
    proc.write(payload);
    proc.closeWriteChannel();
    if (!proc.waitForFinished(800) || proc.exitCode() != 0) {
        qWarning().noquote() << "TouchUsbMonitor: sudo tee failed" << path
                             << proc.readAllStandardError();
        proc.kill();
        return false;
    }
    return true;
}

