#ifndef UICLICKSOUND_H
#define UICLICKSOUND_H

#include <QElapsedTimer>
#include <QObject>
#include <QSoundEffect>

/**
 * Глобальный звук касания UI: <fotekRoot>/sounds/button0.wav
 */
class UiClickSound : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit UiClickSound(QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);

    qreal volume() const;
    void setVolume(qreal volume);

    /// Ручной клик (например, после смены громкости на слайдере настроек).
    /// force=true — даже если звук касания выключен в настройках.
    Q_INVOKABLE void play(bool force = false);
    Q_INVOKABLE void stop();

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void enabledChanged();
    void volumeChanged();

private:
    void loadSource();
    void playClick(bool force = false);
    bool isOnSettingsVolumeSlider(const QPointF &globalPos) const;

    static constexpr qint64 kMinIntervalMs = 180;
    static constexpr const char *kVolumeSliderObjectName = "settingsVolumeSlider";

    QSoundEffect *m_effect = nullptr;
    bool m_enabled = true;
    qreal m_volume = 1.0;
    QElapsedTimer m_lastPlay;
};

#endif // UICLICKSOUND_H
