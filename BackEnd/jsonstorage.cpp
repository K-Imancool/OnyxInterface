#include "jsonstorage.h"
#include <QFile>
#include <QVariantMap>

JsonStorage::JsonStorage(QObject *parent, QVariantMap* initMap)
    : QObject{parent}
{
    QFile jsonFile(JSON_FILE_NAME);
    // Читаем файл, если есть
    if (QFile::exists(JSON_FILE_NAME)) {
        if (!jsonFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Ошибка чтения файла json";
        }
        else {
            QByteArray jsonData = jsonFile.readAll();
            m_document = QJsonDocument::fromJson(jsonData);
            m_object = m_document.object();
        }
    }
    else {
        // Если файла нет, заполняем его значениями по умолчанию
        m_object = QJsonObject::fromVariantMap(*initMap);
        m_document.setObject(m_object);
    }
    jsonFile.close();

}

JsonStorage::~JsonStorage()
{
//    save();
}

void JsonStorage::save(QString key, QJsonValue data)
//template <typename T> void JsonStorage::save(QString key, T data)
{
    m_object[key] = data;

    QFile jsonFile(JSON_FILE_NAME);
    if (!jsonFile.open(QIODevice::WriteOnly)) {
        qDebug() << "не открывается json";
    }
    else {
        jsonFile.write(QJsonDocument(m_object).toJson(QJsonDocument::Indented));
        jsonFile.close();
//        qDebug() << "записали " << data << " в " << key;
    }
}

bool JsonStorage::read(QString key, QJsonValue* data)
{
    if (!m_object.contains(key))
        return false;
    QJsonValue savedData = m_object[key];
    *data = savedData;
//    qDebug() << "в " << key << " лежит " << savedData.toInt();
    return true;
}
