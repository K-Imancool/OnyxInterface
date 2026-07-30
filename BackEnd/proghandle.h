#ifndef PROGHANDLE_H
#define PROGHANDLE_H

#include <QList>
#include <QObject>
#include <QPair>
#include <QVariant>
#include <QVariantMap>

// #include "userprogsloadmodel.h"

class ProgHandle : public QObject
{
	Q_OBJECT
	
public:
	Q_PROPERTY(QStringList scopeNameList READ scopeNameList NOTIFY scopeNameListChanged FINAL)
	Q_PROPERTY(QList<int> scopeIdList READ scopeIdList NOTIFY scopeNameListChanged FINAL)
	Q_PROPERTY(QStringList progNameList READ progNameList NOTIFY progNameListChanged FINAL)
	Q_PROPERTY(QList<int> progIdList READ progIdList NOTIFY progNameListChanged FINAL)
	// Q_PROPERTY(QStringList userProgList READ userProgList NOTIFY userProgListChanged FINAL)
	Q_PROPERTY(int scopeIdx READ scopeIdx WRITE setScopeIdx NOTIFY scopeIdxChanged FINAL)
	Q_PROPERTY(bool isRecomProgs READ isRecomProgs WRITE setIsRecomProgs NOTIFY isRecomProgsChanged FINAL)
	
	explicit ProgHandle(QObject *parent = nullptr);
	
	Q_INVOKABLE void loadRecommendedProg(int recomProgId, bool clear = true);
    Q_INVOKABLE void loadFreeSettings();
    Q_INVOKABLE void loadEmptyFreeSettings();
    Q_INVOKABLE void loadLastSettings();
    Q_INVOKABLE void saveCurrentState();
	Q_INVOKABLE void removeSubProg();
	Q_INVOKABLE void permitAll();
	Q_INVOKABLE void saveProg(const QString& scopeName,
	                          const QString& progName);
	Q_INVOKABLE bool userProgExists(const QString& scopeName,
	                                const QString& progName);
	Q_INVOKABLE void addEmptyDefault();
	Q_INVOKABLE void copyCurrent();
    Q_INVOKABLE void deleteAllUserProgs();
	Q_INVOKABLE void deleteProgRequest(int index);
	Q_INVOKABLE void renameProgRequest(int index, const QString& name);

	Q_INVOKABLE void deleteScopeRequest(int index);
	Q_INVOKABLE void renameScopeRequest(int index, const QString& name);
	Q_INVOKABLE void addScopeRequest(const QString& name);
	Q_INVOKABLE bool hasSubPrograms(int index) const;
	Q_INVOKABLE QVariantList subProgramsAt(int index) const;

	Q_INVOKABLE QString readTextFile(const QString& filePath);
	
	QStringList scopeNameList() const;
	QStringList progNameList() const;
	QList<int> scopeIdList() const;
	QList<int> progIdList() const;
	
	int scopeIdx() const;
	void setScopeIdx(int newScopeIdx);
	
	void setProgList(const std::map<int, QString>& lst/*, bool isRecom = true*/);
	void setProgSubLists(const QList<QList<QPair<int, QString>>>& subLists);
	void setScopeList(const std::map<int, QString>& scopes/*, bool isRecom = true*/);
	
	bool isRecomProgs() const;
	void setIsRecomProgs(bool newIsRecomProgs);

public slots:
	void notifyEndoProgramMixRejected();

signals:
	//все этим методы и сигналы нужны т.к. возможно хочется сделать модель ридонли внутри qml
	// а редачить только через хендлеры для разграничения доступа
	
	void signalCopyCurrent();
	void signalAddEmptyDefault(bool clearLoad = true);
	void signalLoadRecommend(int scopeIdx, int progIdx, int subProgIdx);
	void signalRecomProgChosen(int progId, bool clear);
    void signalFreeSettingsRequested();
    void signalEmptyFreeSettingsRequested();
    void signalLastSettingsRequested();
    void signalSaveCurrentStateRequested();
    void signalDeleteAllUserProgs();
	void signalRemoveSub();
	//может быть избыточно и проги сможем просто по id разделять
	void signalUserProgChosen(int progId);
	void signalDeleteProg(int progId);
	void signalRenameProg(int progId, const QString& name);
	void signalDeleteScope(int scopeId);
	void signalRenameScope(int scopeId, const QString& name);
	void signalAddScope(const QString& name);
	
	void signalLoadEmpty();
	void signalSave(int id, const QString& name);
	void signalSaveName(const QString& scopeName,
	                    const QString& progName);
	void signalUnlockProg();
	
	void signalScopeRequest(int scopeId);
	
	void updateScopes(bool isRecom);
	
	void currentModeIndexChanged();
	void scopeNameListChanged();
	void progNameListChanged();
	
	void scopeIdxChanged();
	
	void isRecomProgsChanged();

	void endoProgramMixRejected();

	void userProgExistsRequested(const QString& scopeName,
	                             const QString& progName,
	                             bool* result);

private:
	int m_scopeIdx = 0;
	std::map<int, QString> m_scopes;
	std::map<int, QString> m_progs;
	QList<QList<QPair<int, QString>>> m_progSubLists;
	bool m_isRecomProgs;
};


#endif // PROGHANDLE_H
