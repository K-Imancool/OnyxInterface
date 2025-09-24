#ifndef UARTQMLBRIDGE_H
#define UARTQMLBRIDGE_H


#include <QObject>
#include <QString>
#include <QSerialPort>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QTime>

//#include "qqml.h"

class UartToQmlBridge : public QObject
{
    Q_OBJECT
//    Q_PROPERTY(QByteArray command  READ command  WRITE writeData NOTIFY commandChanged)
//    Q_PROPERTY(QByteArray response READ response WRITE writeData NOTIFY responseChanged)

public:
    explicit UartToQmlBridge(QObject *parent = nullptr, QString port = "ttyS3",
                         QSerialPort::BaudRate rate = QSerialPort::Baud57600);
    ~UartToQmlBridge();

    enum ConnectResult : quint8 {
        CONNECT_ACK = 0,      // Подтверждение получено
        CONNECT_NO,           // Нет ответа
        CONNECT_DATA,         // Пришли какие-то данные
        CONNECT_TX_ERR        // Ошибка передачи
    };

//    QByteArray command();
//    QByteArray response();

    quint16 getTimeout() const;

    int transmitDelay() const;

    bool waitForAnswer() const;

signals:
//    void commandChanged();
//    void responseChanged();

    void uartRecieve(const QByteArray &rxData);
    void errorOccurred(const QString &error);

private:
    void openSerialPort();
    void closeSerialPort();
    void handleError(QSerialPort::SerialPortError error);
    QByteArray readData();

public slots:
    bool writeData(const QByteArray &txData);
//    void readData();

private:
    QSerialPort *m_serial = nullptr;
    QByteArray m_txData;
    QByteArray m_rxData;
    QString m_portName;
    QSerialPort::BaudRate m_baudRate;
    QTime m_writeTime;
    int m_transmitDelay;
    bool m_waitForAnswer;
//    QByteArray m_command;
//    QByteArray m_response;
//    bool m_moduleStatus[3];
};

#endif // UARTQMLBRIDGE_H
