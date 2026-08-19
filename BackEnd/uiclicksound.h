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

    /// Обычный клик: всегда 30% Pulse. force=true — даже если звук выключен.
    Q_INVOKABLE void play(bool force = false);
    /// Превью слайдера громкости: уровни 0..3 → 20/30/45/60%. Всегда играет.
    Q_INVOKABLE void playPreview(int level);
    Q_INVOKABLE void stop();

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void enabledChanged();

private:
    void loadSource();
    void playAtPercent(int percent, bool force);
    bool isOnSettingsVolumeSlider(QQuickWindow *window, const QPointF &globalPos);

    static constexpr qint64 kMinIntervalMs = 220;
    static constexpr int kNormalClickPercent = 30;
    static constexpr int kPulseVolumeNorm = 65536;
    static constexpr const char *kVolumeSliderObjectName = "settingsVolumeSlider";

    QString m_wavPath;
    QString m_paplayBin;
    QPointer<QQuickItem> m_volumeSlider;
    bool m_enabled = true;
    bool m_touchSeen = false;
    QElapsedTimer m_lastPlay;
};

#endif // UICLICKSOUND_H
