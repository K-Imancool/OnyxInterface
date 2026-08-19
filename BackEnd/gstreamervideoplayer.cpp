#include "gstreamervideoplayer.h"

#include <QDir>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSocketNotifier>
#include <QtGlobal>
#include <qqml.h>

#include <gst/video/gstvideodecoder.h>

Q_LOGGING_CATEGORY(lcGStreamerVideo, "video.gstreamer")

namespace {

constexpr int kPositionUpdateIntervalMs = 250;
constexpr int kPlayStartRetryIntervalMs = 250;
constexpr int kMaxPlayStartAttempts = 20;

QString gstMessageToQString(const gchar *message)
{
    return message ? QString::fromUtf8(message) : QString();
}

void unrefElement(GstElement *element)
{
    if (element) {
        gst_object_unref(element);
    }
}

void disableVideoDecoderQos(GstBin *bin, GstBin *subBin,
                            GstElement *element, gpointer userData)
{
    Q_UNUSED(bin)
    Q_UNUSED(subBin)
    Q_UNUSED(userData)

    if (!GST_IS_VIDEO_DECODER(element)) {
        return;
    }

    g_object_set(G_OBJECT(element), "qos", FALSE, nullptr);
    qCInfo(lcGStreamerVideo)
            << "QoS frame dropping disabled for"
            << GST_ELEMENT_NAME(element);
}

} // namespace

GStreamerVideoPlayer::GStreamerVideoPlayer(QObject *parent)
    : QObject(parent)
{
    m_positionTimer.setInterval(kPositionUpdateIntervalMs);
    m_positionTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_positionTimer, &QTimer::timeout,
            this, &GStreamerVideoPlayer::updatePosition);

    m_playStartTimer.setInterval(kPlayStartRetryIntervalMs);
    m_playStartTimer.setSingleShot(true);
    m_playStartTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_playStartTimer, &QTimer::timeout,
            this, &GStreamerVideoPlayer::verifyPlaybackStarted);
}

GStreamerVideoPlayer::~GStreamerVideoPlayer()
{
    shutdown();
}

void GStreamerVideoPlayer::registerQmlType()
{
    qmlRegisterType<GStreamerVideoPlayer>("BackEnd", 1, 0,
                                          "GStreamerVideoPlayer");
}

QUrl GStreamerVideoPlayer::source() const
{
    return m_source;
}

void GStreamerVideoPlayer::setSource(const QUrl &source)
{
    if (m_source == source) {
        if (!m_pipeline && !m_source.isEmpty() && m_videoItem) {
            resetMediaState();
            schedulePipelinePreparation(50);
        }
        return;
    }

    m_playRequested = false;
    m_stopPending = false;
    destroyPipeline();
    m_source = source;
    resetMediaState();
    emit sourceChanged();

    if (!m_source.isEmpty() && m_videoItem) {
        // Даём render thread освободить GL-текстуры предыдущего pipeline.
        schedulePipelinePreparation(50);
    }
}

QQuickItem *GStreamerVideoPlayer::videoItem() const
{
    return m_videoItem.data();
}

void GStreamerVideoPlayer::setVideoItem(QQuickItem *videoItem)
{
    if (m_videoItem == videoItem) {
        return;
    }

    destroyPipeline();
    m_videoItem = videoItem;
    emit videoItemChanged();

    if (!m_source.isEmpty() && m_videoItem) {
        schedulePipelinePreparation(0);
    }
}

GStreamerVideoPlayer::PlaybackState GStreamerVideoPlayer::playbackState() const
{
    return m_playbackState;
}

GStreamerVideoPlayer::MediaStatus GStreamerVideoPlayer::status() const
{
    return m_status;
}

qint64 GStreamerVideoPlayer::position() const
{
    return m_position;
}

qint64 GStreamerVideoPlayer::duration() const
{
    return m_duration;
}

bool GStreamerVideoPlayer::seekable() const
{
    return m_seekable;
}

bool GStreamerVideoPlayer::buffering() const
{
    return m_buffering;
}

qreal GStreamerVideoPlayer::volume() const
{
    return m_volume;
}

void GStreamerVideoPlayer::setVolume(qreal volume)
{
    const qreal boundedVolume = qBound<qreal>(0.0, volume, 1.0);
    if (qFuzzyCompare(m_volume, boundedVolume)) {
        return;
    }

    m_volume = boundedVolume;
    if (m_pipeline) {
        g_object_set(G_OBJECT(m_pipeline), "volume",
                     static_cast<gdouble>(m_volume), nullptr);
    }
    emit volumeChanged();
}

bool GStreamerVideoPlayer::muted() const
{
    return m_muted;
}

void GStreamerVideoPlayer::setMuted(bool muted)
{
    if (m_muted == muted) {
        return;
    }

    m_muted = muted;
    if (m_pipeline) {
        g_object_set(G_OBJECT(m_pipeline), "mute", m_muted, nullptr);
    }
    emit mutedChanged();
}

QString GStreamerVideoPlayer::errorString() const
{
    return m_errorString;
}

QStringList GStreamerVideoPlayer::scanVideoFiles(
        const QString &folderPath) const
{
    QDir directory(folderPath);
    if (!directory.exists()) {
        return {};
    }

    directory.setNameFilters({
        QStringLiteral("*.mp4"), QStringLiteral("*.MP4"),
        QStringLiteral("*.avi"), QStringLiteral("*.AVI"),
        QStringLiteral("*.mkv"), QStringLiteral("*.MKV"),
        QStringLiteral("*.mov"), QStringLiteral("*.MOV"),
        QStringLiteral("*.wmv"), QStringLiteral("*.WMV"),
        QStringLiteral("*.flv"), QStringLiteral("*.FLV"),
        QStringLiteral("*.webm"), QStringLiteral("*.WEBM"),
        QStringLiteral("*.m4v"), QStringLiteral("*.M4V"),
        QStringLiteral("*.mpeg"), QStringLiteral("*.MPEG"),
        QStringLiteral("*.mpg"), QStringLiteral("*.MPG")
    });
    directory.setFilter(QDir::Files | QDir::Readable);
    directory.setSorting(QDir::Name);
    return directory.entryList();
}

void GStreamerVideoPlayer::setLocalFile(const QString &filePath)
{
    setSource(QUrl::fromLocalFile(filePath));
}

void GStreamerVideoPlayer::play()
{
    m_playRequested = true;
    m_stopPending = false;
    m_playStartAttempts = 0;
    if (!ensurePipeline()) {
        return;
    }

    if (m_status == EndOfMedia) {
        seek(0);
    }

    requestPlayingState();
}

void GStreamerVideoPlayer::pause()
{
    m_playRequested = false;
    m_stopPending = false;
    m_playStartTimer.stop();
    m_playStartAttempts = 0;
    if (!m_pipeline) {
        return;
    }

    const GstStateChangeReturn result =
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (result == GST_STATE_CHANGE_FAILURE) {
        setError(tr("Не удалось приостановить воспроизведение"));
        return;
    }

    setPlaybackState(PausedState);
    m_positionTimer.stop();
    updatePosition();
}

void GStreamerVideoPlayer::stop()
{
    m_playRequested = false;
    m_playStartTimer.stop();
    m_playStartAttempts = 0;
    if (!m_pipeline) {
        m_stopPending = false;
        setPlaybackState(StoppedState);
        setPosition(0);
        return;
    }

    m_positionTimer.stop();
    setPlaybackState(StoppedState);

    m_stopPending = true;
    const GstStateChangeReturn result =
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (result == GST_STATE_CHANGE_FAILURE) {
        m_stopPending = false;
        setError(tr("Не удалось остановить воспроизведение"));
    } else if (result != GST_STATE_CHANGE_ASYNC) {
        finishPendingStop();
    }
}

void GStreamerVideoPlayer::seek(qint64 positionMs)
{
    if (!m_pipeline || !m_seekable) {
        return;
    }

    const qint64 upperBound = m_duration > 0 ? m_duration : positionMs;
    const qint64 boundedPosition = qBound<qint64>(0, positionMs, upperBound);
    const gboolean succeeded = gst_element_seek_simple(
                m_pipeline,
                GST_FORMAT_TIME,
                static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH
                                          | GST_SEEK_FLAG_KEY_UNIT),
                boundedPosition * GST_MSECOND);
    if (!succeeded) {
        qCWarning(lcGStreamerVideo) << "GStreamer seek failed at"
                                   << boundedPosition << "ms";
        return;
    }

    setPosition(boundedPosition);
}

void GStreamerVideoPlayer::shutdown()
{
    m_playRequested = false;
    m_stopPending = false;
    destroyPipeline();
    if (!m_source.isEmpty()) {
        m_source = QUrl();
        emit sourceChanged();
    }
    if (m_videoItem) {
        m_videoItem.clear();
        emit videoItemChanged();
    }
    resetMediaState();
}

bool GStreamerVideoPlayer::requestPlayingState()
{
    if (!m_pipeline || !m_playRequested) {
        return false;
    }

    GstState currentState = GST_STATE_NULL;
    GstState pendingState = GST_STATE_VOID_PENDING;
    gst_element_get_state(
                m_pipeline, &currentState, &pendingState, 0);

    if (currentState == GST_STATE_PLAYING) {
        m_playStartTimer.stop();
        m_playStartAttempts = 0;
        setPlaybackState(PlayingState);
        m_positionTimer.start();
        return true;
    }

    if (pendingState != GST_STATE_PLAYING) {
        const GstStateChangeReturn result =
                gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (result == GST_STATE_CHANGE_FAILURE) {
            setError(tr("Не удалось запустить воспроизведение"));
            return false;
        }
    }

    if (!m_playStartTimer.isActive()) {
        m_playStartTimer.start();
    }
    return true;
}

void GStreamerVideoPlayer::verifyPlaybackStarted()
{
    if (!m_pipeline || !m_playRequested) {
        m_playStartAttempts = 0;
        return;
    }

    GstState currentState = GST_STATE_NULL;
    GstState pendingState = GST_STATE_VOID_PENDING;
    gst_element_get_state(
                m_pipeline, &currentState, &pendingState, 0);

    if (currentState == GST_STATE_PLAYING) {
        m_playStartAttempts = 0;
        setPlaybackState(PlayingState);
        m_positionTimer.start();
        return;
    }

    ++m_playStartAttempts;
    if (m_playStartAttempts >= kMaxPlayStartAttempts) {
        setError(tr("Истекло время ожидания запуска воспроизведения"));
        return;
    }

    // После асинхронного preroll pipeline иногда остаётся в PAUSED,
    // несмотря на более ранний запрос PLAYING. Повторяем намерение пользователя.
    if (pendingState != GST_STATE_PLAYING) {
        const GstStateChangeReturn result =
                gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (result == GST_STATE_CHANGE_FAILURE) {
            setError(tr("Не удалось запустить воспроизведение"));
            return;
        }
    }

    m_playStartTimer.start();
}

void GStreamerVideoPlayer::schedulePipelinePreparation(int delayMs)
{
    if (m_prepareRetryScheduled) {
        return;
    }

    m_prepareRetryScheduled = true;
    QTimer::singleShot(delayMs, this, [this] {
        m_prepareRetryScheduled = false;
        if (!m_pipeline && !m_source.isEmpty() && m_videoItem) {
            ensurePipeline();
        }
    });
}

bool GStreamerVideoPlayer::ensurePipeline()
{
    if (m_pipeline) {
        return true;
    }
    if (m_source.isEmpty() || !m_videoItem) {
        return false;
    }

    QQuickWindow *window = m_videoItem->window();
    if (!window || !window->isSceneGraphInitialized()) {
        schedulePipelinePreparation(50);
        return false;
    }

    m_prepareRetryScheduled = false;
    setStatus(Loading);
    m_errorString.clear();
    emit errorStringChanged();

    GstElement *pipeline = gst_element_factory_make("playbin", "video-player");
    if (!pipeline) {
        setError(tr("Компонент GStreamer playbin недоступен"));
        return false;
    }
    g_signal_connect(pipeline, "deep-element-added",
                     G_CALLBACK(disableVideoDecoderQos), nullptr);

    GstElement *videoSinkBin = createVideoSinkBin();
    if (!videoSinkBin) {
        gst_object_unref(pipeline);
        return false;
    }

    g_object_set(G_OBJECT(m_qmlSink), "widget",
                 static_cast<gpointer>(m_videoItem.data()), nullptr);

    // qmlglsink должен первым получить Qt-совместимый GstGLDisplay.
    if (gst_element_set_state(m_qmlSink, GST_STATE_READY)
            == GST_STATE_CHANGE_FAILURE) {
        setError(tr("qmlglsink не удалось подключиться к QML-сцене"));
        gst_element_set_state(m_qmlSink, GST_STATE_NULL);
        gst_object_unref(videoSinkBin);
        gst_object_unref(pipeline);
        m_qmlSink = nullptr;
        return false;
    }

    const QByteArray uri = m_source.toEncoded(QUrl::FullyEncoded);
    gst_object_ref_sink(videoSinkBin);
    g_object_set(G_OBJECT(pipeline),
                 "uri", uri.constData(),
                 "video-sink", videoSinkBin,
                 "volume", static_cast<gdouble>(m_volume),
                 "mute", m_muted,
                 nullptr);

    // Иначе autoaudiosink берёт ALSA напрямую, Pulse/клики получают drain timeout.
    GstElement *audioSink = gst_element_factory_make("pulsesink", "video-audio-sink");
    if (audioSink) {
        g_object_set(G_OBJECT(pipeline), "audio-sink", audioSink, nullptr);
    }
    gst_object_unref(videoSinkBin);

    m_pipeline = pipeline;
    m_bus = gst_element_get_bus(m_pipeline);
    gst_bus_get_pollfd(m_bus, &m_busPollFd);

    if (m_busPollFd.fd >= 0) {
        m_busNotifier = new QSocketNotifier(
                    m_busPollFd.fd, QSocketNotifier::Read, this);
        connect(m_busNotifier, &QSocketNotifier::activated,
                this, [this] { drainBus(); });
    }

    const GstStateChangeReturn result =
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    if (result == GST_STATE_CHANGE_FAILURE) {
        setError(tr("Не удалось подготовить видео к воспроизведению"));
        destroyPipeline();
        return false;
    }

    qCInfo(lcGStreamerVideo) << "Preparing video with qmlglsink:"
                             << m_source;

    if (m_playRequested) {
        if (!requestPlayingState()) {
            destroyPipeline();
            return false;
        }
    }

    return true;
}

GstElement *GStreamerVideoPlayer::createVideoSinkBin()
{
    GstElement *sinkBin = gst_bin_new("qml-video-sink-bin");
    GstElement *upload = gst_element_factory_make("glupload", "gl-upload");
    GstElement *convert = gst_element_factory_make("glcolorconvert", "gl-convert");
    GstElement *capsFilter = gst_element_factory_make("capsfilter", "gl-caps");
    GstElement *sink = gst_element_factory_make("qmlglsink", "qml-video-sink");

    if (!sinkBin || !upload || !convert || !capsFilter || !sink) {
        const QString missingElement =
                !upload ? QStringLiteral("glupload")
              : !convert ? QStringLiteral("glcolorconvert")
              : !capsFilter ? QStringLiteral("capsfilter")
              : !sink ? QStringLiteral("qmlglsink")
                      : QStringLiteral("GStreamer bin");
        setError(tr("Отсутствует компонент GStreamer: %1")
                 .arg(missingElement));

        unrefElement(upload);
        unrefElement(convert);
        unrefElement(capsFilter);
        unrefElement(sink);
        unrefElement(sinkBin);
        return nullptr;
    }

    // Для локального файла сохраняем синхронизацию по clock, но не отправляем
    // QoS upstream: v4l2slh264dec иначе периодически отбрасывает кадры заранее.
    g_object_set(G_OBJECT(sink),
                 "sync", TRUE,
                 "qos", FALSE,
                 nullptr);

    GstCaps *outputCaps = gst_caps_from_string(
                "video/x-raw(memory:GLMemory),"
                "format=RGBA,texture-target=2D");
    g_object_set(G_OBJECT(capsFilter), "caps", outputCaps, nullptr);
    gst_caps_unref(outputCaps);

    gst_bin_add_many(
                GST_BIN(sinkBin), upload, convert, capsFilter, sink, nullptr);
    if (!gst_element_link_many(
                upload, convert, capsFilter, sink, nullptr)) {
        setError(tr("Не удалось связать GL-компоненты GStreamer"));
        gst_object_unref(sinkBin);
        return nullptr;
    }

    GstPad *uploadSinkPad = gst_element_get_static_pad(upload, "sink");
    GstPad *ghostPad = uploadSinkPad
            ? gst_ghost_pad_new("sink", uploadSinkPad)
            : nullptr;
    if (uploadSinkPad) {
        gst_object_unref(uploadSinkPad);
    }
    if (!ghostPad || !gst_element_add_pad(sinkBin, ghostPad)) {
        if (ghostPad) {
            gst_object_unref(ghostPad);
        }
        setError(tr("Не удалось создать вход видеосинка GStreamer"));
        gst_object_unref(sinkBin);
        return nullptr;
    }

    m_qmlSink = sink;
    return sinkBin;
}

void GStreamerVideoPlayer::destroyPipeline()
{
    m_positionTimer.stop();
    m_playStartTimer.stop();
    m_playStartAttempts = 0;

    if (m_busNotifier) {
        m_busNotifier->setEnabled(false);
        delete m_busNotifier;
        m_busNotifier = nullptr;
    }

    if (m_bus) {
        gst_bus_set_flushing(m_bus, TRUE);
    }

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }

    if (m_qmlSink) {
        g_object_set(G_OBJECT(m_qmlSink), "widget", nullptr, nullptr);
    }

    if (m_bus) {
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }
    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    m_qmlSink = nullptr;
    m_busPollFd = {};
    m_stopPending = false;
    setBuffering(false);
    setPlaybackState(StoppedState);
}

void GStreamerVideoPlayer::drainBus()
{
    if (!m_bus) {
        return;
    }

    if (m_busNotifier) {
        m_busNotifier->setEnabled(false);
    }

    while (m_bus) {
        GstMessage *message = gst_bus_pop(m_bus);
        if (!message) {
            break;
        }
        processMessage(message);
        gst_message_unref(message);
    }

    if (m_busNotifier && m_bus) {
        m_busNotifier->setEnabled(true);
    }
}

void GStreamerVideoPlayer::processMessage(GstMessage *message)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR: {
        GError *error = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        setError(gstMessageToQString(error ? error->message : nullptr),
                 gstMessageToQString(debug));
        if (error) {
            g_error_free(error);
        }
        g_free(debug);
        QMetaObject::invokeMethod(this, [this] {
            if (m_status == InvalidMedia) {
                destroyPipeline();
            }
        }, Qt::QueuedConnection);
        break;
    }
    case GST_MESSAGE_EOS:
        m_positionTimer.stop();
        updatePosition();
        setBuffering(false);
        setPlaybackState(StoppedState);
        setStatus(EndOfMedia);
        break;
    case GST_MESSAGE_ASYNC_DONE:
        finishPendingStop();
        updateDuration();
        updateSeekable();
        updatePosition();
        if (m_status != InvalidMedia && m_status != EndOfMedia) {
            setBuffering(false);
            setStatus(Loaded);
        }
        if (m_playRequested
                && m_playbackState != PlayingState) {
            requestPlayingState();
        }
        break;
    case GST_MESSAGE_DURATION_CHANGED:
        updateDuration();
        break;
    case GST_MESSAGE_BUFFERING: {
        gint percent = 0;
        gst_message_parse_buffering(message, &percent);
        const bool isBuffering = percent < 100;
        setBuffering(isBuffering);
        if (isBuffering) {
            setStatus(Buffering);
        } else if (m_status != InvalidMedia) {
            setStatus(Loaded);
        }
        break;
    }
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(m_pipeline)) {
            GstState oldState = GST_STATE_NULL;
            GstState newState = GST_STATE_NULL;
            GstState pendingState = GST_STATE_VOID_PENDING;
            gst_message_parse_state_changed(
                        message, &oldState, &newState, &pendingState);
            Q_UNUSED(oldState)
            Q_UNUSED(pendingState)

            if (newState == GST_STATE_PLAYING) {
                m_playStartTimer.stop();
                m_playStartAttempts = 0;
                setPlaybackState(PlayingState);
                m_positionTimer.start();
                if (!m_buffering) {
                    setStatus(Loaded);
                }
            }
        }
        break;
    case GST_MESSAGE_CLOCK_LOST:
        if (m_pipeline && m_playbackState == PlayingState) {
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
            gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        }
        break;
    default:
        break;
    }
}

void GStreamerVideoPlayer::updatePosition()
{
    if (!m_pipeline) {
        return;
    }

    gint64 position = GST_CLOCK_TIME_NONE;
    if (gst_element_query_position(
                m_pipeline, GST_FORMAT_TIME, &position)
            && GST_CLOCK_TIME_IS_VALID(position)) {
        setPosition(position / GST_MSECOND);
    }
}

void GStreamerVideoPlayer::updateDuration()
{
    if (!m_pipeline) {
        return;
    }

    gint64 duration = GST_CLOCK_TIME_NONE;
    if (gst_element_query_duration(
                m_pipeline, GST_FORMAT_TIME, &duration)
            && GST_CLOCK_TIME_IS_VALID(duration)) {
        setDuration(duration / GST_MSECOND);
    }
}

void GStreamerVideoPlayer::updateSeekable()
{
    if (!m_pipeline) {
        setSeekable(false);
        return;
    }

    GstQuery *query = gst_query_new_seeking(GST_FORMAT_TIME);
    gboolean seekable = FALSE;
    if (gst_element_query(m_pipeline, query)) {
        GstFormat format = GST_FORMAT_UNDEFINED;
        gint64 start = 0;
        gint64 end = 0;
        gst_query_parse_seeking(query, &format, &seekable, &start, &end);
        Q_UNUSED(format)
        Q_UNUSED(start)
        Q_UNUSED(end)
    }
    gst_query_unref(query);
    setSeekable(seekable);
}

void GStreamerVideoPlayer::finishPendingStop()
{
    if (!m_stopPending || !m_pipeline) {
        return;
    }

    m_stopPending = false;
    const gboolean succeeded = gst_element_seek_simple(
                m_pipeline,
                GST_FORMAT_TIME,
                static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH
                                          | GST_SEEK_FLAG_KEY_UNIT),
                0);
    if (succeeded) {
        setPosition(0);
    } else {
        qCWarning(lcGStreamerVideo)
                << "GStreamer failed to seek to the start while stopping";
        updatePosition();
    }

    if (m_status != InvalidMedia) {
        setStatus(Loaded);
    }
}

void GStreamerVideoPlayer::setPlaybackState(PlaybackState state)
{
    if (m_playbackState == state) {
        return;
    }
    m_playbackState = state;
    emit playbackStateChanged();
}

void GStreamerVideoPlayer::setStatus(MediaStatus status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void GStreamerVideoPlayer::setPosition(qint64 position)
{
    if (m_position == position) {
        return;
    }
    m_position = position;
    emit positionChanged();
}

void GStreamerVideoPlayer::setDuration(qint64 duration)
{
    if (m_duration == duration) {
        return;
    }
    m_duration = duration;
    emit durationChanged();
}

void GStreamerVideoPlayer::setSeekable(bool seekable)
{
    if (m_seekable == seekable) {
        return;
    }
    m_seekable = seekable;
    emit seekableChanged();
}

void GStreamerVideoPlayer::setBuffering(bool buffering)
{
    if (m_buffering == buffering) {
        return;
    }
    m_buffering = buffering;
    emit bufferingChanged();
}

void GStreamerVideoPlayer::setError(const QString &message,
                                    const QString &debugDetails)
{
    const QString effectiveMessage = message.isEmpty()
            ? tr("Неизвестная ошибка GStreamer")
            : message;
    m_errorString = effectiveMessage;
    m_playRequested = false;
    m_stopPending = false;
    m_positionTimer.stop();
    m_playStartTimer.stop();
    m_playStartAttempts = 0;
    setBuffering(false);
    setPlaybackState(StoppedState);
    setStatus(InvalidMedia);
    emit errorStringChanged();
    emit errorOccurred(effectiveMessage);

    qCWarning(lcGStreamerVideo).noquote()
            << "GStreamer video error:" << effectiveMessage;
    if (!debugDetails.isEmpty()) {
        qCWarning(lcGStreamerVideo).noquote()
                << "GStreamer debug:" << debugDetails;
    }
}

void GStreamerVideoPlayer::resetMediaState()
{
    setPlaybackState(StoppedState);
    setStatus(m_source.isEmpty() ? NoMedia : Loading);
    setPosition(0);
    setDuration(0);
    setSeekable(false);
    setBuffering(false);

    if (!m_errorString.isEmpty()) {
        m_errorString.clear();
        emit errorStringChanged();
    }
}
