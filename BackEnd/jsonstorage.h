#ifndef JSONSTORAGE_H
#define JSONSTORAGE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>

class JsonStorage : public QObject
{
    Q_OBJECT
public:
    explicit JsonStorage(QObject *parent = nullptr, QVariantMap* initMap = nullptr);

    ~JsonStorage();


    void save(QString key, QJsonValue data);
    bool read(QString key, QJsonValue* data);

    Q_INVOKABLE void saveString(const QString& key, const QString& value);
    Q_INVOKABLE QString readString(const QString& key, const QString& defaultValue = QString()) const;
    Q_INVOKABLE void saveInt(const QString& key, int value);
    Q_INVOKABLE int readInt(const QString& key, int defaultValue = 0) const;
//    template <typename T> void save(QString key, T);
//    template <typename T> bool read(QString key, T*);

signals:

private:
    QJsonDocument m_document;
    QJsonObject m_object;

    // Продакшен-путь настроек
    const QString JSON_FILE_NAME = "/var/lib/qtpr/save.json";
    // Легаси-путь задаётся через AppPaths::legacySaveJsonPath()

};

#endif // JSONSTORAGE_H
