#include "TouchUsbMonitor.h"
#include "DeviceLogManager.h"
#include "periphhandler.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QMouseEvent>
#include <QProcess>
#include <QTouchEvent>
#include <QTimer>

namespace {

constexpr char kRecoverHelper[] = "/usr/local/sbin/roc-touch-usb-recover";
constexpr char kVid[] = "222a";
constexpr char kPid[] = "0001";

constexpr int kFirstCheckMs = 800;
constexpr int kPollOkMs = 30000;
constexpr int kPollMissingMs = 700;
constexpr int kMissingConfirmNeeded = 4;
constexpr int kRecoverTimeoutMs = 45000;
constexpr int kPostRecoverRecheckMs = 2000;
constexpr int kMaxRecoverAttempts = 2;
constexpr qint64 kTouchEventTrustMs = 5 * 60 * 1000;
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

} // namespace

TouchUsbMonitor::TouchUsbMonitor(DeviceLogManager *deviceLog, QObject *parent)
    : QObject(parent)
    , m_deviceLog(deviceLog)
    , m_timer(new QTimer(this))
    , m_process(new QProcess(this))
{
    m_timer->setInterval(kPollOkMs);
    connect(m_timer, &QTimer::timeout, this, &TouchUsbMonitor::check);

    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus status) {
                if (status != QProcess::NormalExit) {
                    onRecoverFinished(-1);
                    return;
                }
                onRecoverFinished(exitCode);
            });

    if (QCoreApplication *app = QCoreApplication::instance()) {
        app->installEventFilter(this);
    }

    QTimer::singleShot(kFirstCheckMs, this, [this]() {
        check();
        m_timer->start();
    });
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
    if (!active) {
        return;
    }
    if (isActivationMute()) {
        qWarning("TouchUsbMonitor: mute pointer events during activation");
    } else {
        qWarning("TouchUsbMonitor: AutoStartStop — pointer allowed (touch stops activation)");
    }
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

bool TouchUsbMonitor::touchEventRecent() const
{
    return m_lastTouchEvent.isValid() && m_lastTouchEvent.elapsed() < kTouchEventTrustMs;
}

void TouchUsbMonitor::onLiveTouchEvidence()
{
    m_lastTouchEvent.start();
    if (m_recovering) {
        return;
    }
    if (m_touchOk && !m_screenFaultShown && m_missingConfirmCount == 0) {
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

void TouchUsbMonitor::updatePollInterval()
{
    const int interval = (!m_touchOk || m_faultActive || m_screenFaultShown
                          || m_missingConfirmCount > 0)
            ? kPollMissingMs
            : kPollOkMs;
    if (m_timer->interval() != interval) {
        m_timer->setInterval(interval);
    }
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
    updatePollInterval();
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
    updatePollInterval();
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
        m_recoverAttempts = 0;
        m_missingConfirmCount = 0;
        clearScreenFault();
        updatePollInterval();
        return;
    }

    updatePollInterval();

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
    if (m_recovering) {
        return;
    }

    if (touchInputPresent()) {
        applyState(QStringLiteral("ok"),
                   QStringLiteral("WINSTARTP Mouse present"),
                   false);
        return;
    }

    if (touchEventRecent()) {
        applyState(QStringLiteral("ok"),
                   QStringLiteral("recent touch event"),
                   false);
        return;
    }

    ++m_missingConfirmCount;
    QString sysfsPath;
    const bool usbPresent = touchUsbDevicePresent(&sysfsPath);
    const QString detail = usbPresent
            ? QStringLiteral("no WINSTARTP Mouse (usb %1, confirm %2/%3)")
              .arg(sysfsPath)
              .arg(m_missingConfirmCount)
              .arg(kMissingConfirmNeeded)
            : QStringLiteral("no WINSTARTP Mouse (usb absent, confirm %1/%2)")
              .arg(m_missingConfirmCount)
              .arg(kMissingConfirmNeeded);

    if (m_missingConfirmCount < kMissingConfirmNeeded) {
        applyState(QStringLiteral("lost"), detail, false);
        qWarning().noquote() << "TouchUsbMonitor: observe" << detail;
        updatePollInterval();
        return;
    }

    if (m_recoverAttempts >= kMaxRecoverAttempts) {
        m_recoverAttempts = 0;
    }
    startRecover(detail);
}

void TouchUsbMonitor::startRecover(const QString &detail)
{
    if (m_process->state() != QProcess::NotRunning) {
        return;
    }
    if (m_recoverAttempts >= kMaxRecoverAttempts) {
        applyState(QStringLiteral("failed"),
                   QStringLiteral("recover attempts exhausted: %1").arg(detail),
                   true);
        raiseScreenFault();
        return;
    }

    ++m_recoverAttempts;
    applyState(QStringLiteral("lost"), detail, true);
    setRecovering(true);
    ++m_recoverGeneration;
    const int generation = m_recoverGeneration;

    qWarning().noquote() << "TouchUsbMonitor: recover attempt" << m_recoverAttempts
                         << detail;

    m_process->start(QStringLiteral("sudo"), {
                         QStringLiteral("-n"),
                         QString::fromLatin1(kRecoverHelper)
                     });
    if (!m_process->waitForStarted(500)) {
        setRecovering(false);
        applyState(QStringLiteral("failed"),
                   QStringLiteral("sudo -n %1 failed to start").arg(QLatin1String(kRecoverHelper)),
                   true);
        raiseScreenFault();
        return;
    }

    QTimer::singleShot(kRecoverTimeoutMs, this, [this, generation]() {
        if (generation != m_recoverGeneration) {
            return;
        }
        if (m_process->state() == QProcess::NotRunning) {
            return;
        }
        m_process->kill();
        setRecovering(false);
        applyState(QStringLiteral("failed"),
                   QStringLiteral("recover helper timed out"),
                   true);
        raiseScreenFault();
    });
}

void TouchUsbMonitor::schedulePostRecoverRecheck()
{
    const int generation = m_recoverGeneration;
    QTimer::singleShot(kPostRecoverRecheckMs, this, [this, generation]() {
        if (generation != m_recoverGeneration || m_recovering) {
            return;
        }
        if (touchInputPresent()) {
            applyState(QStringLiteral("recovered"),
                       QStringLiteral("WINSTARTP Mouse after settle"),
                       false);
            return;
        }
        if (m_recoverAttempts < kMaxRecoverAttempts) {
            startRecover(QStringLiteral("retry after settle, still no WINSTARTP Mouse"));
            return;
        }
        applyState(QStringLiteral("failed"),
                   QStringLiteral("no WINSTARTP Mouse after recover"),
                   true);
        raiseScreenFault();
    });
}

void TouchUsbMonitor::onRecoverFinished(int exitCode)
{
    ++m_recoverGeneration;
    setRecovering(false);

    const QString out = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    if (!out.isEmpty()) {
        qWarning().noquote() << "TouchUsbMonitor: helper stdout:" << out;
    }
    if (!err.isEmpty()) {
        qWarning().noquote() << "TouchUsbMonitor: helper stderr:" << err;
    }

    if (touchInputPresent()) {
        applyState(QStringLiteral("recovered"),
                   QStringLiteral("power-cycle restored WINSTARTP Mouse"),
                   false);
        return;
    }

    if (exitCode == 0 || m_recoverAttempts < kMaxRecoverAttempts) {
        applyState(QStringLiteral("lost"),
                   QStringLiteral("recover exit %1, waiting settle").arg(exitCode),
                   true);
        schedulePostRecoverRecheck();
        return;
    }

    applyState(QStringLiteral("failed"),
               QStringLiteral("recover exit %1%2")
               .arg(exitCode)
               .arg(err.isEmpty() ? QString() : (QStringLiteral(": ") + err)),
               true);
    raiseScreenFault();
}
