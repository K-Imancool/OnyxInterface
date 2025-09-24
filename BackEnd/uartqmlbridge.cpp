#include <QDebug>
#include "uartqmlbridge.h"

UartToQmlBridge::UartToQmlBridge(QObject *parent, QString port, QSerialPort::BaudRate rate)
    : QObject{parent},
      m_portName(port),
      m_baudRate(rate),
      m_serial(new QSerialPort(this)),
      m_waitForAnswer(false)
{
    Q_UNUSED(parent);
    openSerialPort();
    connect(m_serial, &QSerialPort::readyRead, this, &UartToQmlBridge::readData);
    connect(m_serial, &QSerialPort::errorOccurred, this, &UartToQmlBridge::handleError);
}

UartToQmlBridge::~UartToQmlBridge()
{
    closeSerialPort();
}

//QByteArray UartToQmlBridge::command()
//{
//    return m_command;
//}

//QByteArray UartToQmlBridge::response()
//{
//    return m_response;
//}

void UartToQmlBridge::openSerialPort()
{
    m_serial->setPortName(m_portName);
    m_serial->setBaudRate(m_baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);
    m_serial->close();
    if (!m_serial->open(QIODevice::ReadWrite))
        qDebug() << (QString("Can't open serial port ").append(m_portName));
}

void UartToQmlBridge::closeSerialPort()
{
    if (m_serial->isOpen())
        m_serial->close();
}

void UartToQmlBridge::handleError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError)
        emit errorOccurred(tr("Ошибка порта: %1").arg(m_serial->errorString()));
}

bool UartToQmlBridge::writeData(const QByteArray &txData)
{
    quint16 writedByte;
    // Записываем данные в SerialPort
    writedByte = m_serial->write(txData);
    if (writedByte != txData.size() ||
        !m_serial->waitForBytesWritten(10))
        return false;

    m_writeTime = QTime::currentTime();
    m_waitForAnswer = true;
    return true;
}

QByteArray UartToQmlBridge::readData()
{
    QTime readTime = QTime::currentTime();
    const QByteArray data = m_serial->readAll();
    m_transmitDelay = m_writeTime.msecsTo(readTime);
    // Сбрасываем флаг отправки
    m_waitForAnswer = false;
    if (data != m_rxData) {
        m_rxData = data;
//        emit rxChanged();
    }
    emit uartRecieve(data);
    return m_rxData;
}

bool UartToQmlBridge::waitForAnswer() const
{
    return m_waitForAnswer;
}

int UartToQmlBridge::transmitDelay() const
{
    return m_transmitDelay;
}
