#include "proghandle.h"


ProgHandle::ProgHandle(QObject *parent)
    : QObject{parent}
{

}

void ProgHandle::loadSelected()
{

}

void ProgHandle::loadRecommendedProg(int recomProgIdx, bool clear)
{
    if (recomProgIdx >= m_progs.size())
        return;
    auto iter = m_progs.begin();
    iter += recomProgIdx;
    emit signalRecomProgChosen(iter.key(), clear);
}

void ProgHandle::removeSubProg(int idx)
{

    emit signalRemoveSub(idx);
}

void ProgHandle::loadUserProg(int recomProgId)
{
    emit signalUserProgChosen(recomProgId);
}

void ProgHandle::loadEmptyProg()
{
    emit signalLoadEmpty();
}

void ProgHandle::saveProg(int id, const QString &name)
{
    emit signalSave(id, name);
}

void ProgHandle::permitAll()
{
    emit signalUnlockProg();
}

QStringList ProgHandle::scopeNameList() const
{
    return m_scopes.values();
}

QStringList ProgHandle::progNameList() const
{
    return m_progs.values();
}

QList<int> ProgHandle::scopeIdList() const
{
    return m_scopes.keys();
}

QList<int> ProgHandle::progIdList() const
{
    return m_progs.keys();
}

int ProgHandle::scopeIdx() const
{
    return m_scopeIdx;
}

void ProgHandle::setScopeIdx(int newScopeIdx)
{
    if (m_scopes.size() <= newScopeIdx)
        return;
    if (m_scopeIdx == newScopeIdx)
        return;
    m_scopeIdx = newScopeIdx;
    auto iter = m_scopes.begin();
    iter += newScopeIdx;
    emit signalScopeRequest(iter.key());
    emit scopeIdxChanged();
}


void ProgHandle::setProgList(QMap<int, QString> lst)
{
    m_progs = lst;
    emit progNameListChanged();
}

void ProgHandle::setScopeNameList(QMap<int, QString> scopes)
{
    m_scopes = scopes;

    // m_scopeNameList = m_scopes.values();
    setScopeIdx(0);
    emit scopeNameList();
}
