#include "databasereader.h"

#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

DataBaseReader::DataBaseReader(const QString& pathToDb)
    : QObject(nullptr)
{
    QSqlDatabase someDb = QSqlDatabase::addDatabase("QSQLITE", "etoBasa");

    qDebug() << "Исходный путь:" << pathToDb;

    // Преобразуем относительный путь в абсолютный
    QFileInfo fileInfo(pathToDb);
    QString absolutePath;

    if (fileInfo.isRelative()) {
        // Если путь относительный, используем папку с исполняемым файлом
        QString appDir = QCoreApplication::applicationDirPath();
        qDebug() << "Папка приложения:" << appDir;
        absolutePath = appDir + "/" + pathToDb;
        qDebug() << "Относительный путь преобразован в абсолютный:" << absolutePath;
    } else {
        absolutePath = fileInfo.absoluteFilePath();
        qDebug() << "Путь уже абсолютный:" << absolutePath;
    }

    // Проверяем существование файла
    QFileInfo finalFileInfo(absolutePath);
    if (!finalFileInfo.exists()) {
        QString m_lastError = QString("Файл базы данных не найден: %1").arg(absolutePath);
        qWarning() << m_lastError;
    }
    else {
        qDebug() << "Файл базы данных найден:" << absolutePath;
        qDebug() << "Размер файла:" << finalFileInfo.size() << "байт";
        qDebug() << "Права доступа:" << finalFileInfo.permissions();
        qDebug() << "Абсолютный путь:" << absolutePath;
    }

    someDb.setDatabaseName(absolutePath);
}

void DataBaseReader::slotSendQuery(const QString &queryStr, int valueNumbersAwaited)
{
    QList<QVariantList> result;
    QSqlDatabase db = QSqlDatabase::database("etoBasa");
    db.open();

    QSqlQuery query(db);
    query.prepare(queryStr);
    query.setForwardOnly(true);

    if (!query.exec()) {
        qDebug() << query.lastError().text();
    }

    while (query.next()) {
        QVariant res1 = query.value(0);
        QVariant res2 = query.value(1);

        result.append(QVariantList{res1, res2});
    }


    emit signalResultReady(queryStr, result);

}

QList<QVariantList> DataBaseReader::slotSendSelectQuery(const QStringList &tables,
                                         const QStringList &columns,
                                         const QString &conditions)
{
    QList<QVariantList> result;
    QSqlDatabase db = QSqlDatabase::database("etoBasa");
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
        qDebug() << query.lastError().text() << tmp;
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
