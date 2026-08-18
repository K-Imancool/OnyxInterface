#include "userprogtransfercontroller.h"

#include "apppaths.h"
#include "dblocale.h"
#include "onyxapp.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariantMap>

#include <algorithm>

namespace {

QString uniqueConnName(const QString &prefix)
{
    return prefix + QLatin1Char('_') + QString::number(QRandomGenerator::global()->generate());
}

QString quoteIdent(const QString &name)
{
    QString t = name;
    t.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + t + QLatin1Char('"');
}

QString quotedJoin(const QStringList &names)
{
    QStringList quoted;
    quoted.reserve(names.size());
    for (const QString &name : names) {
        quoted.append(quoteIdent(name));
    }
    return quoted.join(QLatin1Char(','));
}

bool tableExists(QSqlDatabase &db, const QString &name)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1"));
    q.addBindValue(name);
    return q.exec() && q.next();
}

QStringList tableColumns(QSqlDatabase &db, const QString &table)
{
    QStringList cols;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(quoteIdent(table)))) {
        return cols;
    }
    while (q.next()) {
        cols.append(q.value(1).toString());
    }
    return cols;
}

QStringList commonColumns(const QStringList &srcCols, const QStringList &dstCols)
{
    QStringList result;
    result.reserve(dstCols.size());
    QSet<QString> srcSet;
    for (const QString &col : srcCols) {
        srcSet.insert(col);
    }
    for (const QString &col : dstCols) {
        if (srcSet.contains(col)) {
            result.append(col);
        }
    }
    return result;
}

bool copySqliteDatabase(const QString &srcPath, const QString &dstPath)
{
    QFile::remove(dstPath);
    if (QFile::copy(srcPath, dstPath) && QFileInfo::exists(dstPath) && QFileInfo(dstPath).size() > 0) {
        return true;
    }
    QFile::remove(dstPath);

    const QString conn = uniqueConnName(QStringLiteral("userprog_snap"));
    bool copied = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(srcPath);
        if (db.open()) {
            QString escaped = dstPath;
            escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
            QSqlQuery q(db);
            copied = q.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped))
                    && QFileInfo::exists(dstPath)
                    && QFileInfo(dstPath).size() > 0;
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return copied;
}

QString sqlInList(const QList<int> &ids)
{
    QStringList parts;
    parts.reserve(ids.size());
    for (int id : ids) {
        parts.append(QString::number(id));
    }
    return parts.join(QLatin1Char(','));
}

QString uniqueProgName(const QSet<QString> &existing, const QString &baseName)
{
    QString name = baseName.trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("Программа");
    }
    if (!existing.contains(name)) {
        return name;
    }
    int n = 2;
    QString candidate;
    do {
        candidate = name + QLatin1Char(' ') + QString::number(n);
        ++n;
    } while (existing.contains(candidate));
    return candidate;
}

int nextMaxId(QSqlDatabase &db, const QString &table, const QString &condition, int floorValue)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT MAX(id) FROM %1 WHERE %2").arg(table, condition))) {
        return floorValue;
    }
    int maxV = floorValue;
    if (q.next() && q.value(0).isValid() && !q.value(0).isNull()) {
        maxV = std::max(maxV, q.value(0).toInt());
    }
    return maxV;
}

bool insertRow(QSqlDatabase &db, const QString &table,
               const QStringList &columns, const QMap<QString, QVariant> &values)
{
    if (columns.isEmpty()) {
        return false;
    }
    QStringList placeholders;
    placeholders.reserve(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
        placeholders.append(QStringLiteral("?"));
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
              .arg(quoteIdent(table), quotedJoin(columns), placeholders.join(QLatin1Char(','))));
    for (const QString &col : columns) {
        q.addBindValue(values.value(col));
    }
    if (!q.exec()) {
        qWarning() << "UserProgTransfer: insert" << table << "failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QMap<QString, QVariant> readRow(const QSqlQuery &q, const QStringList &columns)
{
    QMap<QString, QVariant> values;
    for (int i = 0; i < columns.size(); ++i) {
        values.insert(columns.at(i), q.value(i));
    }
    return values;
}

} // namespace

UserProgTransferController::UserProgTransferController(QObject *parent)
    : QObject(parent)
{
}

void UserProgTransferController::refreshScopes()
{
    m_scopes = loadScopes();
    emit scopesChanged();
}

QVariantList UserProgTransferController::loadScopes() const
{
    QVariantList result;
    auto *app = qobject_cast<OnyxApp *>(qApp);
    if (!app) {
        return result;
    }
    const QSharedPointer<DataBaseReader> reader = app->getUserProgDbReader();
    if (reader.isNull()) {
        return result;
    }

    const QList<QVariantList> rows = reader->slotSendSelectQuery(
                QStringList{QStringLiteral("Scopes")},
                QStringList{QStringLiteral("id"), DbLocale::column(QStringLiteral("Name"))},
                QStringLiteral("id > 1000 ORDER BY id"));
    for (const QVariantList &row : rows) {
        if (row.size() < 2) {
            continue;
        }
        bool ok = false;
        const int id = row.at(0).toInt(&ok);
        if (!ok || id <= 1000) {
            continue;
        }
        int progCount = 0;
        const QList<QVariantList> progRows = reader->slotSendSelectQuery(
                    QStringList{QStringLiteral("Progs")},
                    QStringList{QStringLiteral("id"), QStringLiteral("Prog_NUM")},
                    QStringLiteral("Scope_ID = %1").arg(id));
        for (const QVariantList &prog : progRows) {
            if (prog.isEmpty()) {
                continue;
            }
            if (prog.size() > 1) {
                if (prog.at(1).toInt() % 10 == 0) {
                    ++progCount;
                }
            } else {
                ++progCount;
            }
        }
        QVariantMap item;
        item.insert(QStringLiteral("id"), id);
        item.insert(QStringLiteral("name"), row.at(1).toString().trimmed());
        item.insert(QStringLiteral("progCount"), progCount);
        result.append(item);
    }
    return result;
}

bool UserProgTransferController::exportScopesToFile(const QList<int> &scopeIds,
                                                    const QString &destPath,
                                                    QString *errorHtml)
{
    auto setError = [errorHtml](const QString &text) {
        if (errorHtml) {
            *errorHtml = QStringLiteral("<p>%1</p>").arg(text.toHtmlEscaped());
        }
    };

    QList<int> ids = scopeIds;
    ids.erase(std::remove_if(ids.begin(), ids.end(), [](int id) { return id <= 1000; }), ids.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    if (ids.isEmpty()) {
        setError(QStringLiteral("Не выбраны папки программ."));
        return false;
    }

    const QString srcPath = AppPaths::instance().userProgDbPath();
    if (!QFileInfo::exists(srcPath)) {
        setError(QStringLiteral("База программ пользователя не найдена."));
        return false;
    }

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        setError(QStringLiteral("Не удалось создать временный каталог."));
        return false;
    }
    const QString snapshotPath = QDir(tmp.path()).filePath(QStringLiteral("snapshot.db"));
    if (!copySqliteDatabase(srcPath, snapshotPath)) {
        setError(QStringLiteral("Не удалось скопировать базу программ."));
        return false;
    }

    const QString conn = uniqueConnName(QStringLiteral("userprog_export"));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(snapshotPath);
        if (!db.open()) {
            setError(QStringLiteral("Не удалось открыть копию базы программ."));
        } else if (!tableExists(db, QStringLiteral("Scopes"))
                   || !tableExists(db, QStringLiteral("Progs"))) {
            setError(QStringLiteral("В базе нет таблиц Scopes/Progs."));
        } else {
            const QString inList = sqlInList(ids);
            QSqlQuery q(db);
            if (tableExists(db, QStringLiteral("Lists"))) {
                q.exec(QStringLiteral(
                           "DELETE FROM Lists WHERE Prog_ID IN "
                           "(SELECT id FROM Progs WHERE Scope_ID NOT IN (%1))")
                       .arg(inList));
            }
            q.exec(QStringLiteral("DELETE FROM Progs WHERE Scope_ID NOT IN (%1)").arg(inList));
            q.exec(QStringLiteral("DELETE FROM Scopes WHERE id NOT IN (%1)").arg(inList));
            q.exec(QStringLiteral(
                       "CREATE TABLE IF NOT EXISTS TransferMeta (key TEXT PRIMARY KEY, value TEXT)"));
            q.exec(QStringLiteral(
                       "INSERT OR REPLACE INTO TransferMeta(key, value) VALUES('format', 'onyx-userprogs')"));
            q.exec(QStringLiteral(
                       "INSERT OR REPLACE INTO TransferMeta(key, value) VALUES('version', '1')"));

            QSqlQuery check(db);
            check.exec(QStringLiteral("SELECT COUNT(*) FROM Scopes"));
            int scopeCount = 0;
            if (check.next()) {
                scopeCount = check.value(0).toInt();
            }
            if (scopeCount <= 0) {
                setError(QStringLiteral("В выбранных папках нет данных для экспорта."));
            } else {
                QFile::remove(destPath);
                QString escaped = destPath;
                escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
                if (q.exec(QStringLiteral("VACUUM INTO '%1'").arg(escaped))
                        && QFileInfo::exists(destPath)
                        && QFileInfo(destPath).size() > 0) {
                    ok = true;
                } else {
                    db.close();
                    ok = QFile::copy(snapshotPath, destPath)
                            && QFileInfo::exists(destPath)
                            && QFileInfo(destPath).size() > 0;
                    if (!ok) {
                        setError(QStringLiteral("Не удалось сохранить файл программ."));
                    }
                }
            }
        }
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return ok;
}

bool UserProgTransferController::importFromFile(const QString &srcPath,
                                                QString *summary,
                                                QString *error,
                                                int *importedPrograms,
                                                int *newFolders,
                                                int *existingFolders)
{
    auto setError = [error](const QString &text) {
        if (error) {
            *error = text;
        }
        qWarning() << "UserProgTransfer import:" << text;
    };
    if (summary) {
        summary->clear();
    }
    if (importedPrograms) {
        *importedPrograms = 0;
    }
    if (newFolders) {
        *newFolders = 0;
    }
    if (existingFolders) {
        *existingFolders = 0;
    }

    if (!QFileInfo::exists(srcPath) || QFileInfo(srcPath).size() <= 0) {
        setError(QStringLiteral("Файл программ пуст или не найден."));
        return false;
    }

    auto *app = qobject_cast<OnyxApp *>(qApp);
    if (!app) {
        setError(QStringLiteral("Приложение недоступно."));
        return false;
    }
    const QSharedPointer<DataBaseReader> destReader = app->getUserProgDbReader();
    if (destReader.isNull()) {
        setError(QStringLiteral("База программ пользователя недоступна."));
        return false;
    }

    const QString srcConn = uniqueConnName(QStringLiteral("userprog_import"));
    bool ok = false;
    int importedScopes = 0;
    int reusedScopes = 0;
    int importedProgs = 0;
    {
        QSqlDatabase srcDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), srcConn);
        srcDb.setDatabaseName(srcPath);
        srcDb.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!srcDb.open()) {
            setError(QStringLiteral("Не удалось открыть файл программ."));
        } else if (!tableExists(srcDb, QStringLiteral("Scopes"))
                   || !tableExists(srcDb, QStringLiteral("Progs"))) {
            setError(QStringLiteral("Файл не содержит программы пользователя."));
        } else {
            QSqlDatabase destDb = QSqlDatabase::database(destReader->connectionName());
            if (!destDb.isValid() || !destDb.open()) {
                setError(QStringLiteral("Не удалось открыть базу программ на аппарате."));
            } else if (!destReader->beginTransaction()) {
                setError(QStringLiteral("Не удалось начать запись в базу программ."));
            } else {

                const QStringList srcScopeCols = tableColumns(srcDb, QStringLiteral("Scopes"));
                const QStringList dstScopeCols = tableColumns(destDb, QStringLiteral("Scopes"));
                const QStringList srcProgCols = tableColumns(srcDb, QStringLiteral("Progs"));
                const QStringList dstProgCols = tableColumns(destDb, QStringLiteral("Progs"));
                const QStringList srcListCols = tableExists(srcDb, QStringLiteral("Lists"))
                        ? tableColumns(srcDb, QStringLiteral("Lists"))
                        : QStringList();
                const QStringList dstListCols = tableExists(destDb, QStringLiteral("Lists"))
                        ? tableColumns(destDb, QStringLiteral("Lists"))
                        : QStringList();
                const QStringList scopeCols = commonColumns(srcScopeCols, dstScopeCols);
                const QStringList progCols = commonColumns(srcProgCols, dstProgCols);
                QStringList listCols = commonColumns(srcListCols, dstListCols);
                listCols.removeAll(QStringLiteral("id"));

                if (!scopeCols.contains(QStringLiteral("id"))
                        || !progCols.contains(QStringLiteral("id"))
                        || !progCols.contains(QStringLiteral("Scope_ID"))) {
                    destReader->rollback();
                    setError(QStringLiteral("Несовместимая структура файла программ."));
                } else {
                    QSqlQuery countQ(srcDb);
                    countQ.exec(QStringLiteral("SELECT COUNT(*) FROM Scopes WHERE id > 1000"));
                    int userScopeCount = 0;
                    if (countQ.next()) {
                        userScopeCount = countQ.value(0).toInt();
                    }
                    const QString scopeWhere = userScopeCount > 0
                            ? QStringLiteral("id > 1000")
                            : QStringLiteral("1=1");

                    int nextScopeId = nextMaxId(destDb, QStringLiteral("Scopes"),
                                                QStringLiteral("id >= 1000"), 1000) + 1;
                    if (nextScopeId < 1001) {
                        nextScopeId = 1001;
                    }
                    int nextProgId = nextMaxId(destDb, QStringLiteral("Progs"),
                                               QStringLiteral("id > 1000"), 1000) + 1;
                    if (nextProgId < 1001) {
                        nextProgId = 1001;
                    }

                    bool failed = false;
                    QSqlQuery scopeQ(srcDb);
                    if (!scopeQ.exec(QStringLiteral("SELECT %1 FROM Scopes WHERE %2 ORDER BY id")
                                     .arg(quotedJoin(srcScopeCols), scopeWhere))) {
                        failed = true;
                        setError(QStringLiteral("Не удалось прочитать папки из файла."));
                    }

                    while (!failed && scopeQ.next()) {
                        const QMap<QString, QVariant> srcScope = readRow(scopeQ, srcScopeCols);
                        const int srcScopeId = srcScope.value(QStringLiteral("id")).toInt();
                        const QString nameRu = srcScope.value(QStringLiteral("Name_RU")).toString().trimmed();
                        QString folderName = nameRu;
                        if (folderName.isEmpty()) {
                            folderName = srcScope.value(QStringLiteral("Name_EN")).toString().trimmed();
                        }
                        if (folderName.isEmpty()) {
                            folderName = srcScope.value(QStringLiteral("Name_ES")).toString().trimmed();
                        }
                        if (folderName.isEmpty()) {
                            folderName = QStringLiteral("Папка");
                        }

                        int destScopeId = -1;
                        QSqlQuery findScope(destDb);
                        findScope.prepare(QStringLiteral(
                            "SELECT id FROM Scopes WHERE TRIM(Name_RU) = ? AND id > 1000 LIMIT 1"));
                        findScope.addBindValue(folderName);
                        if (findScope.exec() && findScope.next()) {
                            destScopeId = findScope.value(0).toInt();
                            ++reusedScopes;
                        } else {
                            destScopeId = nextScopeId++;
                            QMap<QString, QVariant> insertVals = srcScope;
                            insertVals.insert(QStringLiteral("id"), destScopeId);
                            if (insertVals.contains(QStringLiteral("Num"))) {
                                insertVals.insert(QStringLiteral("Num"), destScopeId);
                            }
                            if (insertVals.contains(QStringLiteral("Name_RU"))
                                    && insertVals.value(QStringLiteral("Name_RU")).toString().trimmed().isEmpty()) {
                                insertVals.insert(QStringLiteral("Name_RU"), folderName);
                            }
                            if (!insertRow(destDb, QStringLiteral("Scopes"), scopeCols, insertVals)) {
                                failed = true;
                                setError(QStringLiteral("Не удалось добавить папку «%1».").arg(folderName));
                                break;
                            }
                            ++importedScopes;
                        }

                        QSet<QString> existingNames;
                        QSqlQuery namesQ(destDb);
                        namesQ.prepare(QStringLiteral(
                            "SELECT Name_RU FROM Progs WHERE Scope_ID = ?"));
                        namesQ.addBindValue(destScopeId);
                        if (namesQ.exec()) {
                            while (namesQ.next()) {
                                const QString n = namesQ.value(0).toString().trimmed();
                                if (!n.isEmpty()) {
                                    existingNames.insert(n);
                                }
                            }
                        }

                        QSqlQuery progQ(srcDb);
                        progQ.prepare(QStringLiteral("SELECT %1 FROM Progs WHERE Scope_ID = ? ORDER BY id")
                                      .arg(quotedJoin(srcProgCols)));
                        progQ.addBindValue(srcScopeId);
                        if (!progQ.exec()) {
                            failed = true;
                            setError(QStringLiteral("Не удалось прочитать программы папки «%1».")
                                     .arg(folderName));
                            break;
                        }

                        QMap<int, int> progIdMap;
                        while (progQ.next()) {
                            QMap<QString, QVariant> srcProg = readRow(progQ, srcProgCols);
                            const int srcProgId = srcProg.value(QStringLiteral("id")).toInt();
                            const QString origRu = srcProg.value(QStringLiteral("Name_RU")).toString().trimmed();
                            const QString uniqueRu = uniqueProgName(existingNames, origRu);
                            if (uniqueRu != origRu && origRu.isEmpty() == false) {
                                const QString suffix = uniqueRu.mid(origRu.size());
                                if (srcProg.contains(QStringLiteral("Name_EN"))) {
                                    srcProg.insert(QStringLiteral("Name_EN"),
                                                   srcProg.value(QStringLiteral("Name_EN")).toString() + suffix);
                                }
                                if (srcProg.contains(QStringLiteral("Name_ES"))) {
                                    srcProg.insert(QStringLiteral("Name_ES"),
                                                   srcProg.value(QStringLiteral("Name_ES")).toString() + suffix);
                                }
                            } else if (origRu.isEmpty()) {
                                if (srcProg.contains(QStringLiteral("Name_EN"))
                                        && srcProg.value(QStringLiteral("Name_EN")).toString().trimmed().isEmpty()) {
                                    srcProg.insert(QStringLiteral("Name_EN"), uniqueRu);
                                }
                                if (srcProg.contains(QStringLiteral("Name_ES"))
                                        && srcProg.value(QStringLiteral("Name_ES")).toString().trimmed().isEmpty()) {
                                    srcProg.insert(QStringLiteral("Name_ES"), uniqueRu);
                                }
                            }
                            srcProg.insert(QStringLiteral("Name_RU"), uniqueRu);
                            existingNames.insert(uniqueRu);

                            const int destProgId = nextProgId++;
                            srcProg.insert(QStringLiteral("id"), destProgId);
                            srcProg.insert(QStringLiteral("Scope_ID"), destScopeId);
                            if (progCols.contains(QStringLiteral("Prog_NUM"))) {
                                const int progNum = srcProg.value(QStringLiteral("Prog_NUM")).toInt();
                                srcProg.insert(QStringLiteral("Prog_NUM"),
                                               (progNum % 10 == 0) ? progNum : 0);
                            }
                            if (progCols.contains(QStringLiteral("Argon"))) {
                                const QVariant argonVal = srcProg.value(QStringLiteral("Argon"));
                                if (!argonVal.isValid() || argonVal.isNull()) {
                                    srcProg.insert(QStringLiteral("Argon"), 0);
                                }
                            }
                            if (progCols.contains(QStringLiteral("Sub_NUM"))
                                    && (!srcProg.value(QStringLiteral("Sub_NUM")).isValid()
                                        || srcProg.value(QStringLiteral("Sub_NUM")).isNull())) {
                                srcProg.insert(QStringLiteral("Sub_NUM"), 0);
                            }
                            if (!insertRow(destDb, QStringLiteral("Progs"), progCols, srcProg)) {
                                failed = true;
                                setError(QStringLiteral("Не удалось добавить программу «%1».")
                                         .arg(uniqueRu));
                                break;
                            }
                            progIdMap.insert(srcProgId, destProgId);
                            ++importedProgs;
                        }
                        if (failed) {
                            break;
                        }

                        if (!listCols.isEmpty() && !progIdMap.isEmpty()) {
                            QList<int> srcProgIds = progIdMap.keys();
                            const QString progIn = sqlInList(srcProgIds);
                            QSqlQuery listQ(srcDb);
                            if (!listQ.exec(QStringLiteral("SELECT %1 FROM Lists WHERE Prog_ID IN (%2)")
                                            .arg(quotedJoin(srcListCols), progIn))) {
                                failed = true;
                                setError(QStringLiteral("Не удалось прочитать настройки программ."));
                                break;
                            }
                            while (listQ.next()) {
                                QMap<QString, QVariant> srcList = readRow(listQ, srcListCols);
                                const int srcProgId = srcList.value(QStringLiteral("Prog_ID")).toInt();
                                if (!progIdMap.contains(srcProgId)) {
                                    continue;
                                }
                                srcList.insert(QStringLiteral("Prog_ID"), progIdMap.value(srcProgId));
                                if (!insertRow(destDb, QStringLiteral("Lists"), listCols, srcList)) {
                                    failed = true;
                                    setError(QStringLiteral("Не удалось добавить настройки программы."));
                                    break;
                                }
                            }
                        }
                    }

                    if (failed) {
                        destReader->rollback();
                    } else if (importedProgs == 0 && importedScopes == 0 && reusedScopes == 0) {
                        destReader->rollback();
                        setError(QStringLiteral("В файле нет папок программ для загрузки."));
                    } else if (!destDb.commit()) {
                        destReader->rollback();
                        setError(QStringLiteral("Не удалось сохранить загруженные программы."));
                    } else {
                        QSqlQuery checkpoint(destDb);
                        checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
                        ok = true;
                    }
                }
            }
        }
        if (srcDb.isOpen()) {
            srcDb.close();
        }
    }
    QSqlDatabase::removeDatabase(srcConn);

    if (!ok) {
        return false;
    }

    if (importedPrograms) {
        *importedPrograms = importedProgs;
    }
    if (newFolders) {
        *newFolders = importedScopes;
    }
    if (existingFolders) {
        *existingFolders = reusedScopes;
    }
    if (summary) {
        *summary = QStringLiteral("Загружено программ: %1. Новых папок: %2. В существующие папки: %3.")
                   .arg(importedProgs)
                   .arg(importedScopes)
                   .arg(reusedScopes);
    }
    emit importFinished();
    refreshScopes();
    return true;
}
