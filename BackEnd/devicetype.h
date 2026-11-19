#ifndef DEVICETYPE_H
#define DEVICETYPE_H

#include <QString>

/**
 * Тип генератора: отображаемые имена ONYX Pro / ONYX-A Pro.
 * Для совместимости принимаются и старые коды ONYX-M / ONYX-AM.
 * В unlock-ключах и тегах файлов канон остаётся ONYX-M / ONYX-AM.
 */
namespace DeviceType {

inline QString defaultType()
{
    return QStringLiteral("ONYX-A Pro");
}

inline QString displayNameOnyxPro()
{
    return QStringLiteral("ONYX Pro");
}

inline QString displayNameOnyxAPro()
{
    return QStringLiteral("ONYX-A Pro");
}

inline QString normalized(const QString &deviceType)
{
    return deviceType.trimmed().toUpper();
}

inline bool isOnyxM(const QString &deviceType)
{
    const QString n = normalized(deviceType);
    return n == QLatin1String("ONYX-M")
            || n == QLatin1String("ONYX PRO")
            || n == QLatin1String("ONYXPRO");
}

inline bool isOnyxAm(const QString &deviceType)
{
    const QString n = normalized(deviceType);
    return n == QLatin1String("ONYX-AM")
            || n == QLatin1String("ONYX-A PRO")
            || n == QLatin1String("ONYX A PRO")
            || n == QLatin1String("ONYXAPRO");
}

/** Отображаемое имя; пустое/неизвестное → ONYX-A Pro (как прежний дефолт). */
inline QString displayName(const QString &deviceType)
{
    if (deviceType.trimmed().isEmpty()) {
        return defaultType();
    }
    return isOnyxM(deviceType) ? displayNameOnyxPro() : displayNameOnyxAPro();
}

/** Канон для unlock-ключей и внутренней совместимости. */
inline QString canonicalId(const QString &deviceType)
{
    return isOnyxM(deviceType) ? QStringLiteral("ONYX-M") : QStringLiteral("ONYX-AM");
}

/** Короткий тег для имён файлов: m / am. */
inline QString fileTag(const QString &deviceType)
{
    return isOnyxM(deviceType) ? QStringLiteral("m") : QStringLiteral("am");
}

/** Компактный код в журнале: M / AM (как раньше после среза префикса). */
inline QString compactLogCode(const QString &deviceType)
{
    return isOnyxM(deviceType) ? QStringLiteral("M") : QStringLiteral("AM");
}

inline QString displayNameFromCompact(const QString &compactCode)
{
    const QString n = normalized(compactCode);
    if (n == QLatin1String("M") || n == QLatin1String("PRO") || isOnyxM(compactCode)) {
        return displayNameOnyxPro();
    }
    return displayNameOnyxAPro();
}

} // namespace DeviceType

#endif // DEVICETYPE_H
