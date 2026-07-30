#ifndef GSTREAMERVIDEOPLAYER_H
#define GSTREAMERVIDEOPLAYER_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <gst/gst.h>

class QQuickItem;
class QSocketNotifier;

class GStreamerVideoPlayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QQuickItem *videoItem READ videoItem WRITE setVideoItem NOTIFY videoItemChanged)
    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(MediaStatus status READ status NOTIFY statusChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    enum PlaybackState {
        StoppedState,
        PlayingState,
        PausedState
    };
    Q_ENUM(PlaybackState)

    enum MediaStatus {
        NoMedia,
        Loading,
        Loaded,
        Buffering,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(MediaStatus)

    explicit GStreamerVideoPlayer(QObject *parent = nullptr);
    ~GStreamerVideoPlayer() override;

    static void registerQmlType();

    QUrl source() const;
    void setSource(const QUrl &source);

    QQuickItem *videoItem() const;
    void setVideoItem(QQuickItem *videoItem);

    PlaybackState playbackState() const;
    MediaStatus status() const;
    qint64 position() const;
    qint64 duration() const;
    bool seekable() const;
    bool buffering() const;

    qreal volume() const;
    void setVolume(qreal volume);

    bool muted() const;
    void setMuted(bool muted);

    QString errorString() const;

    Q_INVOKABLE QStringList scanVideoFiles(const QString &folderPath) const;
    Q_INVOKABLE void setLocalFile(const QString &filePath);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void shutdown();

Q_SIGNALS:
    void sourceChanged();
    void videoItemChanged();
    void playbackStateChanged();
    void statusChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void bufferingChanged();
    void volumeChanged();
    void mutedChanged();
    void errorStringChanged();
    void errorOccurred(const QString &message);

private Q_SLOTS:
    void drainBus();
    void updatePosition();

private:
    bool ensurePipeline();
    bool requestPlayingState();
    void verifyPlaybackStarted();
    void schedulePipelinePreparation(int delayMs);
    GstElement *createVideoSinkBin();
    void destroyPipeline();
    void processMessage(GstMessage *message);
    void updateDuration();
    void updateSeekable();
    void finishPendingStop();

    void setPlaybackState(PlaybackState state);
    void setStatus(MediaStatus status);
    void setPosition(qint64 position);
    void setDuration(qint64 duration);
    void setSeekable(bool seekable);
    void setBuffering(bool buffering);
    void setError(const QString &message, const QString &debugDetails = QString());
    void resetMediaState();

    QUrl m_source;
    QPointer<QQuickItem> m_videoItem;
    PlaybackState m_playbackState = StoppedState;
    MediaStatus m_status = NoMedia;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    bool m_seekable = false;
    bool m_buffering = false;
    qreal m_volume = 0.8;
    bool m_muted = false;
    QString m_errorString;

    GstElement *m_pipeline = nullptr;
    GstElement *m_qmlSink = nullptr;
    GstBus *m_bus = nullptr;
    GPollFD m_busPollFd {};
    QSocketNotifier *m_busNotifier = nullptr;
    QTimer m_positionTimer;
    QTimer m_playStartTimer;
    int m_playStartAttempts = 0;
    bool m_prepareRetryScheduled = false;
    bool m_playRequested = false;
    bool m_stopPending = false;
};

#endif // GSTREAMERVIDEOPLAYER_H
