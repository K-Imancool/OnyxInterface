#ifndef UICLICKSOUND_H
#define UICLICKSOUND_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

class QQuickItem;
class QQuickWindow;

/**
 * Глобальный звук касания UI: <fotekRoot>/sounds/button0.wav
 * Воспроизведение через Pulse (paplay --volume), без Qt Multimedia/ALSA.
 */
class UiClickSound : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit UiClickSound(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);

    /// Ложные касания во время активации (кроме АСС) не щёлкают.
    /// Превью громкости и play(force) не глушатся.
    void setActivationMuted(bool muted);

    /// Обычный клик: всегда 60% Pulse. force=true — даже если звук выключен.
    Q_INVOKABLE void play(bool force = false);
    /// Превью слайдера громкости: уровни 0..3 → 40/60/80/100%. Всегда играет.
    Q_INVOKABLE void playPreview(int level);
    Q_INVOKABLE void stop();

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void enabledChanged();
    /// Касание слайдера громкости: уровень по координате пальца, не по старому value.
    void volumeSliderTouched(int level);

private:
    struct PlayTrace {
        const char *decision = "nop";
        qint64 sincePlayMs = -1;
        qint64 spawnMs = -1;
    };

    void loadSource();
    PlayTrace playAtPercent(int percent, bool force);
    bool isOnSettingsVolumeSlider(QQuickWindow *window, const QPointF &globalPos);
    bool previewVolumeSlider(QQuickWindow *window, const QPointF &globalPos);
    int volumeLevelAt(const QQuickItem *slider, const QPointF &globalPos) const;
    void logClick(const char *origin, const PlayTrace &trace, qint64 filterMs) const;

    static constexpr qint64 kMinIntervalMs = 220;
    static constexpr int kNormalClickPercent = 60;
    static constexpr int kPulseVolumeNorm = 65536;
    static constexpr const char *kVolumeSliderObjectName = "settingsVolumeSlider";

    QString m_wavPath;
    QString m_paplayBin;
    QPointer<QQuickItem> m_volumeSlider;
    bool m_enabled = true;
    bool m_activationMuted = false;
    bool m_touchSeen = false;
    QElapsedTimer m_lastPlay;
    QElapsedTimer m_lastInput;
};

#endif // UICLICKSOUND_H
