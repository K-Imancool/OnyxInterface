#include "gpiomonitor.h"

#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>
#include <QThread>
#include <QTimer>

#include <cctype>
#include <cerrno>
#include <cstring>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <linux/gpio.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {

// Аппаратный debounce в ядре при запросе линии (gpio v2 LINE_ATTR_ID_DEBOUNCE).
// 2 мс отсекает дребезг контакта, на выдержку питания 100 мс не влияет.
constexpr unsigned kDebounceUs = 2000;

#ifdef Q_OS_UNIX
// Сопоставить label gpiochip с нашим банком: точное "gpio0", адрес "fdd60000.gpio",
// но не "gpio01" при поиске "gpio0".
bool labelMatches(const QByteArray &label,
                  const QByteArray &chipLabel,
                  const QByteArray &mmioPrefix)
{
    if (label == chipLabel) {
        return true;
    }
    if (!mmioPrefix.isEmpty() && label.contains(mmioPrefix)) {
        return true;
    }
    if (chipLabel.isEmpty()) {
        return false;
    }
    const int idx = label.indexOf(chipLabel);
    if (idx < 0) {
        return false;
    }
    const int after = idx + chipLabel.size();
    return after >= label.size()
            || !std::isdigit(static_cast<unsigned char>(label.at(after)));
}

QByteArray readPath(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return file.readAll().trimmed();
}

bool writePath(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        return false;
    }
    return file.write(data) == data.size();
}

int openNamedGpioChip(const QByteArray &chipLabel,
                      const QByteArray &mmioPrefix,
                      const QString &pinName)
{
    // Не QDir("/dev"): QDir::System часто не видит символьные узлы gpiochip*,
    // а неудачный open без учёта давал «доступны: нет». Перебор gpiochip0..15.
    QStringList seen;
    bool anyNode = false;

    for (int i = 0; i < 16; ++i) {
        const QByteArray path = QByteArrayLiteral("/dev/gpiochip")
                + QByteArray::number(i);
        if (::access(path.constData(), F_OK) != 0) {
            continue;
        }
        anyNode = true;

        const int fd = ::open(path.constData(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            seen << QStringLiteral("%1: %2")
                    .arg(QString::fromLatin1(path),
                         QString::fromLocal8Bit(std::strerror(errno)));
            continue;
        }

        gpiochip_info info{};
        if (::ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) < 0) {
            seen << QStringLiteral("%1: ioctl %2")
                    .arg(QString::fromLatin1(path),
                         QString::fromLocal8Bit(std::strerror(errno)));
            ::close(fd);
            continue;
        }

        seen << QStringLiteral("%1 (%2, %3 линий)")
                .arg(QString::fromLatin1(path),
                     QString::fromLatin1(info.label))
                .arg(info.lines);

        if (labelMatches(QByteArray(info.label), chipLabel, mmioPrefix)) {
//            qInfo("%s: найден контроллер %s (%s), линий %u",
//                  qPrintable(pinName), path.constData(), info.label, info.lines);
            return fd;
        }
        ::close(fd);
    }

    if (!anyNode) {
        qWarning("%s: в /dev нет gpiochip* — нет GPIO character device "
                 "(или нет прав на /dev). Пробую sysfs.",
                 qPrintable(pinName));
    } else {
        qWarning("%s: контроллер %s/%s не найден среди: %s",
                 qPrintable(pinName),
                 chipLabel.constData(),
                 mmioPrefix.constData(),
                 qPrintable(seen.join(QStringLiteral("; "))));
    }
    return -1;
}

bool lineUsedByKernel(int chipFd, unsigned offset, QString *consumer)
{
    // Штатный DTB ROC-RK3566 держит GPIO0_D5 как enable vcc3v3_vga — тогда
    // userspace получит EBUSY. Пин нужно отпустить в device tree.
    gpio_v2_line_info info{};
    info.offset = offset;
    if (::ioctl(chipFd, GPIO_V2_GET_LINEINFO_IOCTL, &info) < 0) {
        return false;
    }
    if (consumer) {
        *consumer = QString::fromLatin1(info.consumer);
    }
    return (info.flags & GPIO_V2_LINE_FLAG_USED) != 0;
}

bool findSysfsChip(const QByteArray &chipLabel,
                   const QByteArray &mmioPrefix,
                   int *base,
                   QByteArray *foundLabel)
{
    // Запасной путь, если нет /dev/gpiochip*. На современных ядрах base gpio0
    // может быть 0 или 512 — linux gpio = base + offset, не bank*32 вслепую.
    const QStringList roots = {
        QStringLiteral("/sys/class/gpio"),
        QStringLiteral("/sys/bus/gpio/devices")
    };

    for (const QString &root : roots) {
        const QDir dir(root);
        if (!dir.exists()) {
            continue;
        }
        const QStringList names = dir.entryList(
                    QStringList() << QStringLiteral("gpiochip*"),
                    QDir::Dirs | QDir::NoDotAndDotDot,
                    QDir::Name);
        for (const QString &name : names) {
            const QString chipDir = dir.absoluteFilePath(name);
            const QByteArray label = readPath(chipDir + QStringLiteral("/label"));
            const QByteArray baseText = readPath(chipDir + QStringLiteral("/base"));
            bool ok = false;
            const int chipBase = baseText.toInt(&ok);
            if (!ok) {
                continue;
            }
            if (!labelMatches(label, chipLabel, mmioPrefix)
                    && !labelMatches(name.toLatin1(), chipLabel, mmioPrefix)) {
                continue;
            }
            *base = chipBase;
            *foundLabel = label.isEmpty() ? name.toLatin1() : label;
            return true;
        }
    }
    return false;
}
#endif

QByteArray makeConsumerLabel(const QString &pinName)
{
    // Имя потребителя в gpioinfo, напр. qtpr-gpio0-d5 (лимит GPIO_MAX_NAME_SIZE = 32).
    return QByteArrayLiteral("qtpr-")
            + pinName.toLatin1().toLower().replace('_', '-');
}

} // namespace

GpioMonitor::GpioMonitor(const QString &pinName,
                         const QByteArray &chipLabel,
                         const QByteArray &mmioPrefix,
                         unsigned lineOffset,
                         QObject *parent)
    : QObject(parent)
    , m_pinName(pinName)
    , m_chipLabel(chipLabel)
    , m_mmioPrefix(mmioPrefix)
    , m_lineOffset(lineOffset)
    , m_consumer(makeConsumerLabel(pinName))
{
}

GpioMonitor::~GpioMonitor()
{
    closeLine();
}

void GpioMonitor::start()
{
#ifdef Q_OS_UNIX
    if (m_lineFd >= 0) {
        return;
    }
    if (!openChip() || !requestLine()) {
        closeLine();
        setAvailable(false);
        return;
    }

    // Старт слежения — после первого чтения: поток выдержки иначе гоняет fd
    // параллельно с ioctl начального уровня.
    bool value = false;
    if (readValue(&value)) {
        applyValue(value, "начальное состояние");
    } else {
        qWarning("%s: линия захвачена, но прочитать уровень не удалось: %s",
                 qPrintable(m_pinName), std::strerror(errno));
    }

    setAvailable(true);
    startWatching();
#else
    qWarning("%s: контроль GPIO доступен только на Linux", qPrintable(m_pinName));
    setAvailable(false);
#endif
}

bool GpioMonitor::setHigh(bool high)
{
#ifdef Q_OS_UNIX
    // Для GPIO0_D5 (монитор питания) не вызывать: линия должна остаться входом.
    if (m_lineFd < 0 && m_sysfsGpioDir.isEmpty()) {
        qWarning("%s: линия не открыта, запись невозможна", qPrintable(m_pinName));
        return false;
    }

    if (m_usingSysfs) {
        if (!writePath(m_sysfsGpioDir + QStringLiteral("/direction"),
                       QByteArrayLiteral("out\n"))) {
            qWarning("%s: не удалось перевести в выход: %s",
                     qPrintable(m_pinName), std::strerror(errno));
            return false;
        }
        if (!writePath(m_sysfsGpioDir + QStringLiteral("/value"),
                       high ? QByteArrayLiteral("1\n") : QByteArrayLiteral("0\n"))) {
            qWarning("%s: не удалось записать уровень: %s",
                     qPrintable(m_pinName), std::strerror(errno));
            return false;
        }
        m_outputMode = true;
        if (m_notifier) {
            m_notifier->setEnabled(false);
        }
        applyValue(high, "запись");
        return true;
    }

    if (!m_outputMode) {
        gpio_v2_line_config cfg{};
        cfg.flags = GPIO_V2_LINE_FLAG_OUTPUT;
        cfg.num_attrs = 1;
        cfg.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
        cfg.attrs[0].attr.values = high ? 1 : 0;
        cfg.attrs[0].mask = 1;
        if (::ioctl(m_lineFd, GPIO_V2_LINE_SET_CONFIG_IOCTL, &cfg) < 0) {
            qWarning("%s: не удалось перевести в выход: %s",
                     qPrintable(m_pinName), std::strerror(errno));
            return false;
        }
        m_outputMode = true;
        if (m_notifier) {
            m_notifier->setEnabled(false);
        }
        qInfo("%s: линия переведена в выход", qPrintable(m_pinName));
    } else {
        gpio_v2_line_values values{};
        values.mask = 1;
        values.bits = high ? 1 : 0;
        if (::ioctl(m_lineFd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
            qWarning("%s: не удалось записать уровень: %s",
                     qPrintable(m_pinName), std::strerror(errno));
            return false;
        }
    }

    applyValue(high, "запись");
    return true;
#else
    Q_UNUSED(high);
    return false;
#endif
}

bool GpioMonitor::openChip()
{
#ifdef Q_OS_UNIX
    m_chipFd = openNamedGpioChip(m_chipLabel, m_mmioPrefix, m_pinName);
    if (m_chipFd < 0) {
        // Чипы на месте, процесс не root — sysfs export тоже EACCES.
        // udev: deploy/gpio/99-qtpr-gpio.rules (UI — user-сервис kikorik).
        if (::access("/dev/gpiochip0", F_OK) == 0
                && ::access("/dev/gpiochip0", R_OK) != 0) {
            qWarning("%s: нет прав на /dev/gpiochip*. "
                     "На плате: sudo deploy/gpio/install-qtpr-gpio.sh "
                     "и перезапуск UI.",
                     qPrintable(m_pinName));
            return false;
        }
    }
    if (m_chipFd >= 0) {
        QString consumer;
        if (lineUsedByKernel(m_chipFd, m_lineOffset, &consumer)) {
            QByteArray hint;
            if (m_pinName == QLatin1String("GPIO0_D5")) {
                hint = " В штатном DTB ROC-RK3566 это enable регулятора vcc3v3_vga.";
            }
            qWarning("%s: линия занята ядром (consumer: %s). "
                     "Пин нужно освободить в device tree.%s",
                     qPrintable(m_pinName),
                     consumer.isEmpty() ? "unknown" : qPrintable(consumer),
                     hint.constData());
            return false;
        }
        m_usingSysfs = false;
        return true;
    }
    return openSysfsChip();
#else
    return false;
#endif
}

bool GpioMonitor::openSysfsChip()
{
#ifdef Q_OS_UNIX
    int base = -1;
    QByteArray foundLabel;
    if (!findSysfsChip(m_chipLabel, m_mmioPrefix, &base, &foundLabel)) {
        qWarning("%s: sysfs gpiochip %s тоже не найден "
                 "(проверьте ls /dev/gpiochip* и ls /sys/class/gpio)",
                 qPrintable(m_pinName), m_chipLabel.constData());
        return false;
    }

    m_usingSysfs = true;
    // Не bank*32: на 6.x base gpio0 бывает 512. linux gpio = base + offset.
    m_linuxGpio = base + static_cast<int>(m_lineOffset);
    m_sysfsGpioDir = QStringLiteral("/sys/class/gpio/gpio%1").arg(m_linuxGpio);
    qInfo("%s: использую sysfs %s (label %s, base %d, linux gpio %d)",
          qPrintable(m_pinName),
          qPrintable(m_sysfsGpioDir),
          foundLabel.constData(),
          base,
          m_linuxGpio);
    return true;
#else
    return false;
#endif
}

bool GpioMonitor::requestLine()
{
#ifdef Q_OS_UNIX
    if (m_usingSysfs) {
        return requestSysfsLine();
    }
    return requestCharDevLine();
#else
    return false;
#endif
}

bool GpioMonitor::requestCharDevLine()
{
#ifdef Q_OS_UNIX
    // Вход + оба фронта. Debounce 2 мс — если ядро не поддерживает атрибут,
    // повторный ioctl без него. O_NONBLOCK: иначе read событий блокирует поток.
    gpio_v2_line_request req{};
    req.offsets[0] = m_lineOffset;
    req.num_lines = 1;
    std::strncpy(req.consumer, m_consumer.constData(), sizeof(req.consumer) - 1);
    req.config.flags = GPIO_V2_LINE_FLAG_INPUT
            | GPIO_V2_LINE_FLAG_EDGE_RISING
            | GPIO_V2_LINE_FLAG_EDGE_FALLING;
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
    req.config.attrs[0].attr.debounce_period_us = kDebounceUs;
    req.config.attrs[0].mask = 1;

    if (::ioctl(m_chipFd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        req.config.num_attrs = 0;
        std::memset(req.config.attrs, 0, sizeof(req.config.attrs));
        if (::ioctl(m_chipFd, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
            qWarning("%s: не удалось захватить линию %u: %s",
                     qPrintable(m_pinName), m_lineOffset, std::strerror(errno));
            return false;
        }
        qWarning("%s: захват без аппаратного debounce", qPrintable(m_pinName));
    }

    m_lineFd = req.fd;
    const int flags = ::fcntl(m_lineFd, F_GETFL);
    if (flags >= 0) {
        ::fcntl(m_lineFd, F_SETFL, flags | O_NONBLOCK);
    }

//    qInfo("%s: контроль линии %s offset %u запущен",
//          qPrintable(m_pinName), m_chipLabel.constData(), m_lineOffset);
    return true;
#else
    return false;
#endif
}

bool GpioMonitor::requestSysfsLine()
{
#ifdef Q_OS_UNIX
    // Запас, если нет gpio cdev. UI без root обычно не может export — см. udev.
    if (!QFileInfo::exists(m_sysfsGpioDir)) {
        if (!writePath(QStringLiteral("/sys/class/gpio/export"),
                       QByteArray::number(m_linuxGpio) + '\n')) {
            qWarning("%s: export %d не удался: %s",
                     qPrintable(m_pinName), m_linuxGpio, std::strerror(errno));
            return false;
        }
        m_sysfsExported = true;
    }

    if (!    writePath(m_sysfsGpioDir + QStringLiteral("/direction"),
                   QByteArrayLiteral("in\n"))) {
        qWarning("%s: не удалось выставить direction=in: %s",
                 qPrintable(m_pinName), std::strerror(errno));
        return false;
    }
    // both: sysfs сообщает POLLPRI на фронт; без edge notifier молчит.
    writePath(m_sysfsGpioDir + QStringLiteral("/edge"),
              QByteArrayLiteral("both\n"));

    const QByteArray valuePath =
            (m_sysfsGpioDir + QStringLiteral("/value")).toLocal8Bit();
    m_lineFd = ::open(valuePath.constData(), O_RDONLY | O_CLOEXEC);
    if (m_lineFd < 0) {
        qWarning("%s: не удалось открыть %s: %s",
                 qPrintable(m_pinName), valuePath.constData(), std::strerror(errno));
        return false;
    }

    qInfo("%s: контроль через sysfs запущен", qPrintable(m_pinName));
    return true;
#else
    return false;
#endif
}

void GpioMonitor::startWatching()
{
#ifdef Q_OS_UNIX
    // Монитор питания: только поток выдержки (он же читает события).
    // Иначе QSocketNotifier + poll GET_VALUES на том же fd мешают друг другу.
    if (m_lowHoldMs > 0) {
        startHoldThread();
        return;
    }

    const QSocketNotifier::Type type = m_usingSysfs
            ? QSocketNotifier::Exception  // sysfs gpio: ядро сигналит POLLPRI
            : QSocketNotifier::Read;
    m_notifier = new QSocketNotifier(m_lineFd, type, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, [this] { onLineEvent(); });

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(50);
    connect(m_pollTimer, &QTimer::timeout, this, &GpioMonitor::pollValue);
    m_pollTimer->start();
#endif
}

bool GpioMonitor::readValue(bool *high) const
{
#ifdef Q_OS_UNIX
    if (!high) {
        return false;
    }
    if (m_usingSysfs) {
        return readSysfsValue(high);
    }
    if (m_lineFd < 0) {
        return false;
    }
    gpio_v2_line_values values{};
    values.mask = 1;
    if (::ioctl(m_lineFd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
        return false;
    }
    *high = (values.bits & 1u) != 0;
    return true;
#else
    Q_UNUSED(high);
    return false;
#endif
}

bool GpioMonitor::readSysfsValue(bool *high) const
{
#ifdef Q_OS_UNIX
    // sysfs value после POLLPRI нужно перемотать на начало, иначе read даст EOF.
    if (m_lineFd >= 0) {
        if (::lseek(m_lineFd, 0, SEEK_SET) < 0) {
            return false;
        }
        char buf[8] = {};
        const ssize_t n = ::read(m_lineFd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            return false;
        }
        *high = (buf[0] == '1');
        return true;
    }
    const QByteArray text = readPath(m_sysfsGpioDir + QStringLiteral("/value"));
    if (text.isEmpty()) {
        return false;
    }
    *high = text.startsWith('1');
    return true;
#else
    Q_UNUSED(high);
    return false;
#endif
}

void GpioMonitor::applyValue(bool high, const char *reason)
{
    // reason попадает в stderr (messageHandler): «начальное состояние», «фронт ↓» и т.д.
    if (m_available && high == m_high) {
        return;
    }
    m_high = high;
//    qInfo("%s: %s (%s)", qPrintable(m_pinName), high ? "HIGH" : "LOW", reason);
    emit highChanged();
}

void GpioMonitor::setLowHoldMs(int milliseconds)
{
    // Вызывать до start(). Если линия уже открыта без выдержки — останавливаем
    // GUI-опрос и переходим на поток.
    m_lowHoldMs = qMax(0, milliseconds);
    m_lowHeldEmitted.store(false);
    if (m_lineFd >= 0 && m_lowHoldMs > 0) {
        if (m_pollTimer) {
            m_pollTimer->stop();
        }
        if (m_notifier) {
            m_notifier->setEnabled(false);
        }
        startHoldThread();
    }
}

bool GpioMonitor::collectLevel(bool *high)
{
#ifdef Q_OS_UNIX
    if (!high || m_lineFd < 0) {
        return false;
    }

    QMutexLocker locker(&m_lineMutex);
    bool gotEvent = false;
    bool eventHigh = false;

    if (!m_usingSysfs) {
        gpio_v2_line_event event{};
        while (true) {
            const ssize_t n = ::read(m_lineFd, &event, sizeof(event));
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                return false;
            }
            if (n != static_cast<ssize_t>(sizeof(event))) {
                break;
            }
            gotEvent = true;
            // Пачка событий: берём последнее. RISING = физический HIGH (не ACTIVE_LOW).
            eventHigh = (event.id == GPIO_V2_LINE_EVENT_RISING_EDGE);
        }
    }

    // Фронт — источник истины. GET_VALUES на event-fd на RK3566 иногда остаётся HIGH
    // после падения линии и сбрасывал 100 мс выдержку питания каждые ~10–50 мс.
    if (gotEvent) {
        m_haveEdgeLevel = true;
        m_edgeHigh = eventHigh;
        *high = eventHigh;
        return true;
    }
    if (m_haveEdgeLevel) {
        *high = m_edgeHigh;
        return true;
    }

    gpio_v2_line_values values{};
    values.mask = 1;
    if (!m_usingSysfs
            && ::ioctl(m_lineFd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) == 0) {
        *high = (values.bits & 1u) != 0;
        return true;
    }
    if (m_usingSysfs && readSysfsValue(high)) {
        return true;
    }
    return false;
#else
    Q_UNUSED(high);
    return false;
#endif
}

void GpioMonitor::startHoldThread()
{
    if (m_holdThread || m_lowHoldMs <= 0 || m_lineFd < 0) {
        return;
    }
    m_holdStop.store(false);
    m_lowHeldEmitted.store(false);
    m_holdThread = QThread::create([this]() { lowHoldLoop(); });
    m_holdThread->setObjectName(QStringLiteral("gpio-hold-%1").arg(m_pinName));
    m_holdThread->start();
//    qInfo("%s: поток выдержки LOW %d мс запущен",
//          qPrintable(m_pinName), m_lowHoldMs);
}

void GpioMonitor::stopHoldThread()
{
    m_holdStop.store(true);
    if (m_holdThread) {
        m_holdThread->wait(1500);
        delete m_holdThread;
        m_holdThread = nullptr;
    }
}

void GpioMonitor::lowHoldLoop()
{
#ifdef Q_OS_UNIX
    QElapsedTimer lowTimer;
    bool lastHigh = m_high;

    while (!m_holdStop.load()) {
        // 10 мс: и реакция на фронт, и проверка «уже 100 мс LOW» без GUI-таймера.
        pollfd pfd{};
        pfd.fd = m_lineFd;
        pfd.events = POLLIN | POLLPRI;
        ::poll(&pfd, 1, 10);

        bool high = lastHigh;
        if (!collectLevel(&high)) {
            continue;
        }

        if (high != lastHigh) {
            lastHigh = high;
            const char *reason = high ? "фронт ↑" : "фронт ↓";
            QMetaObject::invokeMethod(this, [this, high, reason]() {
                applyValue(high, reason);
            }, Qt::QueuedConnection);
        }

        if (high) {
            if (lowTimer.isValid()) {
                qInfo("%s: выдержка LOW сброшена", qPrintable(m_pinName));
            }
            lowTimer.invalidate();
            m_lowHeldEmitted.store(false);
        } else {
            if (!lowTimer.isValid()) {
                lowTimer.start();
                qInfo("%s: LOW, выдержка %d мс", qPrintable(m_pinName), m_lowHoldMs);
            } else if (!m_lowHeldEmitted.load()
                       && lowTimer.elapsed() >= m_lowHoldMs) {
                m_lowHeldEmitted.store(true);
                qInfo("%s: LOW удерживается %d мс",
                      qPrintable(m_pinName), m_lowHoldMs);
                QMetaObject::invokeMethod(this, [this]() {
                    // Queued: слот выключения (журнал + systemctl) должен быть в GUI-потоке.
                    emit lowHeld();
                }, Qt::QueuedConnection);
            }
        }
    }
#else
    Q_UNUSED(this);
#endif
}

void GpioMonitor::onLineEvent()
{
#ifdef Q_OS_UNIX
    // Только GPIO без выдержки (GPIO4_C5). Питание читается в lowHoldLoop.
    if (m_usingSysfs) {
        bool high = false;
        if (readSysfsValue(&high)) {
            applyValue(high, "фронт");
        }
        return;
    }

    gpio_v2_line_event event{};
    while (true) {
        const ssize_t n = ::read(m_lineFd, &event, sizeof(event));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            qWarning("%s: ошибка чтения события: %s",
                     qPrintable(m_pinName), std::strerror(errno));
            break;
        }
        if (n != static_cast<ssize_t>(sizeof(event))) {
            break;
        }

        applyValue(event.id == GPIO_V2_LINE_EVENT_RISING_EDGE,
                   event.id == GPIO_V2_LINE_EVENT_RISING_EDGE
                   ? "фронт ↑"
                   : "фронт ↓");
    }
#else
    Q_UNUSED(this);
#endif
}

void GpioMonitor::pollValue()
{
    // Только линии без выдержки (GPIO4_C5): IRQ мог не прийти.
    bool value = false;
    if (readValue(&value)) {
        applyValue(value, "опрос");
    }
}

void GpioMonitor::closeLine()
{
#ifdef Q_OS_UNIX
    // Сначала остановить поток: иначе close(fd) под ногами у poll/read.
    stopHoldThread();
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_lineFd >= 0) {
        ::close(m_lineFd);
        m_lineFd = -1;
    }
    if (m_chipFd >= 0) {
        ::close(m_chipFd);
        m_chipFd = -1;
    }
    if (m_sysfsExported && m_linuxGpio >= 0) {
        writePath(QStringLiteral("/sys/class/gpio/unexport"),
                  QByteArray::number(m_linuxGpio) + '\n');
        m_sysfsExported = false;
    }
#endif
}

void GpioMonitor::setAvailable(bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    emit availableChanged();
}
