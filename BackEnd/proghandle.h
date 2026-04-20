#ifndef PROGHANDLE_H
#define PROGHANDLE_H

#include <QObject>
#include <QVariant>
#include <QVariantMap>


class ProgHandle : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(QStringList scopeNameList READ scopeNameList NOTIFY scopeNameListChanged FINAL)
    Q_PROPERTY(QList<int> scopeIdList READ scopeIdList NOTIFY scopeNameListChanged FINAL)
    Q_PROPERTY(QStringList progNameList READ progNameList NOTIFY progNameListChanged FINAL)
    Q_PROPERTY(QList<int> progIdList READ progIdList NOTIFY progNameListChanged FINAL)
    Q_PROPERTY(int scopeIdx READ scopeIdx WRITE setScopeIdx NOTIFY scopeIdxChanged FINAL)

    explicit ProgHandle(QObject *parent = nullptr);

    Q_INVOKABLE void loadSelected();
    Q_INVOKABLE void loadRecommendedProg(int recomProgId, bool clear = true);
    Q_INVOKABLE void removeSubProg(int idx);
    Q_INVOKABLE void loadUserProg(int recomProgId);
    Q_INVOKABLE void loadEmptyProg();
    Q_INVOKABLE void permitAll();
    Q_INVOKABLE void saveProg(int id, const QString& name);

    QStringList scopeNameList() const;
    QStringList progNameList() const;
    QList<int> scopeIdList() const;
    QList<int> progIdList() const;

    int scopeIdx() const;
    void setScopeIdx(int newScopeIdx);

    void setProgList(QMap<int, QString> lst);

    void setScopeNameList(QMap<int, QString> scopes);

signals:
    void signalLoadRecommend(int scopeIdx, int progIdx, int subProgIdx);

    void signalRecomProgChosen(int progId, bool clear);

    void signalRemoveSub(int toRemove);
    //может быть избыточно и проги сможем просто по id разделять
    void signalUserProgChosen(int progId);

    void signalLoadEmpty();
    void signalSave(int id, const QString& name);
    void signalUnlockProg();

    void signalScopeRequest(int scopeId);

    void currentModeIndexChanged();
    void scopeNameListChanged();
    void progNameListChanged();

    void scopeIdxChanged();

private:
    int m_scopeIdx = 0;
    QMap<int, QString> m_scopes;
    QMap<int, QString> m_progs;
};


#endif // PROGHANDLE_H
