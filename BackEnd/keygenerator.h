#ifndef KEYGENERATOR_H
#define KEYGENERATOR_H

#include <QObject>
#include <QString>
#include <QCryptographicHash>

class KeyGenerator : public QObject
{
    Q_OBJECT
    
public:
    explicit KeyGenerator(QObject *parent = nullptr);
    
    // Генерация 10-значного ключа из 6-значного номера
    Q_INVOKABLE QString generateKey(int serialNumber);
    
    // Проверка соответствия ключа номеру
    Q_INVOKABLE bool validateKey(int serialNumber, const QString &key);

    // Генерация 12-значного ключа разблокировки с учетом типа генератора и функции
    Q_INVOKABLE QString generateUnlockKey(int serialNumber,
                                          const QString &deviceType,
                                          const QString &featureCode);

    // Проверка 12-значного ключа разблокировки
    Q_INVOKABLE bool validateUnlockKey(int serialNumber,
                                       const QString &deviceType,
                                       const QString &featureCode,
                                       const QString &key);
    
private:
    // Секретная соль для усиления безопасности
    static const QString SECRET_SALT;
    
    // Внутренняя функция генерации хеша
    QString generateHash(int serialNumber);
    
    // Преобразование хеша в 10-значный ключ
    QString hashToKey(const QString &hash);

    QString normalizeDeviceType(const QString &deviceType) const;
    QString normalizeFeatureCode(const QString &featureCode) const;
    QString deviceSalt(const QString &deviceType) const;
    QString featureSalt(const QString &featureCode) const;
};

#endif // KEYGENERATOR_H
