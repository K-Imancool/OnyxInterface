#ifndef PROGLOADER_H
#define PROGLOADER_H

// #include <array>

#include "BackEnd/progloaderbase.h"
#include "instrument.h"
#include "databasereader.h"
#include "socketmodel.h"
#include "Structures.h"

#include <QObject>
#include <QPointer>
#include <map>
// #include "userprogsloadmodel.h"
// bi1CutInstr = "0", bi1CutMode = "1000", bi1CutPower = "1";



class ProgLoader : public QObject
{
	Q_OBJECT
public:

	enum progType {
		ptRecom = 0,
		ptUser,
		ptService,
		ptCount
	};

	explicit ProgLoader(QObject *parent = nullptr);
	/**
	 * @brief Сохраняет текущее состояние всех сокетов в БД (таблица Lists, id=1000)
	 */
	
public slots:
	void slotSaveCurrentState();
	
signals:
	
public:	
	void defaultSocketInit(bool clear = true);
	// void removeSubProg(int index);
	/**
	 * @brief ControlCenter::programmLoadSocketInit
	 * @param progId
	 * @details я тут с ума сойду - вся эта функция за раз в голове не помещается
	 * Шаг 1 - получить строки таблицы Lists, с соответствующими id (каждая строка - 1 рабочий экран)
	 * Шаг 2 - поличить строки таблицы EnabledMods, с соответствующими id
	 *         (каждая строка - 1 разрещённый режим)
	 * Шаг 3 - поличить строки таблицы EnabledInstr, с соответствующими id
	 *         (каждая строка - 1 разрещённый инструмент)
	 * Шаг 4 - из таблицы Instrum получить список допустимых инструментов
	 *         для каждого режима в списке режимов (Шаг 2)
	 * Шаг 5 - Проредить полученный список инструментов оставив в нём только
	 *         те, которые разрешены в данной программе (Шаг 3)
	 * Шаг 6 - Поселедовательная инициализация полусокетов по строкам из (Шаг 1)
	 *         Если сокет включён:
	 *         6.1 - для каждого полусокета прореживание списка режимов
	 *         6.2 - для каждого полусокета прореживания списка инструментов
	 *         6.3 инициализация сокета полученным списком допустимыз режимов и инструментов
	 *         6.4 установка режима, мощностии и инструмента по умолчанию
	 */
	bool programmLoadSocketInit(int progId, bool clear = true);
	bool freeSettingsSocketInit(bool clear = true);
	
	/**
	 * @brief getCategories получение списка доспуных программ в категории
	 * @param scopeID
	 * @return
	 */
	std::map<int, QString> getProgs(int scopeID);
	
	std::map<int, QString> getCategories();
	
	void saveUserProg(const QString& scopeName,
	                    const QString& progName);
	

	///формально ничего не мешает удалить/переименовать рекоменд прогу
	void deleteUserProg(int id);
	void deleteAllUserProgs();

	void renameUserProg(int id, const QString& name);

	void deleteUserScope(int id);

	void renameUserScope(int id, const QString& name);
	
	/**
	 * @brief Загружает последнее сохранённое состояние из БД
	 */
	bool loadCurrentState();
	
	std::map<int, std::map<int, Onyx::InstrInfo>> getConstraints(const std::vector<int> &idList);
	
	void setSocketModelPtr(QSharedPointer<SocketModel> newSocketModelPtr);

	int addUserScope(const QString& name);
	
	void setCurLoaderType(progType newCurLoaderType);

private:
	QList<QStringList> prepSaveState();
	std::map<int, InstrPtr> getInstrums();
	void saveProg(const QString& name = "");
	ProgLoaderBase* getLoader(progType type);
	std::vector<int> getAllowedInstrs(int progId, const QVariantList& progItem);
	std::vector<int> getAllowedModes(int progId, const QVariantList &progItem);
	void fillHalfSocket(int halfSocket,
	                    int socketNumber,
	                    SockPtr socket,
	                    const QVariantList& progItem,
	                    const QStringList& modeNamesList,
	                    const std::vector<int>& allowedModesId,
	                    const std::map<int, std::map<int, Onyx::InstrInfo>>& instrumConstraints);

	QSharedPointer<DataBaseReader> m_dbReaderPtr;
	QSharedPointer<SocketModel> m_socketModelPtr;
	progType m_curLoaderType = ptRecom;	

	// std::map<int, QSharedPointer<ProgLoaderBase>> m_loaders;
	// QPointer<ProgLoaderBase> loader = nullptr;
	progType m_curLoaderType = ptRecom;
	int m_currentLoadedProgId = -1;
	QString m_currentLoadedProgName;
	int m_currentLoadedScopeId = -1;

	ProgLoaderBase* getLoader(progType type);
};

#endif // PROGLOADER_H
