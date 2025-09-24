#include "stmupdater.h"

StmUpdater::StmUpdater(QObject *parent,
                       QFileInfo *fileInfo)
    : QObject{parent}
{
    QFile hexFile;
    // Открываем файл с прошивкой
    hexFile.setFileName(fileInfo->absoluteFilePath());
    if (!hexFile.open(QIODevice::ReadOnly |QIODevice::Text))
        emit updateError("Ошибка открытия hex-файла");
    // Считываем все строки файла в список
    while(!hexFile.atEnd())
        m_hexStrList << hexFile.readLine();

    hexFile.close();
    qDebug() << "считали hex в m_hexStrList: " << QString::number(m_hexStrList.size());






//        readTranslateString(str);
////            str = hexFile.readLine();
//        readSize += str.length();
//        ui->labelDebug->setText(str);
//        hexRecordType = parseHexString(str, &data, &addrOffset);
//        progress = (readSize*100)/fileSize;
//        QString progressStr = "PR: ";
//        progressStr.append(QString::number(progress)).append("Read: ");
//        progressStr.append(QString::number(readSize)).append("FileS: ");
//        progressStr.append(QString::number(fileSize));

//            QTimer::singleShot(2000, [this, progress, progressStr](){
//                ui->progressBarUpdate->setValue(progress);
//                ui->labelFlag->setText(progressStr);});

//    }

}

StmUpdater::~StmUpdater()
{
    //    m_hexFile.close();
}

QList<LinkStm::HexString> StmUpdater::getHexList()
{
    QList<LinkStm::HexString> hexList;
    static uint16_t addrHighByte = 0;
    bool endOfHex = false;
    const QString errStr[] = {"Ошибка чтения hex-файла",
                       "Ошибка контрольной суммы hex-строки",
                       "Ошибка символов в hex-файле",
                       "Ошибка чтения hex-файла"};
    for (const auto& hexStr : m_hexStrList) {
        QByteArray hexData;
        uint16_t addrOffset = 0;
        HexRecordType recordType = parseHexString(hexStr, &hexData, &addrOffset);
        if (recordType >= HexRecordType::HexError) {
            emit updateError(errStr[static_cast<int>(recordType) - static_cast<int>(HexRecordType::HexError)]);
            break;
        }
        // Расширение адреса - первые два байта (0х0800, например)
        if (recordType == HexRecordType::HexExtendLinearAddress) {
            addrHighByte = (hexData.at(0) << 8) | hexData.at(1);
            qDebug() << "ExtendedLinearAddress " << QString::number(addrHighByte);
        }
        // Данные прошивки, которые надо будет передать
        else if (recordType == HexRecordType::HexData) {
            LinkStm::HexString hexPack;
            hexPack.addr = (addrHighByte << 16) | addrOffset;
            hexPack.data = hexData;
            hexList.append(hexPack);
        }
        else if (recordType == HexRecordType::HexEndOfFile) {
            endOfHex = true;
            qDebug() << "endOfHex " << QString::number(hexList.size());
            break;
        }
    }
    if (!endOfHex) {
        emit updateError("Ошибка завершения hex-файла");
    }
    return hexList;
}

StmUpdater::HexRecordType StmUpdater::parseHexString(const QString &hexStr, QByteArray *data, uint16_t *addrOffset)
{
    uint8_t input[hexStr.length()/2 - 1] = {0};   // Входные данные в байтах
    uint8_t crcResult = 0;
    bool ok;
    int numOfBytes = 0;
    HexRecordType recordType;               // Тип записи

    // Обнуляем массив
    data->clear();

    // Каждая строка в hex файле должна начинаться с :
    if (hexStr.at(0) != ":")
        return HexRecordType::HexErrorSym;

    // Формируем массив байтов
    for (int i = 1; i < hexStr.length() - 1; i += 2) {
        QString byteStr;
        byteStr.append(hexStr.at(i)).append(hexStr.at(i+1));
        int byte = byteStr.toLatin1().toUInt(&ok, 16);
        if (!ok)
            return HexRecordType::HexErrorSym;
        input[i/2] = (uint8_t) byte;
        crcResult += (uint8_t) byte;
    }

    // Проверка контрольной суммы
    if (crcResult != 0)
        return HexRecordType::HexErrorCrc;

    // Количество байт данных
    numOfBytes = input[0];
    // Смещение адреса размещения байт
    *addrOffset = (input[1] << 8) + input[2];
    //Тип записи
    if (input[3] <= 5)
        recordType = static_cast<HexRecordType>(input[3]);
    else
        return HexRecordType::HexErrorType;
    // Данные прошивки
    for (int i = 0; i < numOfBytes; ++i) {
        data->append(input[4 + i]);
    }

    return recordType;
}
