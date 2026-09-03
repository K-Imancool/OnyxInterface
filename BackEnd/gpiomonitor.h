#ifndef GPIOMONITOR_H
#define GPIOMONITOR_H

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QString>

#include <atomic>

class QSocketNotifier;
class QThread;
class QTimer;

/**
 * @brief Одна GPIO-линия SoC Rockchip RK3566 (ROC-RK3566 / Armbian).
 *
 * Нумерация Rockchip: GPIOx_Yz, где банк x = gpiochip, группа Y = A/B/C/D (0..3),
 * пин z = 0..7. Смещение линии на чипе: Y*8 + z.
 *   GPIO0_D5 → gpio0, offset 29 (D=3, 3*8+5), MMIO gpio@fdd60000
 *   GPIO4_C5 → gpio4, offset 21 (C=2, 2*8+5), MMIO gpio@fe770000
 *
 * Доступ: Linux GPIO character device v2 (/dev/gpiochipN). Процесс UI идёт
 * от пользователя kikorik — нужны права на узлы (udev: deploy/gpio/).
 * Если character device недоступен, пробуется устаревший sysfs /sys/class/gpio.
 *
 * По умолчанию линия — вход с прерываниями по обоим фронтам. Смена уровня
 * пишется в stderr через qInfo. setHigh() переводит пин в выход (для монитора
 * питания GPIO0_D5 этого не делать: внешняя схема сама задаёт уровень).
 *
 * setLowHoldMs(): отдельный поток считает, сколько линия держит LOW.
 * Используется для GPIO0_D5 (монитор питания): 100 мс LOW → сигнал lowHeld().
 * Поток нужен потому, что QTimer в GUI-потоке сбрасывался опросом GET_VALUES
 * на том же fd, что и события, и выдержка 100 мс никогда не доходила до конца.
 */
class GpioMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString pinName READ pinName CONSTANT)
    Q_PROPERTY(bool high READ high WRITE setHigh NOTIFY highChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    /**
     * @param pinName     Имя для логов, напр. "GPIO0_D5"
     * @param chipLabel   Метка ядра gpiochip: "gpio0" / "gpio4"
     * @param mmioPrefix  Адрес контроллера в DTB (fdd60000 / fe770000), если
     *                    label вида "fdd60000.gpio"
     * @param lineOffset  Номер линии внутри банка 0..31
     */
    GpioMonitor(const QString &pinName,
                const QByteArray &chipLabel,
                const QByteArray &mmioPrefix,
                unsigned lineOffset,
                QObject *parent = nullptr);
    ~GpioMonitor() override;

    QString pinName() const { return m_pinName; }
    bool high() const { return m_high; }
    bool available() const { return m_available; }

    /// Открыть чип, захватить линию, прочитать старт и начать слежение.
    void start();
    /// Перевести линию в выход и выставить уровень. Для входа питания не вызывать.
    Q_INVOKABLE bool setHigh(bool high);
    /// Порог непрерывного LOW (мс). >0 — слежение в отдельном потоке, сигнал lowHeld().
    void setLowHoldMs(int milliseconds);

signals:
    void highChanged();
    void availableChanged();
    /// Непрерывный LOW не короче setLowHoldMs(). Эмитится из GUI-потока (Queued).
    void lowHeld();

private:
    bool openChip();
    bool openSysfsChip();
    bool requestLine();
    bool requestCharDevLine();
    bool requestSysfsLine();
    bool readValue(bool *high) const;
    bool readSysfsValue(bool *high) const;
    /// Слить события с fd и вернуть текущий уровень. После первого фронта
    /// доверяем ему, а не GET_VALUES: ioctl на event-fd давал ложный HIGH
    /// и сбрасывал выдержку питания.
    bool collectLevel(bool *high);
    void applyValue(bool high, const char *reason);
    void onLineEvent();
    void pollValue();
    void startWatching();
    void startHoldThread();
    void stopHoldThread();
    /// Цикл потока: poll(10 мс) → collectLevel → накопление LOW → lowHeld().
    void lowHoldLoop();
    void closeLine();
    void setAvailable(bool available);

    const QString m_pinName;
    const QByteArray m_chipLabel;
    const QByteArray m_mmioPrefix;
    const unsigned m_lineOffset;
    const QByteArray m_consumer;

    int m_chipFd = -1;                 ///< /dev/gpiochipN, пока линия не запрошена
    int m_lineFd = -1;                 ///< fd линии (события + ioctl) либо sysfs value
    int m_linuxGpio = -1;              ///< глобальный номер для sysfs export (base+offset)
    bool m_high = false;
    bool m_available = false;
    bool m_outputMode = false;
    bool m_usingSysfs = false;
    bool m_sysfsExported = false;      ///< true, если export делали мы — тогда unexport
    QString m_sysfsGpioDir;
    int m_lowHoldMs = 0;
    bool m_haveEdgeLevel = false;      ///< уже был хотя бы один фронт — GET_VALUES игнорируем
    bool m_edgeHigh = false;           ///< уровень по последнему фронту
    std::atomic<bool> m_lowHeldEmitted {false};
    std::atomic<bool> m_holdStop {false};
    QSocketNotifier *m_notifier = nullptr;
    QTimer *m_pollTimer = nullptr;
    QThread *m_holdThread = nullptr;
    mutable QMutex m_lineMutex;        ///< ioctl/read line fd из потока выдержки
};

#endif // GPIOMONITOR_H
