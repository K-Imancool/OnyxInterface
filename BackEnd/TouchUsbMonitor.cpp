#include "TouchUsbMonitor.h"
#include "DeviceLogManager.h"
#include "periphhandler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QMouseEvent>
#include <QProcess>
#include <QTouchEvent>
#include <QTimer>

namespace {

constexpr char kVid[] = "222a";
constexpr char kPid[] = "0001";
constexpr char kPortFileName[] = "/.config/UserInterface/touch-usb-port";
constexpr int kHubBusFirst = 1;
constexpr int kHubBusLast = 7;

constexpr int kFaultDeadlineMs = 15000;
constexpr int kPortToggleMs = 2000;
constexpr int kPortPhaseOff = 1;
constexpr int kPortPhaseOn = 2;
constexpr int kPortPollMs = 250;
constexpr int kPortProbeIntervalMs = 500;
constexpr int kPortProbeTries = 20;
// Порог по X: быстрый перескок / ghost после отпускания.
constexpr qreal kMaxDxPx = 50.0;
// Окно после release, в котором режем повторный press с |Δx| > 50.
constexpr qint64 kPostReleaseGuardMs = 200;
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

} // namespace

TouchUsbMonitor::TouchUsbMonitor(DeviceLogManager *deviceLog, QObject *parent)
    : QObject(parent)
    , m_deviceLog(deviceLog)
    , m_stepTimer(new QTimer(this))
    , m_deadlineTimer(new QTimer(this))
    , m_portProbeTimer(new QTimer(this))
    , m_settleTimer(new QTimer(this))
{
    m_stepTimer->setSingleShot(true);
    connect(m_stepTimer, &QTimer::timeout, this, &TouchUsbMonitor::onPortStep);
    m_deadlineTimer->setSingleShot(true);
    connect(m_deadlineTimer, &QTimer::timeout, this, &TouchUsbMonitor::onRecoverDeadline);

    m_portProbeTimer->setInterval(kPortProbeIntervalMs);
    connect(m_portProbeTimer, &QTimer::timeout, this, &TouchUsbMonitor::probeTouchPort);

    m_settleTimer->setInterval(kPortPollMs);
    connect(m_settleTimer, &QTimer::timeout, this, &TouchUsbMonitor::onSettlePoll);

    if (QCoreApplication *app = QCoreApplication::instance()) {
        app->installEventFilter(this);
    }

    loadRememberedPort();
    m_portProbeLeft = kPortProbeTries;
    probeTouchPort();
    if (m_portProbeTimer && m_portProbeLeft > 0) {
        m_portProbeTimer->start();
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
        if (isActivationMute()) {
            qWarning("TouchUsbMonitor: mute pointer events during activation");
        } else {
            qWarning("TouchUsbMonitor: AutoStartStop — pointer allowed (touch stops activation)");
        }
        return;
    }
    if (!m_activationWasOn) {
        return;
    }
    m_activationWasOn = false;
    beginPostActivationCheck();
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
    m_postReleaseArmed = false;
    m_contactPos = screenPos;
    m_lastAcceptedPos = screenPos;
    m_postReleasePos = screenPos;
    m_pointerDownSince.start();
}

void TouchUsbMonitor::endContact(const QPointF &screenPos)
{
    m_pointerDown = false;
    m_postReleaseArmed = true;
    m_postReleasePos = screenPos;
    m_lastAcceptedPos = screenPos;
    m_postReleaseSince.start();
}

void TouchUsbMonitor::clearContactState()
{
    m_pointerDown = false;
    m_postReleaseArmed = false;
    m_contactPos = {};
    m_lastAcceptedPos = {};
    m_postReleasePos = {};
}

bool TouchUsbMonitor::postReleaseGuardActive() const
{
    return m_postReleaseArmed
            && m_postReleaseSince.isValid()
            && m_postReleaseSince.elapsed() < kPostReleaseGuardMs;
}

void TouchUsbMonitor::expireStalePointerState()
{
    if (m_pointerDown
            && m_pointerDownSince.isValid()
            && m_pointerDownSince.elapsed() > kPointerLockTimeoutMs) {
        clearContactState();
    }
    if (m_postReleaseArmed && !postReleaseGuardActive()) {
        m_postReleaseArmed = false;
    }
}

bool TouchUsbMonitor::shouldRejectPress(const QPointF &screenPos) const
{
    // Во время удержания: близкий press (пара touch↔mouse) — пропускаем;
    // далёкий по X — ghost.
    if (m_pointerDown) {
        return qAbs(screenPos.x() - m_contactPos.x()) > kMaxDxPx;
    }
    // После отпускания: слишком быстрый повтор с |Δx| > 50 от первого касания.
    if (postReleaseGuardActive()
            && qAbs(screenPos.x() - m_postReleasePos.x()) > kMaxDxPx) {
        return true;
    }
    return false;
}

bool TouchUsbMonitor::shouldRejectMove(const QPointF &screenPos) const
{
    if (!m_pointerDown) {
        return false;
    }
    // Быстрый перескок по X во время удержания.
    return qAbs(screenPos.x() - m_lastAcceptedPos.x()) > kMaxDxPx;
}

bool TouchUsbMonitor::filterMouseEvent(QMouseEvent *mouse)
{
    const QEvent::Type type = mouse->type();
    const QPointF pos = mouse->screenPos();
    expireStalePointerState();

    if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick) {
        if (!(mouse->buttons() & Qt::LeftButton)
                && mouse->button() != Qt::LeftButton) {
            return false;
        }
        if (shouldRejectPress(pos)) {
            // Пока сыпется мусор — продлеваем post-release окно.
            if (!m_pointerDown && postReleaseGuardActive()) {
                m_postReleaseSince.start();
            }
            return true;
        }
        // Первое касание / близкий парный press — всегда в UI.
        if (!m_pointerDown) {
            beginContact(pos);
        }
        onLiveTouchEvidence();
        return false;
    }

    if (type == QEvent::MouseMove) {
        if (!(mouse->buttons() & Qt::LeftButton) || !m_pointerDown) {
            return false;
        }
        if (shouldRejectMove(pos)) {
            return true;
        }
        m_lastAcceptedPos = pos;
        return false;
    }

    if (type == QEvent::MouseButtonRelease) {
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }
        endContact(m_pointerDown ? m_lastAcceptedPos : pos);
        return false;
    }

    return false;
}

bool TouchUsbMonitor::filterTouchEvent(QTouchEvent *touch)
{
    const QEvent::Type type = touch->type();
    const auto points = touch->touchPoints();
    expireStalePointerState();

    if (type == QEvent::TouchBegin) {
        if (points.isEmpty()) {
            return false;
        }
        const QPointF pos = points.first().screenPos();
        if (shouldRejectPress(pos)) {
            if (!m_pointerDown && postReleaseGuardActive()) {
                m_postReleaseSince.start();
            }
            return true;
        }
        if (!m_pointerDown) {
            beginContact(pos);
        }
        onLiveTouchEvidence();
        return false;
    }

    if (type == QEvent::TouchUpdate) {
        if (!m_pointerDown || points.isEmpty()) {
            return false;
        }
        // Несколько точек сразу — горизонтальный EMI-мусор.
        if (points.size() > 1) {
            return true;
        }
        const QPointF pos = points.first().screenPos();
        if (shouldRejectMove(pos)) {
            return true;
        }
        m_lastAcceptedPos = pos;
        return false;
    }

    if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
        if (m_pointerDown) {
            endContact(m_lastAcceptedPos);
        }
        return false;
    }

    return false;
}

bool TouchUsbMonitor::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    if (!isPointerEvent(event->type())) {
        return QObject::eventFilter(watched, event);
    }

    if (isActivationMute()) {
        return true;
    }

    bool filtered = false;
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
        filtered = filterMouseEvent(static_cast<QMouseEvent *>(event));
        break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        filtered = filterTouchEvent(static_cast<QTouchEvent *>(event));
        break;
    default:
        break;
    }

    if (filtered) {
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void TouchUsbMonitor::onLiveTouchEvidence()
{
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
    QFile devices(QStringLiteral("/proc/bus/input/devices"));
    if (!devices.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    return devices.readAll().contains("WINSTARTP Mouse");
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
        if (readSysfsText(base + QStringLiteral("/idVendor")) != QLatin1String(kVid)) {
            continue;
        }
        if (readSysfsText(base + QStringLiteral("/idProduct")) != QLatin1String(kPid)) {
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

void TouchUsbMonitor::beginPostActivationCheck()
{
    m_stepTimer->stop();
    m_deadlineTimer->stop();
    m_settleTimer->stop();
    m_portPhase = 0;
    m_cycle = 0;
    rememberTouchPort();

    if (touchInputPresent()) {
        setRecovering(false);
        applyState(QStringLiteral("ok"),
                   QStringLiteral("WINSTARTP Mouse present"),
                   false);
        return;
    }

    qWarning("TouchUsbMonitor: WINSTARTP Mouse missing after activation, port reset");
    setRecovering(true);
    applyState(QStringLiteral("lost"),
               QStringLiteral("no WINSTARTP Mouse after activation"),
               false);
    m_checkStarted.start();
    m_deadlineTimer->start(kFaultDeadlineMs);
    kickPortCycle();
}

void TouchUsbMonitor::kickPortCycle()
{
    if (!m_recovering) {
        return;
    }
    if (touchInputPresent()) {
        finishRecovered(QStringLiteral("WINSTARTP Mouse present"));
        return;
    }
    if (m_checkStarted.isValid() && m_checkStarted.elapsed() >= kFaultDeadlineMs) {
        noteOngoingFault(QStringLiteral("no WINSTARTP Mouse, port reset continues"));
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

    if (touchInputPresent()) {
        finishRecovered(QStringLiteral("WINSTARTP Mouse present"));
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
    if (touchInputPresent()) {
        finishRecovered(QStringLiteral("WINSTARTP Mouse before deadline"));
        return;
    }
    noteOngoingFault(QStringLiteral("no WINSTARTP Mouse within 15s, port reset continues"));
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

void TouchUsbMonitor::onSettlePoll()
{
    if (!m_screenFaultShown || touchInputPresent()) {
        if (m_screenFaultShown && touchInputPresent() && !m_recovering) {
            qWarning("TouchUsbMonitor: WINSTARTP Mouse is back, clearing 09");
            rememberTouchPort();
            applyState(QStringLiteral("recovered"),
                       QStringLiteral("WINSTARTP Mouse present after fault"),
                       false);
        }
        m_settleTimer->stop();
    }
}

void TouchUsbMonitor::finishRecovered(const QString &detail)
{
    if (!m_recovering && m_state == QLatin1String("ok")) {
        return;
    }
    m_stepTimer->stop();
    m_deadlineTimer->stop();
    m_settleTimer->stop();
    m_portPhase = 0;
    setRecovering(false);
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
    if (rememberTouchPort()) {
        m_portProbeTimer->stop();
        m_portProbeLeft = 0;
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
    const qint64 left = 800;

    QProcess proc;
    proc.start(QStringLiteral("sudo"), {
                   QStringLiteral("-n"),
                   QStringLiteral("/usr/bin/tee"),
                   path
               });
    if (!proc.waitForStarted(int(qMin(qint64(400), qMax(qint64(1), left))))) {
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

