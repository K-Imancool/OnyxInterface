#ifndef LINKSTM_H
#define LINKSTM_H

#include <QObject>
#include <QTimer>
#include <QTime>
#include <QQueue>
#include <QThread>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QElapsedTimer>
#include "uartqmlbridge.h"
#include "loggingcategories.h"
#include "Structures.h"

using namespace Onyx;

class LinkStm : public QObject
{
    Q_OBJECT
public:
    explicit LinkStm(QObject *parent = nullptr);
    enum TxCommand : quint8 {           // Передаваемые команды
        Allright = 0x00,                // Базовый запрос

        Activation = 0x20,              // Активация

        StopActivation = 0x40,          // Остановка активации

        AckNeutralResist = 0x61,        // Запрос сопротивления НЭ
        OutputLeds = 0x62,              // Включение/выключение подсветки выходов
        IsnManage = 0x63,               // Управление ИСН
//        AckMonoHandleResist = 0x62,     // Запрос сопр. моно держателя                Пока не используются
//        AckBiHandleResist = 0x63,       // Запрос сопр. би держателя
//        ReadMono2Id = 0x64,             // Прочитать данные в определителе Моно2
//        ReadBi2Id = 0x65,               // Прочитать данные в определителе Би2
//        WriteMono2Id = 0x66,            // Запись данных в определитель Моно2
//        WriteBi2Id = 0x67,              // Запись данных в определитель Би2
        WirelessSearch = 0x68,          // Поиск беспроводных устройств
        TxAddrWireless = 0x69,          // Передача адреса для беспроводных устройств
        TxDataWireless = 0x6A,          // Передача данных для беспроводных устройств
        StopArgon = 0x6B,              // Остановка опроса газового тракта
        ArgonBlow = 0x6C,               // Продувка газового тракта
        ReadyToPowerOff = 0x6F,         // Команда на выключение питания

        SignalAlarm = 0x80,             // Выдача звукового сигнала (аварии)
        Volume1 = 0x81,                 // Громкость, уровень 1
        Volume2 = 0x82,
        Volume3 = 0x83,
        Volume4 = 0x84,
        Volume5 = 0x85,
        Volume6 = 0x86,
        Volume7 = 0x87,                 // Громкость, уровень 7

        CurrentVersion = 0xE0,          // Запрос версий ПО
        Erase = 0xE1,                   // Стереть память под прошивку
        StartUpdate = 0xE2,             // Начало передачи ПО
        SoftData = 0xE3,                // Данные прошивки
        UpdateFinish = 0xE4,            // ПО передано - версия ПО
        GoBoot = 0xE5,                  // Переключение на загрузчик
        GoApp = 0xE6,                   // Переключение на рабочую прошивку

        NoTxCommand = 0xFF
    };
    Q_ENUM(TxCommand);

    enum RxCommand : quint8 {           // Принимаемые команды
        Whatsup = 0x00,                 // Стандартная посылка

        GenActivation = 0x20,           // Посылка во время активации

        GenStop = 0x40,                 // Остановка активации
        GenStopХХ = 0x41,               // Остановка активации по холостому ходу в автостопе
        GenStopShort = 0x42,            // Остановка активации по короткому замыканию бранш
        GenStopNeBreak = 0x43,          // Остановка активации из-за обрыва НЭ
        GenStopNoArgon = 0x44,          // Остановка активации - закончился аргон
        GenStopBlockedArg = 0x45,       // Остановка активации - непроходимость газового тракта
        GenStopErr = 0x4F,              // Остановка активации из-за других ошибок

        SpecAnswer = 0x60,
        NeutralResist = 0x61,           // Сопротивление НЭ
        AckOutputLeds = 0x62,           // Подтверждение включения/выключения подсветки выходов
        IsnDAC = 0x63,                  // Значение ЦАП и АЦП ИСН (4 байта)
//        MonoHandleResist = 0x62,        // Сопротивление моно держателя
//        BiHandleResist = 0x63,          // Сопротивление би держателя
//        DataMono2Id = 0x64,             // Данные из определителя Моно2
//        DataBi2Id = 0x65,               // Данные из определителя Би2
//        ConfirmMono2Id = 0x66,          // Подтверждение записи данных в определитель Моно2
//        ConfirmBi2Id = 0x67,            // Подтверждение записи данных в определитель Би2
        WirelessDetected = 0x68,        // Обнаружено беспроводное устройство
        ConfirmAddrWireless = 0x69,     // Подтверждение приёма адреса для беспроводных устройств
        DataWireless = 0x6A,            // Данные от беспроводного устройства
        StopArgonAck = 0x6B,            // Подтверждение остановки опроса аргона
        ArgonBlowAck = 0x6C,            // Подтверждение продувки баллона
        PowerOff = 0x6F,                // Команда на выключение питания

        ErrComm = 0x80,                 // Модуль связи не принимает сигналы от МИФ
        ErrGenComm = 0x81,              // Генератор не отвечает
        ErrArgComm = 0x82,              // Газовый модуль не отвечает
        ErrRadioModule = 0x83,          // Не отвечает радиомодуль
        ErrStuckButtons = 0x84,         // Кнопки или педали зажаты до старта
        ErrNeController = 0x85,         // МК НЭ не отвечает
        ErrRaskController = 0x86,       // МК раскачки не отвечает
        ErrPowerNe5V = 0x87,            // Питание НЭ 5В не соответствует норме
        ErrPowerNe3V3 = 0x88,           // Питание НЭ 3,3В не соответствует норме
        ErrNeOverheat = 0x89,           // Перегрев контроллера НЭ (выше 80)
        ErrStandBy = 0x8C,              // Кнопка StandBy зажата при включении - переход в сервисный режим
        ErrUpdate = 0x8D,               // Ошибка обновления прошивки
        ErrApp = 0x8E,                  // Нет рабочей прошивки (сидим в загрузчике)

        CritIsnStart = 0x90,            // Критичные ошибки - ИСН при включении
        CritAdc1Ucont = 0x91,           // Ошибка АЦП1 - напряжение контура
        CritAdc2Icont = 0x92,           // Ошибка АЦП2 - ток контура
        CritAdc3Igen = 0x93,            // Ошибка АЦП3 - ток генератора
        CritAdc4Uisn = 0x94,            // Ошибка АЦП4 - напряжение ИСН
        CritRelay = 0x95,               // Ошибка реле
        CritIsnWork = 0x96,             // Ошибка ИСН при нормальной работе
        CritNeResonance = 0x97,         // Не найден резонанс при калибровке НЭ
        CritAdcNe = 0x98,               // Ошибка АЦП схемы НЭ

        Version = 0xE0,                 // Версии ПО
        Erased = 0xE1,                  // Рабочая прошивка стёрта
        ReadyToUpdate = 0xE2,           // Готов принять новую прошивку
        SoftDataAck = 0xE3,             // Принял данные прошивки
        UpdateResult = 0xE4,            // Результаты обновления
        BootAck = 0xE5,                 // Стандартный ответ загрузчика
        Start = 0xE6,                   // Ответ рабочей прошивки после старта

    };
    Q_ENUM(RxCommand);

    enum UartState : quint8 {
        STATE_OK = 0,
        STATE_TX_ERR,                   // Ошибка передачи
        STATE_NO_RX,                    // Приёмник не отвечает
        STATE_RX_ERR,                   // Приёмник отвечает не то, что нужно
        STATE_RX_LEN_ERR,               // Не та длина посылки
        STATE_RX_CRC_ERR,               // crc не совпадает
    };
    Q_ENUM(UartState);

    enum BootChoice : quint8 {
        BOOT_0 = 0,
        BOOT_APP_1 = 1,
        BOOT_APP_2 = 2
    };
    Q_ENUM(BootChoice);

    enum McUnit : quint8 {
        MC_COM = 0,                       // Модуль управления и связи
        MC_ARG = 0x20,                    // Модуль аргоновый (1 << 5)
        MC_GEN = 0x40,                    // Модуль генератора (2 << 5)
        MC_RAS = 0x60,                    // Модуль раскачки (3 << 5)
        MC_NEL = 0x80                     // Модуль нейтральника (4 << 5)
    };
    Q_ENUM(McUnit);

    enum LedOutput : quint8 {
        OUT_ALL = 0x00,                   // Все выходы для подсветки
        OUT_BI1 = 0x01,
        OUT_BI2 = 0x02,
        OUT_MONO1 = 0x03,
        OUT_MONO2 = 0x04
    };
    Q_ENUM(LedOutput);

    enum LedColor : quint8 {
        LED_OFF = 0x00,                   // Подсветка выходов выключена
        LED_GREEN = 0x01,                 // Зелёный цвет
        LED_RED = 0x02,                   // Красный цвет
        LED_BLUE = 0x03,                  // Синий цвет
        LED_YELLOW = 0x04,                // Жёлтый цвет
        LED_PURPLE = 0x05,                // Пурпурный цвет
        LED_CYAN = 0x06,                  // Бирюзовый цвет
        LED_WHITE = 0x07                  // Белый цвет
    };
    Q_ENUM(LedColor);

    enum IsnDacAction : quint8 {
        IsnDacNone = 0x00,                // Только напряжение / вкл-выкл
        IsnDacInc = 0x01,                 // Увеличить значение ЦАП
        IsnDacDec = 0x10,                 // Уменьшить значение ЦАП
        IsnDacSave = 0xFF                 // Запомнить значение ЦАП
    };
    Q_ENUM(IsnDacAction);

    struct UartTx {
        quint8 com;
        QByteArray data;
        McUnit mc;
    };

    struct UartRx {
        RxCommand com;
        QByteArray data;
        McUnit mc;
    };

    struct McVersions {
        McUnit mc;
        quint8 bootVer;
        quint8 bootSubVer;
        quint8 appVer;
        quint8 appSubVer;
    };

    struct HexString {
        uint32_t addr;
        QByteArray data;
    };

    struct ActiveSocket {
        quint8 id;
        bool isCut;
        bool isEnable;
        bool is3rdKnob;
        bool autoMode;
    };

    enum CommunicationState : quint8 {      // Состояние обмена
        IDLE = 0,                           // По умолчанию - просто передача состояния
        START_ACTIVATION = 1,               // Запуск активации
        ACTIVATION = 2,                     // В режиме активации
        UPDATING = 3,                       // В режиме обновления ПО МК
        SPECIAL = 4                         // Спец посылки
    };
    Q_ENUM(CommunicationState);


    // Получить строку с шестнадцатиричными данными
    static QString getHexStr(QByteArray byteArray);
    // Последняя отправленная команда
    UartTx getLastCommand() const;
    // Состояние uart соединения
    const UartState &state() const;


    void setTxCommand(const UartTx &newTxCommand);

    void setTxCommandBoot();

    const UartRx &rxCommand() const;

    void updateTransfer(QList<LinkStm::HexString> hexList, QString versionStr);

    const BootChoice &boot() const;

    void setBoot(const BootChoice &newBoot);

    void setMc(const McUnit &newMc);

    /// Упаковать mcVersions в QVariantList и отправить в sigFirmwareVersionsChanged.
    void publishFirmwareVersions();

    int uartRate() const;

public slots:
    void setUartRate(int newUartRate);
    void start();
    /// Загрузка hex из файла (вызов из потока LinkStm)
    void startFirmwareUpdateFromFile(const QString &filePath, const QString &versionStr, int mcUnitRaw);
    void argonBlow();                                   // Передать команду на продувку
    void setLedOutput(LedOutput out, LedColor color);   // Передать команду на включение/выключение подсветки
    /// Управление ИСН: voltage=0 и dacAction=0 — выкл; иначе voltage 0..110, dacAction — IsnDacAction
    void manageIsn(quint8 voltage, quint8 dacAction);
    void setVolume(int level);
    void setEnableActivation(bool enable);
    void setNeutralElDivided(bool divided);
    void setAutoSSmode(quint8 mode);
    void setSocketAutoMode(int socketIndex, quint8 mode);
    void setBiAutoMode(int socketIndex, quint8 mode);
    void setActivCylinderFirst(bool first);
    void setArgonFlowRate(quint8 rate);
    void setNeutralResistPollEnabled(bool enabled);
    void setDebugUart(bool enabled);
    void requestReadyToPowerOff();
    void requestReadyToPowerOffWithData(quint8 byte0, quint8 byte1);
    void updateSocketData(int socketIndex, quint16 cutModeNum, quint16 coagModeNum, 
                         quint16 cutModePower, quint16 coagModePower, quint8 pedal);
    void updateSocketData(int socketIndex, const Onyx::SocketState& info);
    void initializeAllSockets();
    /// Прервать передачу прошивки МК (пустое message — без сообщения об ошибке в UI).
    void abortFirmwareUpdate(const QString &message);

signals:
    //начинаем имена сигналов с sig или signal чтобы
    //при их наборе и автоподстановке было намного проще
    //их найти все, а не листать список полей и методов
    void sigUnitStateChanged(UnitState state);
//    void neutralElConnectedChanged(bool connected);
//    void txError();
//    void rxError();
    void sigRecieveData(UartRx* rxData);
    void sigError(quint8 error);
    void sigErrorForLog(quint8 error);
    void sigUpdateProgress(int progress);
    void sigReportTx(QString txStr);
    void sigReportRx(QString rxStr, int ms);
    void sigPressed3rdKnob(quint8 socket);
    void sigStartActivation(quint8 socket, bool isCut);
    void sigActivationStartedDetails(quint8 socket, bool isCut, quint16 mode, quint16 power,
                                     bool autoMode, quint8 sourceCode);
    void sigStopActivation(quint8 stopReason);
    void sigPowerOffCommand();
    void sigReadyToPowerOffSent();
    void sigNeutralResistReceived(const QByteArray &data);
    void sigIsnDacReceived(int dacValue, int adcValue);
    // Отладочные строки для полупрозрачного оверлея в main.qml
    void sigDebugOverlayLine(const QString &line);
    /// Версии ПО модулей МК: список из 5 QVariantMap (числа для UI и сравнения с обновлениями).
    void sigFirmwareVersionsChanged(const QVariantList &modules);
    void firmwareUpdateParseError(const QString &message);

private slots:
    void sendCommand();

private:
    // Подготовка передачи команды
    QByteArray packTxCommand();
    // Вычисление контрольной суммы
    quint16 calculateCrc16(QByteArray &buffer, quint8 len);
    // Инициализация версий ПО МК
    void mcVersRequest();
    // Установка версий ПО МК
    void setMcVersions(const UartRx &rxCom);
    // Сброс версий модуля в 0.0 (нет ответа / ошибка связи)
    void clearMcVersionAt(int index);
    void clearMcVersionsForUnit(McUnit unit);
    // Расшифровка команды
    void unpackRxCommand(const QByteArray &rxPacket);
    // Проверка на соответствие
    bool checkRxCommand();
    // Команды для процесса обновления
    void setNextCommand();

    void readRxCommand();
    void reportError(quint8 error);

    ActiveSocket determineSocket(const PedalKnobPressed &pedalKnob);

    void prepareDefaultCommand();
    // Проверка, есть ли команда уже в списке, чтобы не плодить дубликаты
    bool checkCommandList(const UartTx &newTxCommand);

    static const quint8 MAX_PACKET_LEN = 40;        // Длина пакета
     // Команды для байт-стаффинга
    static const quint8 FRAME_START = 0x8A;         // Начало посылки
    static const quint8 FRAME_ESCAPE_CHAR = 0x8B;   // Маскирующий символ
    static const quint8 FRAME_XOR_CHAR = 0x20;      // Значение для преобразования

    static const quint8 UART_ADDR = 0xE0;           // Маска адреса модуля в сети uart
    static const quint8 UART_LEN = 0x1F;            // Маска длины сообщения

private:
//    QByteArray* m_txPacket;
    UartTx m_lastCommand;
    QTimer* m_uartTimer;
    UartToQmlBridge* m_uart;
    UartState m_state;
    UartTx m_txCommand;
    UartRx m_rxCommand;
    /// Пока ждём BootAck после GoBoot — не подменять m_txCommand на обычные команды.
    bool m_fwUpdateAwaitingBoot = false;
    /// Пока ждём ReadyToUpdate после StartUpdate — не подменять m_txCommand на Allright.
    bool m_fwUpdateAwaitingReady = false;
    /// Пока ждём Start после GoApp — не подменять m_txCommand и не брать команды из очереди.
    bool m_fwUpdateAwaitingGoApp = false;
    int m_softSize;
    int m_transferredSize;
    QList<HexString> m_hexList;
    QString m_versionStr;
    QQueue<UartTx> m_txCommandQueue;
    QList<UartTx> m_txCommandList;
    bool m_waitAnswer;
    quint8 m_autoSSmode;
    BootChoice m_boot;
    McUnit m_mc;
    McVersions mcVersions[5] = {};

    // Переменные состояния из ControlCenter
    bool m_enableActivation;
    bool m_neutralElDivided;
    quint8 m_argonFlowRate;
    bool m_activCylinderFirst;

    UnitState m_unitState;
    Onyx::SocketState m_socketList[4];
    CommunicationState m_comState;

    bool m_fwUpdateSessionActive = false;
    QElapsedTimer m_fwRxErrStreakTimer;
    bool m_abortFirmwareUpdatePending = false;
    bool m_moduleHasWorkingApp[5] = {false, false, false, false, false};
    quint8 m_lastReportedUiError = 0;
    QElapsedTimer m_uiErrorReportTimer;
    /// После запуска активации не воспринимать удерживаемую педаль как новое нажатие.
    bool m_activationInputConsumedUntilRelease = false;
    /// После sigPressed3rdKnob не повторять, пока педали/кнопки не вернутся в PRESS_NONE
    bool m_thirdKnobSignalConsumedUntilRelease = false;
    bool m_neutralResistPollEnabled = false;
    bool m_debugUart = false;
    bool m_readyToPowerOffPending = false;
    QByteArray m_readyToPowerOffData;

    int m_uartRate;
    /// Повтор одного и того же кода в UI не чаще этого интервала.
    static constexpr qint64 ERROR_UI_REPORT_INTERVAL_MS = 1000;
};

#endif // LINKSTM_H
