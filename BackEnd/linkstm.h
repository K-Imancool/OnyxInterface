#ifndef LINKSTM_H
#define LINKSTM_H

#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QThread>
#include <QDebug>
#include "uartqmlbridge.h"
#include "loggingcategories.h"

class LinkStm : public QObject
{
    Q_OBJECT
public:
    explicit LinkStm(QObject *parent = nullptr);
    enum TxCommand : quint8 {           // Передаваемые команды
        Allright = 0xC0,                // Базовый запрос

        CurrentVersion = 0xF0,          // Запрос версий ПО
        Erase_1 = 0xF1,                 // Стереть банк 1
        Erase_2 = 0xF2,                 // Стереть банк 2
        StartUpdate_1 = 0xF3,           // Начало передачи ПО для банка 1
        StartUpdate_2 = 0xF4,           // Начало передачи ПО для банка 2
        SoftData = 0xF5,                // Данные прошивки
        UpdateFinish = 0xF6,            // ПО передано - версия ПО
        GoBoot = 0xF7,                  // Переключение на загрузчик
        GoBank_1 = 0xF8,                // Переключение на банк 1
        GoBank_2 = 0xF9,                // Переключение на банк 2
        Reboot = 0xFA,                  // Перезагрузка stm
        NoTxCommand = 0xFF,
    };
    Q_ENUM(TxCommand);

    enum RxCommand : quint8 {           // Принимаемые команды
        Whatsup = 0xC0,                 // Стандартный запрос
        MyVersion = 0xF0,               // Версии ПО
        Erased_1 = 0xF1,                // Банк 1 стёрт
        Erased_2 = 0xF2,                // Банк 2 стёрт
        ReadyToUpdate_1 = 0xF3,         // Готов принять новую прошивку в банк 1
        ReadyToUpdate_2 = 0xF4,         // Готов принять новую прошивку в банк 2
        SoftDataAck = 0xF5,             // Принял данные прошивки
        UpdateResult = 0xF6,            // Результаты обновления
        BootAck = 0xF7,                 // Стандартный ответ загрузчика
        Start = 0xFA,                   // Запуск МК
        NoRxCommand = 0xFB,
        RxErrData = 0xFC,               // Ошибка: не те данные для прошивки
        RxErrSub = 0xFD,                // Не отвечает подчинённый МК
        RxErrCrc = 0xFE,                // Ошибки приёма (CRC)
        RxErr = 0xFF,                   // Ошибки приёма
    };
    Q_ENUM(RxCommand);

    enum MC : quint8 {                  // Микроконтроллеры (старшие три бита)
        MAIN = 0x00,                    // Основной (модуль связи)
        SUB1 = 0x20,                    // Подчинённый (модуль генератора, например)
        SUB2 = 0x40,                    // Подчинённый (модуль генератора, например)
        SUB3 = 0x60,                    // Подчинённый (модуль генератора, например)
        SUB4 = 0x80,                    // Подчинённый (модуль генератора, например)
        SUB5 = 0xA0,                    // Подчинённый (модуль генератора, например)
        SUB6 = 0xC0,                    // Подчинённый (модуль генератора, например)
        SUB7 = 0xE0,                    // Подчинённый (модуль генератора, например)

    };
    Q_ENUM(MC);

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
        MC_0 = 0,                       // Модуль связи
        MC_1 = 0x20,                    // Модуль индикации (1 << 5)
        MC_2 = 0x40,                    // Модуль определителя (2 << 5)
        MC_3 = 0x60,                    // Модуль  (3 << 5)
        MC_4 = 0x80,                    // Модуль  (4 << 5)
    };
    Q_ENUM(McUnit);

    struct UartTx {
        TxCommand com;
        QByteArray data;
        McUnit mc;
    };

    struct UartRx {
        RxCommand com;
        QByteArray data;
        McUnit mc;
    };

    struct HexString {
        uint32_t addr;
        QByteArray data;
    };

    // Получить строку с шестнадцатиричными данными
    static QString getHexStr(QByteArray byteArray);
    // Последняя отправленная команда
    UartTx getLastCommand() const;
    // Состояние uart соединения
    const UartState &state() const;

    void setTxCommand(const UartTx &newTxCommand);

    void setTxCommandBoot();

    const UartRx &rxCommand() const;

    void updateTransfer(QList<LinkStm::HexString> hexList, int bank, QString versionStr);

    const BootChoice &boot() const;

    void setBoot(const BootChoice &newBoot);

    void setMc(const McUnit &newMc);

signals:
//    void txError();
//    void rxError();
    void recieveData(UartRx* rxData);
    void error(UartState errorState);
    void updateProgress(int progress);
    void reportTx(QString txStr);
    void reportRx(QString rxStr, int ms);

private slots:
    void sendCommand();

private:
    // Подготовка передачи команды
    QByteArray packTxCommand();
    // Вычисление контрольной суммы
    quint16 calculateCrc16(QByteArray &buffer, quint8 len);
    // Расшифровка команды
    void unpackRxCommand(const QByteArray &rxPacket);
    // Проверка на соответствие
    bool checkRxCommand();
    // Команды для процесса обновления
    void setNextCommand();
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
    bool m_update;
    int m_softSize;
    int m_transferredSize;
    QList<HexString> m_hexList;
    QString m_versionStr;
    QQueue<UartTx> m_txCommandQueue;
    QList<UartTx> m_txCommandList;
    bool m_waitAnswer;
    BootChoice m_boot;
    McUnit m_mc;
};

#endif // LINKSTM_H
