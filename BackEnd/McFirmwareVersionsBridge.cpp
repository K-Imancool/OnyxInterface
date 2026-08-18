#include "McFirmwareVersionsBridge.h"

#include <QVariantMap>

McFirmwareVersionsBridge::McFirmwareVersionsBridge(QObject *parent)
    : QObject(parent)
{
}

bool McFirmwareVersionsBridge::modulesDataEqual(const QVariantList &a, const QVariantList &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    static const QStringList keys = {
        QStringLiteral("index"),
        QStringLiteral("mcUnit"),
        QStringLiteral("bootMain"),
        QStringLiteral("bootSub"),
        QStringLiteral("appMain"),
        QStringLiteral("appSub"),
        QStringLiteral("reportsBootAndApp1"),
        QStringLiteral("hasWorkingApp"),
        QStringLiteral("appCorrupted"),
        QStringLiteral("appStatus"),
    };
    for (int i = 0; i < a.size(); ++i) {
        const QVariantMap ma = a.at(i).toMap();
        const QVariantMap mb = b.at(i).toMap();
        for (const QString &k : keys) {
            if (ma.value(k) != mb.value(k)) {
                return false;
            }
        }
    }
    return true;
}

void McFirmwareVersionsBridge::setModules(const QVariantList &list)
{
    if (modulesDataEqual(m_modules, list)) {
        return;
    }
    m_modules = list;
    emit modulesChanged();
}
