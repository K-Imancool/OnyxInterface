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
#include <QTouchEvent>

UiClickSound::UiClickSound(QObject *parent)
    : QObject(parent)
{
    m_lastPlay.invalidate();
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
    playAtPercent(kNormalClickPercent, force);
}

void UiClickSound::playPreview(int level)
{
    static const int kPreviewPercents[] = {20, 30, 45, 60};
    const int clamped = qBound(0, level, 3);
    playAtPercent(kPreviewPercents[clamped], true);
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
        m_touchSeen = true;
        const QPointF globalPos = touchEvent->touchPoints().first().screenPos();
        if (isOnSettingsVolumeSlider(window, globalPos)) {
            break;
        }
        play();
        break;
    }
    case QEvent::MouseButtonPress: {
        if (m_touchSeen) {
            break;
        }
        const auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }
        if (mouseEvent->source() != Qt::MouseEventNotSynthesized) {
            break;
        }
        if (isOnSettingsVolumeSlider(window, mouseEvent->globalPos())) {
            break;
        }
        play();
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

void UiClickSound::playAtPercent(int percent, bool force)
{
    if ((!force && !m_enabled) || m_wavPath.isEmpty()) {
        return;
    }
    if (!force && m_lastPlay.isValid() && m_lastPlay.elapsed() < kMinIntervalMs) {
        return;
    }
    m_lastPlay.restart();

    const int pulseVolume = qBound(0,
                                   qRound(percent / 100.0 * kPulseVolumeNorm),
                                   kPulseVolumeNorm);
    QProcess::startDetached(m_paplayBin,
                            {QStringLiteral("--volume"),
                             QString::number(pulseVolume),
                             m_wavPath});
}
