#ifndef UICLICKSOUND_H
#define UICLICKSOUND_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>

class ClickAudioEngine;
class QQuickItem;
class QQuickWindow;
class QThread;

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
    ~UiClickSound() override;

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
    bool isOnSettingsVolumeSlider(QQuickWindow *window, const QPointF &globalPos);

    static constexpr qint64 kMinIntervalMs = 220;
    static constexpr const char *kVolumeSliderObjectName = "settingsVolumeSlider";

    QThread *m_audioThread = nullptr;
    ClickAudioEngine *m_engine = nullptr;
    QPointer<QQuickItem> m_volumeSlider;
    bool m_enabled = true;
    bool m_touchSeen = false;
    qreal m_volume = 1.0;
    QElapsedTimer m_lastPlay;
};

#endif // UICLICKSOUND_H
