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

void UiClickSound::setActivationMuted(bool muted)
{
    m_activationMuted = muted;
}

void UiClickSound::play(bool force)
{
    QElapsedTimer filterTimer;
    filterTimer.start();
    logClick("play", playAtPercent(kNormalClickPercent, force), filterTimer.elapsed());
}

void UiClickSound::playPreview(int level)
{
    static const int kPreviewPercents[] = {40, 60, 80, 100};
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

bool UiClickSound::previewVolumeSlider(QQuickWindow *window, const QPointF &globalPos)
{
    if (!isOnSettingsVolumeSlider(window, globalPos)) {
        return false;
    }
    const int level = volumeLevelAt(m_volumeSlider.data(), globalPos);
    playPreview(level);
    emit volumeSliderTouched(level);
    return true;
}

int UiClickSound::volumeLevelAt(const QQuickItem *slider, const QPointF &globalPos) const
{
    if (!slider) {
        return 0;
    }

    // Та же геометрия, что QQuickSliderPrivate::positionAt + snapPosition + valueAt:
    // на TouchBegin value слайдера ещё предыдущий.
    const QPointF point = slider->mapFromGlobal(globalPos);
    const qreal from = slider->property("from").toReal();
    const qreal to = slider->property("to").toReal();
    const qreal step = slider->property("stepSize").toReal();
    const bool horizontal = slider->property("orientation").toInt() != Qt::Vertical;

    const QObject *handleObject = slider->property("handle").value<QObject *>();
    const auto *handle = qobject_cast<const QQuickItem *>(handleObject);
    const qreal handleSize = handle
            ? (horizontal ? handle->width() : handle->height())
            : 0.0;
    const qreal offset = handleSize / 2.0;
    const qreal extent = (horizontal
                          ? slider->property("availableWidth").toReal()
                          : slider->property("availableHeight").toReal())
            - handleSize;

    qreal pos = 0.0;
    if (!qFuzzyIsNull(extent)) {
        if (horizontal) {
            if (slider->property("mirrored").toBool()) {
                pos = (slider->width() - point.x()
                       - slider->property("rightPadding").toReal() - offset) / extent;
            } else {
                pos = (point.x() - slider->property("leftPadding").toReal() - offset) / extent;
            }
        } else {
            pos = (slider->height() - point.y()
                   - slider->property("bottomPadding").toReal() - offset) / extent;
        }
    }
    pos = qBound<qreal>(0.0, pos, 1.0);

    const qreal range = to - from;
    if (!qFuzzyIsNull(range) && !qFuzzyIsNull(step)) {
        const qreal effectiveStep = step / range;
        if (!qFuzzyIsNull(effectiveStep)) {
            pos = qRound(pos / effectiveStep) * effectiveStep;
        }
    }

    const qreal span = range * pos;
    const qreal value = qFuzzyIsNull(step)
            ? from + span
            : from + qRound(span / step) * step;
    return qRound(value);
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
    if (!window) {
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
        if (previewVolumeSlider(window, globalPos)) {
            m_lastInput.restart();
            break;
        }
        if (!m_enabled) {
            break;
        }
        PlayTrace trace;
        if (m_activationMuted) {
            trace.decision = "skip-activation";
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
        if (previewVolumeSlider(window, mouseEvent->globalPos())) {
            m_lastInput.restart();
            break;
        }
        if (!m_enabled) {
            break;
        }
        PlayTrace trace;
        if (m_activationMuted) {
            trace.decision = "skip-activation";
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
