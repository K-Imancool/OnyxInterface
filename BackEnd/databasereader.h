#ifndef DATABASEREADER_H
#define DATABASEREADER_H

#include <QObject>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QSharedPointer>

class DataBaseReader : public QObject
{
    Q_OBJECT
public:
    /// @param connectionName уникальное имя Qt-соединения (несколько файлов SQLite).
    /// @param readOnly только чтение (QSQLITE_OPEN_READONLY).
    DataBaseReader(const QString& pathToDb,
                   const QString& connectionName = QString(),
                   bool readOnly = false);

    QString connectionName() const { return m_connectionName; }

    QList<QVariantList> slotSendSelectQuery(const QStringList &tables,
                             const QStringList& columns,
                             const QString& conditions) const;
    
    /**
     * @brief Выполняет UPDATE/INSERT/DELETE запрос
     * @param queryStr SQL запрос
     * @return true если успешно, false если ошибка
     */
    bool executeUpdateQuery(const QString& queryStr) const;

    bool beginTransaction() const;
    void rollback() const;
    void commit() const;

signals:

    void signalResultReady(const QString& query, const QList<QVariantList> res) const;

private:
    QString m_connectionName;
};

#endif // DATABASEREADER_H
