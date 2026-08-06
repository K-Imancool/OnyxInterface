#include "uiclicksound.h"
#include "apppaths.h"

#include <QEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTouchEvent>
#include <QUrl>
#include <QWindow>

namespace {

void waitUntilReady(QSoundEffect *effect)
{
    if (!effect || effect->status() == QSoundEffect::Ready) {
        return;
    }

    QEventLoop loop;
    QObject::connect(effect, &QSoundEffect::statusChanged, &loop, [effect, &loop]() {
        if (effect->status() == QSoundEffect::Ready
                || effect->status() == QSoundEffect::Error) {
            loop.quit();
        }
    });
    loop.exec();
}

QQuickItem *topmostItemAt(QQuickItem *root, const QPointF &pos)
{
    if (!root || !root->isVisible() || !root->isEnabled()) {
        return nullptr;
    }

    const QPointF local = root->mapFromScene(pos);
    if (!root->contains(local)) {
        return nullptr;
    }

    const QList<QQuickItem *> children = root->childItems();
    for (int i = children.size() - 1; i >= 0; --i) {
        if (QQuickItem *hit = topmostItemAt(children.at(i), pos)) {
            return hit;
        }
    }
    return root;
}

} // namespace

UiClickSound::UiClickSound(QObject *parent)
    : QObject(parent)
{
    loadSource();
    m_lastPlay.invalidate();
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
    if (m_effect) {
        m_effect->setVolume(m_volume);
    }
    emit volumeChanged();
}

void UiClickSound::play(bool force)
{
    playClick(force);
}

void UiClickSound::stop()
{
    if (m_effect && m_effect->isPlaying()) {
        m_effect->stop();
    }
}

bool UiClickSound::isOnSettingsVolumeSlider(const QPointF &globalPos) const
{
    const QList<QWindow *> windows = QGuiApplication::topLevelWindows();
    for (QWindow *window : windows) {
        auto *quickWindow = qobject_cast<QQuickWindow *>(window);
        if (!quickWindow || !quickWindow->contentItem()) {
            continue;
        }

        const QPointF scenePos = quickWindow->contentItem()->mapFromGlobal(globalPos);
        QQuickItem *hit = topmostItemAt(quickWindow->contentItem(), scenePos);
        while (hit) {
            if (hit->objectName() == QLatin1String(kVolumeSliderObjectName)) {
                return true;
            }
            hit = hit->parentItem();
        }
    }
    return false;
}

bool UiClickSound::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    if (!m_enabled) {
        return false;
    }

    switch (event->type()) {
    case QEvent::TouchBegin: {
        const auto *touchEvent = static_cast<const QTouchEvent *>(event);
        if (touchEvent->touchPoints().isEmpty()) {
            break;
        }
        const QPointF globalPos = touchEvent->touchPoints().first().screenPos();
        // Слайдер громкости сам вызовет play() после applyVolumeLevel.
        if (isOnSettingsVolumeSlider(globalPos)) {
            break;
        }
        playClick();
        break;
    }
    case QEvent::MouseButtonPress: {
        const auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        // Touch уже обработан через TouchBegin.
        if (mouseEvent->source() != Qt::MouseEventNotSynthesized) {
            break;
        }
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }
        if (isOnSettingsVolumeSlider(mouseEvent->globalPos())) {
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

    m_effect = new QSoundEffect(this);
    m_effect->setLoopCount(1);
    m_effect->setVolume(m_volume);
    m_effect->setSource(QUrl::fromLocalFile(path));
    waitUntilReady(m_effect);

    if (m_effect->status() == QSoundEffect::Error) {
        qWarning("UiClickSound: failed to load %s", qPrintable(path));
        m_effect->deleteLater();
        m_effect = nullptr;
    }
}

void UiClickSound::playClick(bool force)
{
    if ((!force && !m_enabled) || !m_effect || m_effect->status() != QSoundEffect::Ready) {
        return;
    }
    if (m_lastPlay.isValid() && m_lastPlay.elapsed() < kMinIntervalMs) {
        return;
    }
    if (m_effect->isPlaying()) {
        return;
    }

    m_lastPlay.restart();
    m_effect->play();
}
