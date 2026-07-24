#include "linkstm.h"
#include "stmupdater.h"

#include <QFileInfo>
#include <QMetaType>
#include <QVariantMap>
#include <QElapsedTimer>
//перенёс инициализатор в конструктор - так не происходит инициализация по умолчанию,
// а затем присваивание новых значени1
LinkStm::LinkStm(QObject *parent)
    : QObject{parent},
    m_lastCommand({NoTxCommand, {}, MC_COM}),
    m_uartTimer(new QTimer(this)),
    m_uart(new UartToQmlBridge(this, "ttyS3", QSerialPort::Baud57600)),
    m_state(STATE_OK),
    m_txCommand({Allright, {}, MC_COM}),
    m_waitAnswer(false),
    m_autoSSmode(0),
    m_enableActivation(true),
    m_neutralElDivided(true),
    m_argonFlowRate(0),
    m_comState(IDLE),
    m_debugUart(false),
    m_uartRate(50)
{
    // Регистрируем типы для использования в Qt::QueuedConnection
    qRegisterMetaType<Onyx::UnitState>("Onyx::UnitState");
    qRegisterMetaType<Onyx::UnitState>("UnitState");
    // m_unitState и так с инициализатором по умолчанию
    for (int i = 0; i < 4; i++) {
        m_socketList[i].cutModeNum = 1000;  // Режим не выбран
        m_socketList[i].coagModeNum = 1000; // Режим не выбран
        m_socketList[i].cutModePower = 0;   // Мощность нулевая
        m_socketList[i].coagModePower = 0;  // Мощность нулевая
        m_socketList[i].pedal = 0;          // Педаль не выбрана
        m_socketList[i].autoMode = 0;       // Нет авто режимов
    }

    // До ответа МК показываем 0.0, а не неинициализированный мусор
    static const McUnit kMcUnits[5] = {MC_COM, MC_ARG, MC_GEN, MC_GEN, MC_GEN};
    for (int i = 0; i < 5; ++i) {
        mcVersions[i].mc = kMcUnits[i];
        clearMcVersionAt(i);
    }

    // Таймер обмена по uart
    m_uartTimer->setTimerType(Qt::PreciseTimer);
    connect(m_uartTimer, &QTimer::timeout, [this]() {sendCommand();});

    connect(m_uart, &UartToQmlBridge::uartRecieve, this, &LinkStm::unpackRxCommand);

    mcVersRequest();

}

void LinkStm::start()
{
    if (m_uartTimer && !m_uartTimer->isActive()) {
        m_uartTimer->start(100);
    }
}

void LinkStm::argonBlow()
{
    UartTx argonBlowCommand;
    argonBlowCommand.com = ArgonBlow;
    argonBlowCommand.mc = MC_COM;
    argonBlowCommand.data.clear();
    setTxCommand(argonBlowCommand);
}

void LinkStm::setLedOutput(LedOutput out, LedColor color)
{
    UartTx ledOutputCommand;
    ledOutputCommand.com = OutputLeds;
    ledOutputCommand.mc = MC_COM;
    ledOutputCommand.data.clear();
    ledOutputCommand.data.append(static_cast<quint8>(out));
    ledOutputCommand.data.append(static_cast<quint8>(color));
    setTxCommand(ledOutputCommand);
}

void LinkStm::setVolume(int level)
{
    const int clamped = qBound(1, level, 7);
    UartTx volumeCommand;
    volumeCommand.com = static_cast<quint8>(SignalAlarm + clamped);
    volumeCommand.mc = MC_COM;
    volumeCommand.data.clear();
    setTxCommand(volumeCommand);
}

void LinkStm::requestReadyToPowerOff()
{
    m_enableActivation = false;
    m_readyToPowerOffData.clear();
    m_readyToPowerOffPending = true;
}

void LinkStm::requestReadyToPowerOffWithData(quint8 byte0, quint8 byte1)
{
    m_enableActivation = false;
    m_readyToPowerOffData.clear();
    m_readyToPowerOffData.append(static_cast<char>(byte0));
    m_readyToPowerOffData.append(static_cast<char>(byte1));
    m_readyToPowerOffPending = true;
}

void LinkStm::setNeutralResistPollEnabled(bool enabled)
{
    if (m_neutralResistPollEnabled == enabled) {
        return;
    }
    m_neutralResistPollEnabled = enabled;
    emit sigNeutralResistReceived(QByteArray());
}

void LinkStm::setDebugUart(bool enabled)
{
    m_debugUart = enabled;
}

void LinkStm::unpackRxCommand(const QByteArray &rxPacket)
{
//    qDebug() << "[LinkStm] unpackRxCommand, bytes:" << rxPacket.size();

    // Замеряем задержку от момента readyRead (readData) до входа в unpackRxCommand
    QTime now = QTime::currentTime();
    int delayFromReadyRead = m_uart->lastReadTime().msecsTo(now);
//    qDebug() << "Delay readyRead -> unpackRxCommand:" << delayFromReadyRead << "ms";

    // Засекаем время обработки unpackRxCommand + checkRxCommand
    QElapsedTimer processTimer;
    processTimer.start();

    quint8 xorValue = 0;
    quint8 rxByte = 0;
    QByteArray destuffedBuffer;         // Для отработки байт-стаффинга

    m_rxCommand.data.clear();

//    qDebug() << "Rx: " << getHexStr(rxPacket) << "ms: " << m_uart->transmitDelay();  // DEBUG
    if (m_debugUart)
        emit sigDebugOverlayLine(QStringLiteral("Rx: %1").arg(getHexStr(rxPacket)));
//    emit sigReportRx(getHexStr(rxPacket), m_uart->transmitDelay());

    // Проверка длины посылки
    if (rxPacket.size() < 4) {
        qDebug() << "Посылка короткая";
        qDebug() << "Rx: " << getHexStr(rxPacket) << "ms: " << m_uart->transmitDelay();
        m_state = STATE_RX_LEN_ERR;
        return;
    }

    // Посылка должна начинаться с FRAME_START
    if (rxPacket.at(0) != FRAME_START) {
        qDebug() << "FrameStart не найден";
        m_state = STATE_RX_ERR;
        return;
    }

    // Делаем обратный байт-стаффинг
    for (int i = 1; i < rxPacket.size(); i++) {
        if (rxPacket.at(i) == FRAME_ESCAPE_CHAR)
            xorValue = FRAME_XOR_CHAR;
        else {
            rxByte = rxPacket.at(i) ^ xorValue;
            destuffedBuffer.append(rxByte);
            xorValue = 0;
        }
    }
    if (xorValue != 0) {
        qWarning() << "Rx packet ends with escape byte, dropping packet";
        m_state = STATE_RX_ERR;
        return;
    }
    if (destuffedBuffer.isEmpty()) {
        qWarning() << "Destuffed packet is empty";
        m_state = STATE_RX_LEN_ERR;
        return;
    }

//    qDebug() << "Rx destuffed buffer: " << getHexStr(destuffedBuffer);  // DEBUG

    // Длина буфера
    int packetLen = (destuffedBuffer.at(0) & UART_LEN)*2 + 4;
    if (packetLen != destuffedBuffer.size()) {
        QString errStr("заявленная длина: ");
        errStr.append(QString::number(packetLen));
        errStr.append(" байт, реальная: ");
        errStr.append(QString::number(destuffedBuffer.size()));
//        qDebug() << errStr;                     // DEBUG
        m_state = STATE_RX_LEN_ERR;
        return;
    }


    //Проверка CRC
    if (calculateCrc16(destuffedBuffer, destuffedBuffer.size()) != 0) {
        m_state = STATE_RX_CRC_ERR;
        qDebug() << "CRC не совпадает";
    }
    // Модуль, от которого пришло сообщение
    m_rxCommand.mc = (McUnit) (destuffedBuffer.at(0) & UART_ADDR);
    // Выбираем команду
    m_rxCommand.com = static_cast<RxCommand>(destuffedBuffer.at(1));

//    qDebug() << "rxCom: " << m_rxCommand.com << ": " << QString::number(destuffedBuffer.at(1), 16); // DEBUG
    m_rxCommand.data.clear();
    for (int i = 2; i < destuffedBuffer.size() - 2; i++)
        m_rxCommand.data.append(destuffedBuffer.at(i));
    // Проверяем соответствие

    if (checkRxCommand()) {
        m_state = STATE_OK;
        m_fwRxErrStreakTimer.invalidate();
        if (m_fwUpdateSessionActive && m_lastCommand.com == UpdateFinish
            && static_cast<quint8>(m_rxCommand.com) == static_cast<quint8>(UpdateFinish)) {
            m_fwUpdateSessionActive = false;
        }
    }
    // Повторяем команду, если ответ не подходящий
    else {
        m_state = STATE_RX_ERR;
//        qDebug() << "не тот ответ от stm";      // DEBUG
        if ((m_fwUpdateSessionActive || m_fwUpdateAwaitingGoApp) && !m_fwUpdateAwaitingBoot) {
            if (!m_fwRxErrStreakTimer.isValid()) {
                m_fwRxErrStreakTimer.start();
            } else if (m_fwRxErrStreakTimer.elapsed() >= 4000) {
                abortFirmwareUpdate(tr("Не удалось обновить модуль"));
            }
        }
    }

    m_waitAnswer = false;

    // Логируем время обработки входящего пакета
//    qDebug() << "unpackRxCommand+checkRxCommand took" << processTimer.elapsed() << "ms";  // DEBUG
}

// Первичная проверка, то пришло, или нет
bool LinkStm::checkRxCommand()
{
    // Адреса во флэш-памяти, куда записывались данные
    uint32_t addrT = 0;
    uint32_t addrR = 0;

    switch (m_txCommand.com) {
    case CurrentVersion:
        return m_rxCommand.com == Version
                || m_rxCommand.com == ErrGenComm
                || m_rxCommand.com == ErrArgComm ? true : false;
    case Erase:
        return m_rxCommand.com == Erased ? true : false;
    case StartUpdate:
        return m_rxCommand.com == ReadyToUpdate ? true : false;
    case GoBoot:
        return m_rxCommand.com == BootAck ? true : false;
    case UpdateFinish:
        return m_rxCommand.com == UpdateResult ? true : false;
    case GoApp:
        return m_rxCommand.com == Start ? true : false;
    case SoftData:
        if (m_rxCommand.com != SoftDataAck || m_rxCommand.data.size() < 4)
            return false;
        // Сравниваем адреса во флэш, переданный и принятый
        for (int i = 0; i < 4; i++) {
            addrT |= m_txCommand.data.at(i) << (24 - i*8);
            addrR |= m_rxCommand.data.at(i) << (24 - i*8);
        }
        if (addrT != addrR)
            return false;
        return true;
    }
    return true;
}

void LinkStm::sendCommand()
{
    // Засекаем время начала отправки (отладка была удалена)
    // QTime startTime = QTime::currentTime();
    
    QByteArray txPacket;
    static ActiveSocket activeSocket = {99, false, false, false, false};   // Активированный сокет
    static int mode = 1000;             // Активированный режим
    static int power = 0;               // Активированная мощность
    static int updateCounter = 0;       // Счётчик обновления ПО
    static int updateProgr = 0;
    static UartState preState = STATE_OK;   // Предыдущее состояние
    static int errCounter = 0;

    if (m_abortFirmwareUpdatePending) {
        updateCounter = 0;
        updateProgr = 0;
        m_abortFirmwareUpdatePending = false;
    }

    m_comState = IDLE;

    // Не дождались адекватного ответа (unpackRxCommand)
    if (m_waitAnswer)
       m_state = STATE_NO_RX;
    //_____________ Проверяем ответ rx__________
    if (m_state != STATE_OK) {
        if (m_state != preState) {
            // qDebug() << "UART-ошибки: " << m_state;
            emit sigError(m_state);             // Ошибки ответа
            preState = m_state;                 // Запоминаем предыдущее состояние
            errCounter = 0;                     // Сбрасываем счётчик ошибок
            if (m_state == STATE_NO_RX && m_lastCommand.com == CurrentVersion) {
                clearMcVersionsForUnit(m_lastCommand.mc);
            }
        }
        else if (errCounter++ > 100) {
            m_state = STATE_OK;                 // Делаем попытку выйти на нормальную работу
            errCounter = 0;
        }
    }
    else {
        if (!m_waitAnswer) {
            preState = STATE_OK;
        }
        errCounter = 0;
    }

    if (m_state == STATE_OK) {
        readRxCommand();                    // Читаем ответ

        if (m_readyToPowerOffPending && !m_fwUpdateSessionActive && !m_fwUpdateAwaitingGoApp) {
            m_txCommand.com = ReadyToPowerOff;
            m_txCommand.data = m_readyToPowerOffData;
            m_readyToPowerOffData.clear();
            m_txCommand.mc = MC_COM;
            m_readyToPowerOffPending = false;
            m_comState = SPECIAL;
//            qDebug() << "txCom: " << m_txCommand.com << ": " << QString::number(m_txCommand.com, 16);

        } else if (!m_neutralResistPollEnabled && !m_txCommandList.isEmpty()
            && !m_fwUpdateSessionActive && !m_fwUpdateAwaitingGoApp) {   // Какую-то спец команду надо передать
            m_txCommand = m_txCommandList.takeFirst();
            m_comState = SPECIAL;
//            qDebug() << "txCom: " << m_txCommand.com << ": " << QString::number(m_txCommand.com, 16);

        }

        //______________Подготовка активации________________
        if (m_comState == START_ACTIVATION) {
            m_comState = IDLE;
            activeSocket = determineSocket(m_unitState.pedalKnob);

            if (activeSocket.isCut) {
                mode = m_socketList[activeSocket.id].cutModeNum;
                power = m_socketList[activeSocket.id].cutModePower;
            }
            else {
                mode = m_socketList[activeSocket.id].coagModeNum;
                power = m_socketList[activeSocket.id].coagModePower;

            }
            if (m_enableActivation && (mode < 32) && (power > 0) && (power <= 400)) {
                    activeSocket.autoMode = m_socketList[activeSocket.id].autoMode > 0 ? true : false;
                    QElapsedTimer m_elapsedTimer;
                    m_elapsedTimer.start();
                    emit sigActivationStartedDetails(activeSocket.id, activeSocket.isCut,
                                                     mode, power, activeSocket.autoMode,
                                                     static_cast<quint8>(m_unitState.pedalKnob));
                    emit sigStartActivation(activeSocket.id, activeSocket.isCut);
                    qint64 afterEmit = m_elapsedTimer.elapsed();
                    if (afterEmit > 5) {
                        qDebug() << "Activation emit took" << (afterEmit) << "ms";
                    }
//                    qDebug() << "Activation " << mode << power;
                    m_comState = ACTIVATION;
            }
        }
        //__________________Команда активации_________________
        if (m_comState == ACTIVATION) {
            if (activeSocket.id >= 4) m_comState = IDLE;
            else if (!m_enableActivation) m_comState = IDLE;
            else {
                m_txCommand.com = Activation;
                m_txCommand.com |= activeSocket.id << 3;
                m_txCommand.com |= activeSocket.autoMode ? (1 << 2) : 0;
                m_txCommand.com |= m_neutralElDivided ? (1 << 1) : 0;
                m_txCommand.data.clear();
                m_txCommand.data.append(power >> 1);        // Старшие биты мощности
                m_txCommand.data.append((power % 2) << 7);  // Младший бит мощности
                m_txCommand.data.append(mode);              // Режим
                // В старшем бите 0 - использовать первый баллон, 1 - второй; дальше - установленный расход от 0,0 до 8,0
                m_txCommand.data.append((m_activCylinderFirst ? 0 : 0x80) | (m_argonFlowRate & 0x7F));
//                qDebug() << "ACT " << getHexStr(m_txCommand.data) << "Ar: " << m_argonFlowRate;
            }
        }

        //__________________Команда по умолчанию_________________
        if (m_comState == IDLE && !m_fwUpdateAwaitingBoot && !m_fwUpdateAwaitingReady
            && !m_fwUpdateAwaitingGoApp) {
            if (m_neutralResistPollEnabled) {
                m_txCommand.com = AckNeutralResist;
                m_txCommand.data.clear();
                m_txCommand.mc = MC_COM;
            } else {
                if (m_unitState.pedalKnob == PRESS_NONE
                    || m_unitState.pedalKnob == PRESS_WRONG) {
                    m_thirdKnobSignalConsumedUntilRelease = false;
                } else {
                    activeSocket = determineSocket(m_unitState.pedalKnob);
                    if (activeSocket.is3rdKnob && activeSocket.id < 4) {
                        if (!m_thirdKnobSignalConsumedUntilRelease) {
                            emit sigPressed3rdKnob(activeSocket.id);
                            m_thirdKnobSignalConsumedUntilRelease = true;
                        }
                    }
                }
                m_txCommand.com = Allright;
                if (m_socketList[0].autoMode == 2) {            // Режим АСС на выходе БИ1
                    m_txCommand.com |= 1 << 2;
                }
                if (m_socketList[1].autoMode == 2) {            // Режим АСС на выходе БИ2
                    m_txCommand.com |= 3 << 2;
                }
                m_txCommand.com |= m_neutralElDivided ? (1 << 1) : 0;
                m_txCommand.com |= m_enableActivation ? 0 : 1;  // Запрет активации
                m_txCommand.data.clear();
                m_txCommand.mc = MC_COM;
            }
        }

        //______________Обновление________________
        if (m_comState == UPDATING) {
            if (m_rxCommand.com == Start) {        // После перезагрузки МК
                if (m_fwUpdateAwaitingGoApp) {
                    m_fwUpdateAwaitingGoApp = false;
                    m_mc = MC_COM;
                    m_txCommand.mc = MC_COM;
                    emit sigUpdateProgress(-1);
                }
                setTxCommandBoot();
            }
            else if ((m_lastCommand.com == StartUpdate) ||
                     (m_lastCommand.com == SoftData)) {
                if (m_hexList.size() > updateCounter) {
                    m_txCommand.com = SoftData;
                    m_txCommand.data.clear();
                    // Формируем команду с данными прошивки по одной из строк hex-файла
                    for (int i = 0; i < 4; i++) {
                        m_txCommand.data.append((uint8_t)(m_hexList.at(updateCounter).addr >> 8*(3-i)));
                    }
                    m_txCommand.data.append(m_hexList.at(updateCounter++).data);
                    // Уведомляем о прогрессе обновления
                    m_transferredSize++;
                    int progress = static_cast<int>(100 * m_transferredSize / m_softSize);
//           qDebug() << "SoftSize: " << m_softSize << " trans: " << m_transferredSize << " progr: " << progress;
                    if (progress > updateProgr) {
                        updateProgr = progress;
                        emit sigUpdateProgress(progress);
                    }
                }
                else {
                    m_txCommand.com = UpdateFinish;
                    m_txCommand.data.clear();
                    updateCounter = 0;
                    updateProgr = 0;
                    int version, subversion;
                    bool isOk;
                    // Строка содержит версию V.V или V (проверяется в MainWindow::on_listView_clicked)
                    if (m_versionStr.contains(".")) {
                        QStringList verList = m_versionStr.split('.');
                        version = verList.at(0).toInt(&isOk, 10);
                        subversion = verList.at(1).toInt(&isOk, 10);
                    }
                    else {
                        version = m_versionStr.toInt(&isOk, 10);
                        subversion = 0;
                    }
                    m_txCommand.data.append((uchar) version);
                    m_txCommand.data.append((uchar) subversion);
                    qDebug() << "new ver: " << getHexStr(m_txCommand.data);
                }
            } // softData
            else if (m_lastCommand.com == UpdateFinish) {
                m_txCommand.com = GoApp;                // После обновления надо перейти в основную прошивку
                m_txCommand.data.clear();
                m_fwUpdateAwaitingGoApp = true;
                m_comState = IDLE;
            }
        } // updating
        //______________Спец команда________________
        if (m_comState == SPECIAL) {
             m_comState = IDLE;
        }
    } // rx good

    //  Если были ошибки m_state отправится та же команда повторно

    // Разная задержка ожидания ответа для разных команд
    switch (m_txCommand.com) {
    case StartUpdate:
    case GoApp:
    case Erase:
    case GoBoot:
        m_uartTimer->setInterval(3000);  // Стирание банка около 6 сек, перезагрузка 3-4 сек
        break;
    case SoftData:
        m_uartTimer->setInterval(100);
        break;
    default:
        m_uartTimer->setInterval(m_uartRate);
//        m_uartTimer->setInterval(50);
    }

//    qDebug() << "txCom: " << m_txCommand.com << ": " << QString::number(m_txCommand.com, 16);
    // Собираем команду в посылку
    m_lastCommand = m_txCommand;        // Сохраняем отправленную команду
    txPacket = packTxCommand();
    // Отправляем посылку
    QString txStr;
    if (!m_uart->writeData(txPacket)) {
        m_state = STATE_TX_ERR;
        txStr = "!Tx ERROR";
//         qDebug() << "Tx ERR!";
   }
   else {
        txStr = getHexStr(txPacket);
//        qDebug() << "Tx: " << getHexStr(txPacket);   // DEBUG
        if (m_debugUart)
            emit sigDebugOverlayLine(QStringLiteral("Tx: %1").arg(getHexStr(txPacket)));
        if (m_txCommand.com == ReadyToPowerOff) {
            emit sigReadyToPowerOffSent();
        }
    }
    
    // Отладочный замер времени от таймера до reportTx удалён
    
    emit sigReportTx(txStr);
    m_waitAnswer = true;
}


QByteArray LinkStm::packTxCommand()
{
    QByteArray buffer(MAX_PACKET_LEN, 0);
    QByteArray packet;
    quint16 crc;
    quint8 i;

    // Заполняем буфер: адрес модуля + длина, команда, данные, crc
    buffer[0] = (quint8) m_txCommand.mc | (m_txCommand.data.length() / 2); // Т.к. у нас переменна только длина данных, а они кратны 2, то можно передавать меньше
    buffer[1] = m_txCommand.com;

    for (i = 0; i < m_txCommand.data.length(); i++) {
        buffer[i+2] = m_txCommand.data.at(i);
    }
    i += 2;
    crc = calculateCrc16(buffer, m_txCommand.data.length() + 2);
    buffer[i++] = (quint8) (crc >> 8);
    buffer[i++] = (quint8) (crc & 0xFF);

    // Организуем байт-стаффинг, когда FRAME_START всегда начало посылки
    packet.append(FRAME_START);
    for (quint8 j = 0; j < i; j++) {
        if (buffer.at(j) == FRAME_START || buffer.at(j) == FRAME_ESCAPE_CHAR) {
           packet.append(FRAME_ESCAPE_CHAR);
           packet.append(buffer.at(j) ^ FRAME_XOR_CHAR);
        }
        else packet.append(buffer.at(j));
    }

    return packet;
}

QString LinkStm::getHexStr(QByteArray byteArray)
{
    QString outStr;
    for (int i = 0; i < byteArray.size(); i++) {
        QString num = QString::number(byteArray.at(i), 16);
        if (num.length() < 2) outStr.append("0x0");
        else outStr.append("0x");
        outStr.append(num).append(" ");
    }
    return outStr;
}



void LinkStm::readRxCommand()
{
    enum RxType : quint8 {
        RxDefault = 0,
        RxActivation = 1,
        RxStop = 2,
        RxSpecial = 3,
        RxErrors = 4,
        RxCritical = 5,
        RxUpdating = 7
    } rxType;

    rxType = static_cast<RxType> (m_rxCommand.com >> 5);

    // Если шлёт что-то не то во время активации, надо послать команду на стоп на всякий случай
    if (m_comState == ACTIVATION) {
        if (rxType != RxActivation && rxType != RxStop) {
            UartTx command;
            command.com = StopActivation;
            command.data.clear();
            command.mc = MC_COM;
            m_txCommandList.append(command);
        }
    }

    // Начинаем с копии текущего состояния
    UnitState unitState = m_unitState;
    bool forceUnitStateEmit = false;

    switch (rxType) {     // Три старших бита определяют тип посылки
    // Стандартная посылка
    case RxDefault:
    {
        // Обновляем только те поля, которые пришли в этой посылке
        unitState.argonCylinder1 = m_rxCommand.com & 0x08 ? true : false;
        unitState.argonCylinder2 = m_rxCommand.com & 0x04 ? true : false;
        unitState.tissueGrab = m_rxCommand.com & 0x02 ? true : false;
        unitState.neutraElConnected = m_rxCommand.com & 0x01 ? true : false;

//        qDebug() << "баллон 1: " << unitState.argonCylinder1 << " баллон 2: " << unitState.argonCylinder2;

        // Преобразуем байт в enum PedalKnobPressed
        if (!m_rxCommand.data.isEmpty()) {
            quint8 pressValue = static_cast<quint8>(m_rxCommand.data.at(0));

            // Проверяем, соответствует ли значение одному из допустимых enum
            switch (pressValue) {
            case PRESS_MONO1_Y:
            case PRESS_MONO1_B:
            case PRESS_MONO2_Y:
            case PRESS_MONO2_B:
            case PRESS_TERMO:
            case PRESS_PED1:
            case PRESS_PED2_Y:
            case PRESS_PED2_B:
                unitState.pedalKnob = static_cast<PedalKnobPressed>(pressValue);
                qDebug() << "Pressed pedal: " << unitState.pedalKnob << "current m_comState:" << m_comState;
                if (m_enableActivation && !m_neutralResistPollEnabled && m_comState != ACTIVATION) {
                    m_comState = START_ACTIVATION;
                    qDebug() << "Set m_comState to START_ACTIVATION";
                    qDebug() << m_rxCommand.com << " " << getHexStr(m_rxCommand.data);
                } else {
                    qDebug() << "m_comState is ACTIVATION, not setting START_ACTIVATION";
                }
                break;
            case PRESS_MONO1_YB:
            case PRESS_MONO2_YB:
            case PRESS_PED2_YB:
            case PRESS_NONE:
                unitState.pedalKnob = static_cast<PedalKnobPressed>(pressValue);
                break;
            default:
                unitState.pedalKnob = PRESS_WRONG;
                 qWarning() << "Invalid pedal/knob press value:" << Qt::hex << pressValue;
                break;
            }
            
            if (m_rxCommand.data.size() > 1) {
                quint8 otherByte = static_cast<quint8>(m_rxCommand.data.at(1));
                unitState.pedalCharge = otherByte >> 5;
                unitState.instrBi2 = static_cast<Onyx::InstrumentConnected>((otherByte >> 3) & 0x03);
                unitState.instrMono2 = static_cast<Onyx::InstrumentConnected>((otherByte >> 1) & 0x03);
            }
        } else {
            // qDebug() << "Посылка от stm отстой - нет нажатий кнопок";
            unitState.pedalKnob = PRESS_NONE;
        }
        break;
    }
    // Во время активации
    case RxActivation:
        unitState.activOutput = (m_rxCommand.com >> 3) & 0x03;
        
        // Проверяем наличие данных перед доступом
        if (m_rxCommand.data.size() >= 1) {
            unitState.activMode = static_cast<quint8>(m_rxCommand.data.at(0)) & 0x1F;     // Активированный режим
        }
        
        // TODO - сделать проверку, что режим и выход совпадают с тем, что мы передали при активации
        
        if (m_rxCommand.data.size() >= 2) {
            unitState.argonRealRate = static_cast<quint8>(m_rxCommand.data.at(1)) & 0x3F; // Реальный расход от 0,0 до 8,0 (0-80)
        }
        m_comState = ACTIVATION;
        forceUnitStateEmit = true;
        break;
    // Остановка
    case RxStop:
        unitState.activOutput = 0;  // Сбрасываем активированный выход
        unitState.activMode = 0;    // Сбрасываем активированный режим
//        if (m_unitState != unitState) {
//            m_unitState = unitState;
//            emit sigUnitStateChanged(m_unitState);
//        }
        emit sigStopActivation(m_rxCommand.com);
//        qDebug() << "Stop! m_rxCommand: " << m_rxCommand.com << m_rxCommand.data;
        m_comState = IDLE;
        break;
    // Ответ на спец.запросы
    case RxSpecial:
        if (m_rxCommand.com == NeutralResist) {
            emit sigNeutralResistReceived(m_rxCommand.data);
        } else if (m_rxCommand.com == PowerOff) {
            emit sigPowerOffCommand();
        } else if (m_rxCommand.com == ArgonBlowAck) {
            if (m_rxCommand.data.size() >= 2) {
                unitState.argonRealRate = static_cast<quint8>(m_rxCommand.data.at(1)) & 0x3F; // Реальный расход от 0,0 до 8,0 (0-80)
            }
            forceUnitStateEmit = true;
        }
        break;
    // Присылаемые ошибки
    case RxErrors:
    case RxCritical:
        if (m_lastCommand.com == CurrentVersion) {
            if (m_rxCommand.com == ErrArgComm) {
                clearMcVersionsForUnit(MC_ARG);
            } else if (m_rxCommand.com == ErrGenComm) {
                clearMcVersionsForUnit(MC_GEN);
            } else {
                clearMcVersionsForUnit(m_lastCommand.mc);
            }
        }
        emit sigError(m_rxCommand.com);
        break;
    // Ответы на команды обновления ПО
    case RxUpdating:
        if (m_rxCommand.com == Version) {
            setMcVersions(m_rxCommand);
            m_comState = IDLE;
        }
        else if (m_rxCommand.com == BootAck) {
            qDebug() << "boot ack принят";
            if (m_fwUpdateAwaitingBoot) {
                m_txCommand.com = StartUpdate;
                m_txCommand.data.clear();
                m_txCommand.mc = m_mc;
                m_fwUpdateAwaitingBoot = false;
                m_fwUpdateAwaitingReady = true;
                qDebug() << "Запускаем обновление";
            }
            m_comState = IDLE;
        }
        else {
            m_comState = UPDATING;
            m_fwUpdateAwaitingReady = false;
        }
        break;
    default:
        // Что-то странное пришло
        qWarning() << "Unknown rx command: " << m_rxCommand.com;
        break;
    }

    if (forceUnitStateEmit || m_unitState != unitState) {
        m_unitState = unitState;
        emit sigUnitStateChanged(m_unitState);
    }

//    qDebug() << "m_comState: " << m_comState;
}

LinkStm::ActiveSocket LinkStm::determineSocket(const PedalKnobPressed &pedalKnob)
{
    ActiveSocket socket;
    
    socket.id = 99;
    socket.isCut = true;
    socket.isEnable = false;
    socket.is3rdKnob = false;

    switch (pedalKnob) {
    case PRESS_MONO1_Y:
        socket.id = 2;  // МОНО1
        break;
    case PRESS_MONO1_B:
        socket.id = 2;  // МОНО1
        socket.isCut = false;
        break;
    case PRESS_MONO1_YB:
        socket.id = 2;  // МОНО1
        socket.is3rdKnob = true;
        break;
    case PRESS_MONO2_Y:
        socket.id = 3;  // МОНО2
        break;
    case PRESS_MONO2_B:
        socket.id = 3;  // МОНО2
        socket.isCut = false;
        break;
    case PRESS_MONO2_YB:
        socket.id = 3;  // МОНО2
        socket.is3rdKnob = true;
        break;
    case PRESS_TERMO:
        socket.id = 1;  // БИ2
        socket.isCut = false;
        break;
    case PRESS_PED1:
        // Ищем сокет, в котором выбрана SINGLE_PED
        for (int i = 0; i < 4; i++) {
            if (m_socketList[i].pedal == 1) {
                socket.id = i;
                socket.isCut = true;  // Single педаль - резание
                break;
            }
        }
        break;
    case PRESS_PED2_Y:
    case PRESS_PED2_B:
    case PRESS_PED2_YB:
        // Ищем сокет, в котором выбрана DOUBLE_PED
        for (int i = 0; i < 4; i++) {
            if (m_socketList[i].pedal == 2) {
                socket.id = i;
                if (pedalKnob == PRESS_PED2_B) {
                    socket.isCut = false;  // Коагуляция
                } else if (pedalKnob == PRESS_PED2_YB) {
                    socket.is3rdKnob = true;
                } else {
                    socket.isCut = true;  // PRESS_PED2_Y - резание
                }
                break;
            }
        }
        break;
    default:
        break;
    }
    
    return socket;
}

// Проверка, есть ли команда уже в списке, чтобы не плодить дубликаты
bool LinkStm::checkCommandList(const UartTx &newTxCommand)
{
    for (auto command : m_txCommandList) {
        if (command.com == newTxCommand.com
            && command.mc == newTxCommand.mc
            && command.data == newTxCommand.data) {
                return false;
        }
    }
    return true;
}

int LinkStm::uartRate() const
{
    return m_uartRate;
}

void LinkStm::setUartRate(int newUartRate)
{
    m_uartRate = newUartRate;
}

void LinkStm::setMc(const McUnit &newMc)
{
    m_mc = newMc;
}

const LinkStm::BootChoice &LinkStm::boot() const
{
    return m_boot;
}

void LinkStm::setBoot(const BootChoice &newBoot)
{
    m_boot = newBoot;
//    qDebug() << m_boot;
}

const LinkStm::UartRx &LinkStm::rxCommand() const
{
    return m_rxCommand;
}

void LinkStm::updateTransfer(QList<HexString> hexList, QString versionStr)
{
    m_softSize = hexList.size();
    m_transferredSize = 0;
    m_hexList = hexList;
    m_fwUpdateSessionActive = true;
    m_fwRxErrStreakTimer.invalidate();
    // Организуем отправку
    UartTx txCom;
    txCom.com = LinkStm::GoBoot;
    txCom.data.clear();
    txCom.mc = m_mc;
    m_fwUpdateAwaitingBoot = true;
    m_fwUpdateAwaitingReady = false;
    m_fwUpdateAwaitingGoApp = false;
    m_txCommand = txCom;
    m_versionStr = versionStr;
    qDebug() << "goBoot before startUpdate";
}

void LinkStm::abortFirmwareUpdate(const QString &message)
{
    m_fwUpdateSessionActive = false;
    m_fwRxErrStreakTimer.invalidate();
    m_fwUpdateAwaitingBoot = false;
    m_fwUpdateAwaitingReady = false;
    m_fwUpdateAwaitingGoApp = false;
    m_hexList.clear();
    m_softSize = 0;
    m_transferredSize = 0;
    m_versionStr.clear();
    m_abortFirmwareUpdatePending = true;
    m_state = STATE_OK;
    m_comState = IDLE;
    m_waitAnswer = false;
    m_mc = MC_COM;
    m_txCommand.com = Allright;
    m_txCommand.data.clear();
    m_txCommand.mc = MC_COM;
    emit firmwareUpdateParseError(message);
}

void LinkStm::startFirmwareUpdateFromFile(const QString &filePath, const QString &versionStr, int mcUnitRaw)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        emit firmwareUpdateParseError(tr("Файл прошивки не найден"));
        return;
    }

    StmUpdater parser(nullptr, &fi);
    QString parseErr;
    QObject::connect(&parser, &StmUpdater::updateError, this,
                     [&parseErr](const QString &e) { parseErr = e; },
                     Qt::DirectConnection);

    const QList<HexString> hexList = parser.getHexList();
    if (!parseErr.isEmpty()) {
        emit firmwareUpdateParseError(parseErr);
        return;
    }
    if (hexList.isEmpty()) {
        emit firmwareUpdateParseError(tr("Пустой или некорректный hex"));
        return;
    }

    setMc(static_cast<McUnit>(static_cast<quint8>(mcUnitRaw)));
    updateTransfer(hexList, versionStr);
}

// Ставим новую команду в очередь
void LinkStm::setTxCommand(const UartTx &newTxCommand)
{
    if (checkCommandList(newTxCommand))
        m_txCommandList.append(newTxCommand);
//    m_txCommandQueue.enqueue(newTxCommand);
}

// Команда на переход в разные версии ПО
void LinkStm::setTxCommandBoot()
{
    UartTx bootCommand;
    bootCommand.data.clear();
    bootCommand.mc = MC_COM;

    if (m_boot == BOOT_0)           // Загрузчик
        bootCommand.com = GoBoot;
    else if (m_boot == BOOT_APP_1)  // Банк 1
        bootCommand.com = GoApp;
    else
        bootCommand.com = Allright;
    // Ставим в очередь
    if (checkCommandList(bootCommand))
        m_txCommandList.append(bootCommand);
    bootCommand.com = CurrentVersion;
    if (checkCommandList(bootCommand)) {
        m_txCommandList.append(bootCommand);
        qDebug() << "bootCommand CurrentVersion";
    }
}


quint16 LinkStm::calculateCrc16(QByteArray &buffer, quint8 len)
{
    static const quint16 crc16Table[256] = {										// Таблица для вычисления CRC
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
        0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
        0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
        0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
        0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
        0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
        0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
        0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
        0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
        0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
        0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
        0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
        0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
        0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
        0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
        0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
        0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
        0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
        0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
        0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
        0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
        0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
        0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
    } ;
    quint16 crc = 0xFFFF;									// Инициирующее значение
    quint8 i = 0;
    while (len-- > 0) {											// Длина должна быть кратной двум
        crc = (crc << 8) ^ crc16Table[(crc >> 8) ^ buffer.at(i++)];
    }
    return crc;													// Если данные были с CRC, на выходе должен быть 0
}

void LinkStm::mcVersRequest()
{
    UartTx verCommand;
    verCommand.data.clear();
    verCommand.com = CurrentVersion;

    // Ставим в очередь

    verCommand.mc = MC_ARG;
    if (checkCommandList(verCommand))
        m_txCommandList.append(verCommand);

    verCommand.mc = MC_GEN;
    if (checkCommandList(verCommand))
        m_txCommandList.append(verCommand);

    verCommand.mc = MC_COM;
    if (checkCommandList(verCommand))
        m_txCommandList.append(verCommand);
    // Заглушка
//    McVersions comVer, argVer, genVer, raskVer, nelVer;
//    comVer.mc = MC_COM;
//    comVer.bootVer = 1;
//    comVer.bootSubVer = 0;
//    comVer.appVer = 2;
//    comVer.appSubVer = 1;

//    argVer = {MC_ARG, 2, 1, 4, 44};
//    genVer = {MC_GEN, 3, 2, 6, 66};
//    raskVer = {MC_RAS, 0, 0, 8, 88};
//    nelVer = {MC_NEL, 0, 0, 9, 99};

//    mcVersions[0] = comVer;
//    mcVersions[1] = argVer;
//    mcVersions[2] = genVer;
//    mcVersions[3] = raskVer;
//    mcVersions[4] = nelVer;

}

void LinkStm::clearMcVersionAt(int index)
{
    if (index < 0 || index >= 5) {
        return;
    }
    mcVersions[index].bootVer = 0;
    mcVersions[index].bootSubVer = 0;
    mcVersions[index].appVer = 0;
    mcVersions[index].appSubVer = 0;
    m_moduleHasWorkingApp[index] = false;
}

void LinkStm::clearMcVersionsForUnit(McUnit unit)
{
    const quint8 idx = static_cast<quint8>(unit) >> 5;
    if (idx >= 3) {
        return;
    }
    clearMcVersionAt(idx);
    // Версии раскачки и НЭ приходят от генератора
    if (idx == 2) {
        clearMcVersionAt(3);
        clearMcVersionAt(4);
    }
    publishFirmwareVersions();
}

void LinkStm::setMcVersions(const UartRx &rxCom)
{
    quint8 mc = rxCom.mc >> 5; // модуль связи - 0, аргонник - 1, генератор - 2
//    bool isErrRx = false;
    if (mc >= 5) {
        qWarning() << "setMcVersions: invalid module index" << mc;
        return;
    }
    if (rxCom.data.size() < 4) {
        qWarning() << "setMcVersions: packet too short, size =" << rxCom.data.size();
        clearMcVersionsForUnit(static_cast<McUnit>(mc << 5));
        return;
    }
    mcVersions[mc].bootVer = rxCom.data.at(0);
    mcVersions[mc].bootSubVer = rxCom.data.at(1);
    mcVersions[mc].appVer = rxCom.data.at(2);
    mcVersions[mc].appSubVer = rxCom.data.at(3);
    m_moduleHasWorkingApp[mc] = mcVersions[mc].appVer == 0 ? false : true;

    // Генератор передаёт ещё версии НЭ и раскачки
    if (mc == 2) {
        if (rxCom.data.size() < 8) {
            qWarning() << "setMcVersions: generator packet too short, size =" << rxCom.data.size();
            clearMcVersionAt(3);
            clearMcVersionAt(4);
            publishFirmwareVersions();
            return;
        }
        mcVersions[3].appVer = rxCom.data.at(6);       // Версия раскачки (без подверсий)
        mcVersions[4].appVer = rxCom.data.at(7);       // Версия НЭ
        mcVersions[3].bootVer = 0;
        mcVersions[3].bootSubVer = 0;
        mcVersions[3].appSubVer = 0;
        mcVersions[4].bootVer = 0;
        mcVersions[4].bootSubVer = 0;
        mcVersions[4].appSubVer = 0;
        m_moduleHasWorkingApp[3] = mcVersions[3].appVer == 0 ? false : true;
        m_moduleHasWorkingApp[4] = mcVersions[4].appVer == 0 ? false : true;
    }

    publishFirmwareVersions();
}

static QVariantList packMcVersionsForQml(const LinkStm::McVersions *vs, bool *moduleHasWorkingApp, int count)
{
    QVariantList list;
    list.reserve(count);
    for (int i = 0; i < count; ++i) {
        const LinkStm::McVersions &v = vs[i];
        QVariantMap m;
        m.insert(QStringLiteral("index"), i);
        m.insert(QStringLiteral("mcUnit"), static_cast<int>(v.mc));
        m.insert(QStringLiteral("bootMain"), static_cast<int>(v.bootVer));
        m.insert(QStringLiteral("bootSub"), static_cast<int>(v.bootSubVer));
        m.insert(QStringLiteral("appMain"), static_cast<int>(v.appVer));
        m.insert(QStringLiteral("appSub"), static_cast<int>(v.appSubVer));
        m.insert(QStringLiteral("reportsBootAndApp1"), i < 3);
        m.insert(QStringLiteral("hasWorkingApp"), moduleHasWorkingApp[i]);
        list.append(m);
    }
    return list;
}

void LinkStm::publishFirmwareVersions()
{
    emit sigFirmwareVersionsChanged(packMcVersionsForQml(mcVersions, m_moduleHasWorkingApp, 5));
}

const LinkStm::UartState &LinkStm::state() const
{
    return m_state;
}

LinkStm::UartTx LinkStm::getLastCommand() const
{
    return m_lastCommand;
}

void LinkStm::setEnableActivation(bool enable)
{
    m_enableActivation = enable;
}

void LinkStm::setNeutralElDivided(bool divided)
{
    m_neutralElDivided = divided;
}

void LinkStm::setAutoSSmode(quint8 mode)
{
    m_autoSSmode = mode;
}

void LinkStm::setSocketAutoMode(int socketIndex, quint8 mode)
{
    if (socketIndex < 0 || socketIndex > 3) {
        return;
    }
    if (mode > 2) {
        mode = 2;
    }
    m_socketList[socketIndex].autoMode = mode;
}

void LinkStm::setBiAutoMode(int socketIndex, quint8 mode)
{
    setSocketAutoMode(socketIndex, mode);
}

void LinkStm::setActivCylinderFirst(bool first)
{
    m_activCylinderFirst = first;
}

void LinkStm::setArgonFlowRate(quint8 rate)
{
    m_argonFlowRate = rate;
//    qDebug() << "Aragorn changed to: " << rate;
}

void LinkStm::updateSocketData(int socketIndex, quint16 cutModeNum, quint16 coagModeNum, 
                              quint16 cutModePower, quint16 coagModePower, quint8 pedal)
{
    if (socketIndex >= 0 && socketIndex < 4) {
        m_socketList[socketIndex].cutModeNum = cutModeNum;
        m_socketList[socketIndex].coagModeNum = coagModeNum;
        m_socketList[socketIndex].cutModePower = cutModePower;
        m_socketList[socketIndex].coagModePower = coagModePower;
        m_socketList[socketIndex].pedal = pedal;
        
        // qDebug() << "LinkStm: Updated socket" << socketIndex
        //          << "cutMode:" << cutModeNum << "coagMode:" << coagModeNum
        //          << "cutPower:" << cutModePower << "coagPower:" << coagModePower
        //          << "pedal:" << pedal;
    }
}

void LinkStm::updateSocketData(int socketIndex, const Onyx::SocketState& info)
{
    if (socketIndex >= 0 && socketIndex < 4) {
        const quint8 savedAutoMode = m_socketList[socketIndex].autoMode;
        m_socketList[socketIndex] = info;
        m_socketList[socketIndex].autoMode = savedAutoMode;
    }
}

void LinkStm::initializeAllSockets()
{
    // Этот метод будет вызываться из ControlCenter для инициализации всех сокетов
    // Пока что просто логируем, что инициализация запрошена
    // qDebug() << "LinkStm: initializeAllSockets called - will be implemented by ControlCenter";
}

