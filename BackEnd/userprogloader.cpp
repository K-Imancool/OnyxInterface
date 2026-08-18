#include "userprogloader.h"
#include "dblocale.h"
#include "onyxapp.h"


UserProgLoader::UserProgLoader(QObject *parent)
    : UserProgLoader(true, parent)
{}

UserProgLoader::UserProgLoader(bool deviceHasArgon, QObject *parent)
    : ProgLoaderBase{deviceHasArgon, parent}
{
    if (m_dbReader.isNull()) {
        OnyxApp* app = dynamic_cast<OnyxApp*>(qApp);
        if (app) {
            m_dbReader = app->getUserProgDbReader();
        }

    }
}


std::map<int, QString> UserProgLoader::getPrograms(int scopeID)
{
    // В userProg.db нет группировки рекомендуемых (Prog_NUM % 10).
    // Argon = NULL тоже показываем: иначе импорт без колонки Argon «пропадает».
    const QString queryCondition =
            QStringLiteral("Scope_ID = %1 AND (Argon IS NULL OR Argon = 0 OR Argon = %2)")
            .arg(scopeID)
            .arg(m_deviceHasArgon ? 2 : 1);

    const QList<QVariantList> progListVariant = m_dbReader->slotSendSelectQuery(
                QStringList{QStringLiteral("Progs")},
                QStringList{DbLocale::column("Name"), QStringLiteral("id")},
                queryCondition);

    std::map<int, QString> progList;
    for (const auto& item : progListVariant) {
        if (item.size() < 2) {
            continue;
        }
        progList.insert_or_assign(item.at(1).toInt(), item.at(0).toString());
    }
    return progList;
}

std::map<int, QString> UserProgLoader::getCategories()
{
    QList<QVariantList> scopeListVariant
                = m_dbReader->slotSendSelectQuery(QStringList{"Scopes"},
                                                     QStringList{"id", DbLocale::column("Name")},
                                                     "id > 1000");

    std::map<int, QString> scopeList;
    for (const auto& item : scopeListVariant) {
        bool ok;
        int id = item.at(0).toInt(&ok);
        QString name = item.at(1).toString();
        if (!ok) {
            continue;
        }
        scopeList.insert_or_assign(id, name);
    }
    return scopeList;
}

// void UserProgLoader::deleteProg(int id)
// {
// }
