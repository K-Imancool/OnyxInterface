#include "databasereader.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

DataBaseReader::DataBaseReader(const QString& pathToDb, const QString& connectionName, bool readOnly)
    : QObject(nullptr)
    , m_connectionName(connectionName.isEmpty() ? QStringLiteral("etoBasa") : connectionName)
{
    QSqlDatabase someDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);

    QFileInfo fileInfo(pathToDb);
    QString absolutePath;

    if (fileInfo.isRelative()) {
        QString appDir = QCoreApplication::applicationDirPath();
        absolutePath = appDir + QLatin1Char('/') + pathToDb;
    } else {
        absolutePath = fileInfo.absoluteFilePath();
    }

    QFileInfo finalFileInfo(absolutePath);
    if (!finalFileInfo.exists()) {
        QString m_lastError = QStringLiteral("Файл базы данных не найден: %1").arg(absolutePath);
        (void)m_lastError;
    }

    someDb.setDatabaseName(absolutePath);
    if (readOnly) {
        someDb.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    }
}

QList<QVariantList> DataBaseReader::slotSendSelectQuery(const QStringList &tables,
                                         const QStringList &columns,
                                         const QString &conditions) const
{
    QList<QVariantList> result;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.open();

    QSqlQuery query(db);
    QString tmp;
    if (conditions.isEmpty()) {
        QString queryString("SELECT %1 FROM %2");
        tmp = queryString.arg(columns.join(',')).arg(tables.join(','));
        query.prepare(tmp);
    } else {
        QString queryString("SELECT %1 FROM %2 WHERE %3");
        tmp = queryString.arg(columns.join(',')).arg(tables.join(',')).arg(conditions);
        query.prepare(tmp);
    }

    query.setForwardOnly(true);

    if (!query.exec()) {
        // qDebug() << query.lastError().text() << tmp;
    }

    int colCount = columns.size();
    while (query.next()) {
        QVariantList rowRes;
        for (int i = 0; i < colCount; ++i)
            rowRes.append(query.value(i));

        result.append(rowRes);
    }

    return result;
}

bool DataBaseReader::executeUpdateQuery(const QString &queryStr) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.open()) {
        // qWarning() << "Failed to open database for update query";
        return false;
    }

    QSqlQuery query(db);

    if (!query.exec(queryStr)) {
        qWarning() << "Update query failed:" << query.lastError().text();
        qWarning() << "Query was:" << queryStr;
        return false;
    }

    return true;
}

bool DataBaseReader::beginTransaction() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.open()) {
        qWarning() << "beginTransaction fail";
        return false;
    }
    return db.transaction();
}

void DataBaseReader::rollback() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.open()) {
        qWarning() << "rollback fail";
        return;
    }
    db.rollback();
}

void DataBaseReader::commit() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.open()) {
        qWarning() << "commit fail";
        return;
    }
    db.commit();
}
