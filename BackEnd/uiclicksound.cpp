#include "uiclicksound.h"
#include "apppaths.h"

#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>
#include <QTouchEvent>
#include <QtEndian>
#include <cstring>
#include <utility>

namespace {

bool loadWavPcm(const QString &path, QAudioFormat *format, QByteArray *pcm)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() < 44
            || bytes.mid(0, 4) != "RIFF"
            || bytes.mid(8, 4) != "WAVE") {
        return false;
    }

    quint16 audioFormat = 0;
    quint16 channels = 0;
    quint16 bits = 0;
    quint32 sampleRate = 0;
    QByteArray data;
    int pos = 12;
    while (pos + 8 <= bytes.size()) {
        const QByteArray chunkId = bytes.mid(pos, 4);
        const quint32 chunkSize = qFromLittleEndian<quint32>(
                    reinterpret_cast<const uchar *>(bytes.constData() + pos + 4));
        pos += 8;
        if (pos + int(chunkSize) > bytes.size()) {
            break;
        }
        if (chunkId == "fmt " && chunkSize >= 16) {
            const uchar *p = reinterpret_cast<const uchar *>(bytes.constData() + pos);
            audioFormat = qFromLittleEndian<quint16>(p);
            channels = qFromLittleEndian<quint16>(p + 2);
            sampleRate = qFromLittleEndian<quint32>(p + 4);
            bits = qFromLittleEndian<quint16>(p + 14);
        } else if (chunkId == "data") {
            data = bytes.mid(pos, int(chunkSize));
            break;
        }
        pos += int((chunkSize + 1u) & ~1u);
    }

    if (audioFormat != 1 || channels < 1 || sampleRate < 1 || bits < 8 || data.isEmpty()) {
        return false;
    }

    format->setCodec(QStringLiteral("audio/pcm"));
    format->setByteOrder(QAudioFormat::LittleEndian);
    format->setSampleRate(int(sampleRate));
    format->setChannelCount(int(channels));
    format->setSampleSize(int(bits));
    format->setSampleType(bits == 8 ? QAudioFormat::UnSignedInt : QAudioFormat::SignedInt);
    *pcm = data;
    return true;
}

QByteArray toStereoIfNeeded(const QByteArray &pcm, QAudioFormat *format)
{
    if (format->channelCount() != 1) {
        return pcm;
    }
    const int bytesPerSample = qMax(1, format->sampleSize() / 8);
    QByteArray stereo;
    stereo.resize(pcm.size() * 2);
    for (int i = 0; i + bytesPerSample <= pcm.size(); i += bytesPerSample) {
        memcpy(stereo.data() + i * 2, pcm.constData() + i, size_t(bytesPerSample));
        memcpy(stereo.data() + i * 2 + bytesPerSample, pcm.constData() + i, size_t(bytesPerSample));
    }
    format->setChannelCount(2);
    return stereo;
}

QAudioDeviceInfo pulsePreferredDevice()
{
    const auto devices = QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);
    for (const auto &device : devices) {
        const QString name = device.deviceName();
        if (name.contains(QLatin1String("pulse"), Qt::CaseInsensitive)
                || name.contains(QLatin1String("alsa_output."), Qt::CaseInsensitive)) {
            return device;
        }
    }
    return QAudioDeviceInfo::defaultOutputDevice();
}

} // namespace

class ClickAudioEngine : public QObject
{
public:
    ClickAudioEngine(QAudioDeviceInfo device, QAudioFormat format, QByteArray pcm, QObject *parent = nullptr)
        : QObject(parent)
        , m_device(device)
        , m_format(format)
        , m_pcm(std::move(pcm))
    {
        const int chunkBytes = qMax(256, m_format.bytesForDuration(20000));
        m_silence.fill(char(m_format.sampleType() == QAudioFormat::UnSignedInt ? 0x80 : 0),
                       chunkBytes);
    }

    void startEngine()
    {
        m_timer = new QTimer(this);
        m_timer->setInterval(10);
        connect(m_timer, &QTimer::timeout, this, &ClickAudioEngine::feed);
        ensureOutput();
        m_timer->start();
    }

    void setVolume(qreal volume)
    {
        m_volume = volume;
        if (m_audio) {
            m_audio->setVolume(m_volume);
        }
    }

    void triggerClick()
    {
        QMutexLocker lock(&m_mutex);
        m_offset = 0;
    }

    void stopClick()
    {
        QMutexLocker lock(&m_mutex);
        m_offset = -1;
    }

    void shutdown()
    {
        if (m_timer) {
            m_timer->stop();
        }
        if (m_audio) {
            m_audio->stop();
            m_io = nullptr;
        }
    }

private:
    bool ensureOutput()
    {
        if (!m_audio) {
            m_audio = new QAudioOutput(m_device, m_format, this);
            m_audio->setCategory(QStringLiteral("game"));
            m_audio->setVolume(m_volume);
            m_audio->setBufferSize(qMax(8192, m_format.bytesForDuration(180000)));
            connect(m_audio, &QAudioOutput::stateChanged, this,
                    [this](QAudio::State state) {
                if (state == QAudio::IdleState) {
                    feed();
                }
            });
        }

        const QAudio::State state = m_audio->state();
        if ((state == QAudio::ActiveState || state == QAudio::IdleState) && m_io) {
            return true;
        }

        m_io = m_audio->start();
        return m_io != nullptr;
    }

    void feed()
    {
        if (!ensureOutput() || !m_io) {
            return;
        }

        int freeBytes = m_audio->bytesFree();
        while (freeBytes >= m_silence.size() && m_silence.size() > 0) {
            QByteArray chunk;
            {
                QMutexLocker lock(&m_mutex);
                if (m_offset >= 0 && m_offset < m_pcm.size()) {
                    const int remain = m_pcm.size() - m_offset;
                    const int take = qMin(remain, m_silence.size());
                    chunk = m_pcm.mid(m_offset, take);
                    m_offset += take;
                    if (m_offset >= m_pcm.size()) {
                        m_offset = -1;
                    }
                }
            }
            if (chunk.isEmpty()) {
                chunk = m_silence;
            }
            const qint64 written = m_io->write(chunk);
            if (written <= 0) {
                break;
            }
            freeBytes -= int(written);
        }
    }

    QMutex m_mutex;
    QAudioDeviceInfo m_device;
    QAudioFormat m_format;
    QByteArray m_pcm;
    QByteArray m_silence;
    QAudioOutput *m_audio = nullptr;
    QIODevice *m_io = nullptr;
    QTimer *m_timer = nullptr;
    int m_offset = -1;
    qreal m_volume = 1.0;
};

UiClickSound::UiClickSound(QObject *parent)
    : QObject(parent)
{
    m_lastPlay.invalidate();
    loadSource();
    if (qApp) {
        qApp->installEventFilter(this);
    }
}

UiClickSound::~UiClickSound()
{
    if (m_engine && m_audioThread) {
        QMetaObject::invokeMethod(m_engine, [engine = m_engine]() {
            engine->shutdown();
        }, Qt::BlockingQueuedConnection);
        m_audioThread->quit();
        m_audioThread->wait(1000);
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

qreal UiClickSound::volume() const
{
    return m_volume;
}

void UiClickSound::setVolume(qreal volume)
{
    const qreal bounded = qBound<qreal>(0.0, volume, 1.0);
    if (qFuzzyCompare(m_volume, bounded)) {
        return;
    }
    m_volume = bounded;
    if (m_engine) {
        QMetaObject::invokeMethod(m_engine, [this, bounded]() {
            m_engine->setVolume(bounded);
        }, Qt::QueuedConnection);
    }
    emit volumeChanged();
}

void UiClickSound::play(bool force)
{
    playClick(force);
}

void UiClickSound::stop()
{
    if (m_engine) {
        m_engine->stopClick();
    }
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
        playClick();
        break;
    }
    case QEvent::MouseButtonPress: {
        // На тачскрине mouse приходит отдельно и даёт второй клик + underrun.
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
        playClick();
        break;
    }
    default:
        break;
    }

    return false;
}

void UiClickSound::loadSource()
{
    const QString path = AppPaths::instance().soundsDir()
            + QStringLiteral("/button0.wav");
    qInfo("UiClickSound: click sound %s", qPrintable(path));

    if (!QFileInfo::exists(path)) {
        qWarning("UiClickSound: file not found: %s", qPrintable(path));
        return;
    }

    QAudioFormat format;
    QByteArray pcm;
    if (!loadWavPcm(path, &format, &pcm)) {
        qWarning("UiClickSound: unsupported wav %s", qPrintable(path));
        return;
    }

    const QAudioDeviceInfo device = pulsePreferredDevice();
    qInfo("UiClickSound: output %s", qPrintable(device.deviceName()));
    if (!device.isFormatSupported(format)) {
        pcm = toStereoIfNeeded(pcm, &format);
        if (!device.isFormatSupported(format)) {
            format = device.nearestFormat(format);
            qWarning("UiClickSound: using nearest format %d Hz, %d ch, %d bit",
                     format.sampleRate(), format.channelCount(), format.sampleSize());
        }
    }

    m_engine = new ClickAudioEngine(device, format, pcm);
    m_engine->setVolume(m_volume);
    m_audioThread = new QThread(this);
    m_engine->moveToThread(m_audioThread);
    connect(m_audioThread, &QThread::started, m_engine, &ClickAudioEngine::startEngine);
    connect(m_audioThread, &QThread::finished, m_engine, &QObject::deleteLater);
    m_audioThread->start();
}

void UiClickSound::playClick(bool force)
{
    if ((!force && !m_enabled) || !m_engine) {
        return;
    }
    if (m_lastPlay.isValid() && m_lastPlay.elapsed() < kMinIntervalMs) {
        return;
    }

    m_lastPlay.restart();
    m_engine->triggerClick();
}
