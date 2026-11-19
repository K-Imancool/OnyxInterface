#include "keygenerator.h"
#include "devicetype.h"
#include <QByteArray>
#include <QDebug>

// Секретная соль - можно изменить на любую другую строку для уникальности ключей
const QString KeyGenerator::SECRET_SALT = "FotekSurgicalSystem2026SecretKey";

KeyGenerator::KeyGenerator(QObject *parent)
    : QObject(parent)
{
}

QString KeyGenerator::generateHash(int serialNumber)
{
    // Создаем строку из номера и соли
    QString input = QString::number(serialNumber) + SECRET_SALT;
    
    // Генерируем SHA-256 хеш
    QByteArray hash = QCryptographicHash::hash(
        input.toUtf8(),
        QCryptographicHash::Sha256
    );
    
    // Преобразуем в hex строку
    return QString(hash.toHex());
}

QString KeyGenerator::hashToKey(const QString &hash)
{
    // Извлекаем цифры из hex строки и формируем 10-значный ключ
    QString digits;
    
    for (int i = 0; i < hash.length() && digits.length() < 10; ++i) {
        QChar c = hash[i];
        if (c.isDigit()) {
            digits.append(c);
        }
    }
    
    // Если цифр недостаточно, используем ASCII коды букв
    if (digits.length() < 10) {
        for (int i = 0; i < hash.length() && digits.length() < 10; ++i) {
            QChar c = hash[i];
            if (c.isLetter()) {
                // Используем последнюю цифру ASCII кода
                int asciiCode = c.toLatin1();
                digits.append(QString::number(asciiCode % 10));
            }
        }
    }
    
    // Гарантируем ровно 10 цифр
    if (digits.length() > 10) {
        digits = digits.left(10);
    } else if (digits.length() < 10) {
        // Дополняем нулями если нужно (не должно произойти)
        while (digits.length() < 10) {
            digits.append('0');
        }
    }
    
    return digits;
}

QString KeyGenerator::generateKey(int serialNumber)
{
    // Проверка корректности входного номера (должен быть 6-значным)
    if (serialNumber < 0 || serialNumber > 999999) {
        qWarning() << "Serial number must be between 0 and 999999";
        return QString();
    }
    
    QString key = generateUnlockKey(serialNumber, QStringLiteral("ONYX-AM"), QStringLiteral("ENDO"));
    
    qDebug() << "Generated key" << key << "for serial number" << serialNumber;
    
    return key;
}

bool KeyGenerator::validateKey(int serialNumber, const QString &key)
{
    // Проверка корректности входных данных
    if (serialNumber < 0 || serialNumber > 999999) {
        qWarning() << "Serial number must be between 0 and 999999";
        return false;
    }
    
    return validateUnlockKey(serialNumber, QStringLiteral("ONYX-AM"), QStringLiteral("ENDO"), key);
}

QString KeyGenerator::normalizeDeviceType(const QString &deviceType) const
{
    // Канон для хеша ключа — старые ID, чтобы ONYX Pro / ONYX-A Pro
    // и ONYX-M / ONYX-AM давали один и тот же unlock-ключ.
    return DeviceType::canonicalId(deviceType);
}

QString KeyGenerator::normalizeFeatureCode(const QString &featureCode) const
{
    const QString normalized = featureCode.trimmed().toUpper();
    if (normalized == QStringLiteral("ENDO")) {
        return QStringLiteral("1");
    }
    return normalized;
}

QString KeyGenerator::deviceSalt(const QString &deviceType) const
{
    return normalizeDeviceType(deviceType) == QStringLiteral("ONYX-M")
            ? QStringLiteral("OnyxMDeviceSalt-8F39C1")
            : QStringLiteral("OnyxAMDeviceSalt-42B7E6");
}

QString KeyGenerator::featureSalt(const QString &featureCode) const
{
    const QString code = normalizeFeatureCode(featureCode);
    if (code == QStringLiteral("1")) {
        return QStringLiteral("EndoscopyUnlockSalt-C58E20");
    }
    return QStringLiteral("OnyxFeatureKeySalt-%1-v1").arg(code);
}

QString KeyGenerator::generateUnlockKey(int serialNumber,
                                        const QString &deviceType,
                                        const QString &featureCode)
{
    if (serialNumber < 260000 || serialNumber > 1000000) {
        qWarning() << "Serial number must be between 260000 and 1000000";
        return QString();
    }

    const QString normalizedDeviceType = normalizeDeviceType(deviceType);
    const QString normalizedFeatureCode = normalizeFeatureCode(featureCode);
    const QString input = QStringLiteral("ONYX-UNLOCK-v1|%1|%2|%3|%4|%5")
            .arg(normalizedDeviceType)
            .arg(normalizedFeatureCode)
            .arg(serialNumber)
            .arg(deviceSalt(normalizedDeviceType))
            .arg(featureSalt(normalizedFeatureCode));

    const QByteArray hash = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);

    quint64 value = 0;
    for (int i = 0; i < 8 && i < hash.size(); ++i) {
        value = (value << 8) | static_cast<quint8>(hash.at(i));
    }

    const quint64 key = 100000000000ULL + (value % 900000000000ULL);
    return QString::number(key);
}

bool KeyGenerator::validateUnlockKey(int serialNumber,
                                     const QString &deviceType,
                                     const QString &featureCode,
                                     const QString &key)
{
    const QString normalizedKey = key.trimmed();
    if (normalizedKey.length() != 12) {
        qWarning() << "Unlock key must be exactly 12 digits";
        return false;
    }

    for (QChar c : normalizedKey) {
        if (!c.isDigit()) {
            qWarning() << "Unlock key must contain only digits";
            return false;
        }
    }

    const QString correctKey = generateUnlockKey(serialNumber, deviceType, featureCode);
    const bool isValid = !correctKey.isEmpty() && normalizedKey == correctKey;

    qDebug() << "Unlock key validation:" << (isValid ? "SUCCESS" : "FAILED")
             << "- Serial:" << serialNumber
             << "- Device:" << normalizeDeviceType(deviceType)
             << "- Feature:" << normalizeFeatureCode(featureCode);

    return isValid;
}
