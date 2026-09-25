#include "uiclicksound.h"
#include "apppaths.h"

#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QThread>
#include <QTouchEvent>

namespace {

bool clickLatencyLogEnabled()
{
    const QByteArray raw = qgetenv("ONYX_CLICK_LATENCY");
    return raw.isEmpty() || raw != "0";
}

}

UiClickSound::UiClickSound(QObject *parent)
    : QObject(parent)
{
    m_lastPlay.invalidate();
    m_lastInput.invalidate();
    loadSource();
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

bool UiClickSound::enabled() const
{
    return m_enabled;
}

void UiClickSound::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();
}

void UiClickSound::play(bool force)
{
    QElapsedTimer filterTimer;
    filterTimer.start();
    logClick("play", playAtPercent(kNormalClickPercent, force), filterTimer.elapsed());
}

void UiClickSound::playPreview(int level)
{
    static const int kPreviewPercents[] = {20, 30, 45, 60};
    const int clamped = qBound(0, level, 3);
    QElapsedTimer filterTimer;
    filterTimer.start();
    logClick("preview", playAtPercent(kPreviewPercents[clamped], true), filterTimer.elapsed());
}

void UiClickSound::stop()
{
}

bool UiClickSound::isOnSettingsVolumeSlider(QQuickWindow *window, const QPointF &globalPos)
{
    if (!window) {
        return false;
    }
    if (m_volumeSlider.isNull()
            || m_volumeSlider->window() != window) {
        m_volumeSlider = window->findChild<QQuickItem *>(
                    QLatin1String(kVolumeSliderObjectName));
    }
    QQuickItem *slider = m_volumeSlider.data();
    if (!slider || !slider->isVisible() || !slider->isEnabled()) {
        return false;
    }

    const QPointF local = slider->mapFromGlobal(globalPos);
    return QRectF(0, 0, slider->width(), slider->height()).contains(local);
}

void UiClickSound::logClick(const char *origin, const PlayTrace &trace, qint64 filterMs) const
{
    if (!clickLatencyLogEnabled()) {
        return;
    }
    const bool guiThread = qApp && QThread::currentThread() == qApp->thread();
    const qint64 sinceInputMs = m_lastInput.isValid() ? m_lastInput.elapsed() : -1;
//    qDebug("click-latency origin=%s decision=%s guiThread=%d tid=%llu "
//           "sinceInput=%lld sincePlay=%lld spawnMs=%lld filterMs=%lld",
//           origin,
//           trace.decision,
//           int(guiThread),
//           static_cast<qulonglong>(reinterpret_cast<quintptr>(QThread::currentThreadId())),
//           sinceInputMs,
//           trace.sincePlayMs,
//           trace.spawnMs,
//           filterMs);
}

bool UiClickSound::eventFilter(QObject *watched, QEvent *event)
{
    auto *window = qobject_cast<QQuickWindow *>(watched);
    if (!window || !m_enabled) {
        return false;
    }

    switch (event->type()) {
    case QEvent::TouchBegin: {
        const auto *touchEvent = static_cast<const QTouchEvent *>(event);
        if (touchEvent->touchPoints().isEmpty()) {
            break;
        }
        QElapsedTimer filterTimer;
        filterTimer.start();
        m_touchSeen = true;
        const QPointF globalPos = touchEvent->touchPoints().first().screenPos();
        PlayTrace trace;
        if (isOnSettingsVolumeSlider(window, globalPos)) {
            trace.decision = "skip-slider";
            trace.sincePlayMs = m_lastPlay.isValid() ? m_lastPlay.elapsed() : -1;
        } else {
            trace = playAtPercent(kNormalClickPercent, false);
        }
        logClick("touch", trace, filterTimer.elapsed());
        m_lastInput.restart();
        break;
    }
    case QEvent::MouseButtonPress: {
        QElapsedTimer filterTimer;
        filterTimer.start();
        if (m_touchSeen) {
            PlayTrace trace;
            trace.decision = "ignore-after-touch";
            trace.sincePlayMs = m_lastPlay.isValid() ? m_lastPlay.elapsed() : -1;
            logClick("mouse", trace, filterTimer.elapsed());
            break;
        }
        const auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }
        if (mouseEvent->source() != Qt::MouseEventNotSynthesized) {
            PlayTrace trace;
            trace.decision = "ignore-synthesized";
            trace.sincePlayMs = m_lastPlay.isValid() ? m_lastPlay.elapsed() : -1;
            logClick("mouse", trace, filterTimer.elapsed());
            break;
        }
        PlayTrace trace;
        if (isOnSettingsVolumeSlider(window, mouseEvent->globalPos())) {
            trace.decision = "skip-slider";
            trace.sincePlayMs = m_lastPlay.isValid() ? m_lastPlay.elapsed() : -1;
        } else {
            trace = playAtPercent(kNormalClickPercent, false);
        }
        logClick("mouse", trace, filterTimer.elapsed());
        m_lastInput.restart();
        break;
    }
    default:
        break;
    }

    return false;
}

void UiClickSound::loadSource()
{
    m_wavPath = AppPaths::instance().soundsDir()
            + QStringLiteral("/button0.wav");
    qInfo("UiClickSound: click sound %s", qPrintable(m_wavPath));

    if (!QFileInfo::exists(m_wavPath)) {
        qWarning("UiClickSound: file not found: %s", qPrintable(m_wavPath));
        m_wavPath.clear();
        return;
    }

    m_paplayBin = QStandardPaths::findExecutable(QStringLiteral("paplay"));
    if (m_paplayBin.isEmpty()) {
        m_paplayBin = QStringLiteral("/usr/bin/paplay");
    }
}

UiClickSound::PlayTrace UiClickSound::playAtPercent(int percent, bool force)
{
    PlayTrace trace;
    trace.sincePlayMs = m_lastPlay.isValid() ? m_lastPlay.elapsed() : -1;
    if ((!force && !m_enabled) || m_wavPath.isEmpty()) {
        trace.decision = m_wavPath.isEmpty() ? "skip-nofile" : "skip-disabled";
        return trace;
    }
    if (!force && trace.sincePlayMs >= 0 && trace.sincePlayMs < kMinIntervalMs) {
        trace.decision = "skip-interval";
        return trace;
    }
    m_lastPlay.restart();

    const int pulseVolume = qBound(0,
                                   qRound(percent / 100.0 * kPulseVolumeNorm),
                                   kPulseVolumeNorm);
    QElapsedTimer spawnTimer;
    spawnTimer.start();
    const bool started = QProcess::startDetached(m_paplayBin,
                            {QStringLiteral("--volume"),
                             QString::number(pulseVolume),
                             m_wavPath});
    trace.decision = started ? "paplay" : "paplay-failed";
    trace.spawnMs = spawnTimer.elapsed();
    return trace;
}
