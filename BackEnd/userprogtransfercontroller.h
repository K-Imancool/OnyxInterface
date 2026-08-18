#ifndef USERPROGTRANSFERCONTROLLER_H
#define USERPROGTRANSFERCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QList>

/// Экспорт выбранных папок userProg.db и слияние импортированного файла
/// с существующими программами (без затирания имён).
class UserProgTransferController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList scopes READ scopes NOTIFY scopesChanged)

public:
    explicit UserProgTransferController(QObject *parent = nullptr);

    QVariantList scopes() const { return m_scopes; }

    Q_INVOKABLE void refreshScopes();

    /// Потокобезопасно: свои SQLite-соединения, не трогает живой DataBaseReader.
    static bool exportScopesToFile(const QList<int> &scopeIds,
                                   const QString &destPath,
                                   QString *errorHtml);

    /// Главный поток: пишет в живую userProg.db.
    bool importFromFile(const QString &srcPath, QString *summary, QString *error,
                        int *importedPrograms = nullptr,
                        int *newFolders = nullptr,
                        int *existingFolders = nullptr);

signals:
    void scopesChanged();
    void importFinished();

private:
    QVariantList loadScopes() const;

    QVariantList m_scopes;
};

#endif // USERPROGTRANSFERCONTROLLER_H
