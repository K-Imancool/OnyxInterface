#include "progloader.h"
#include "BackEnd/dblocale.h"
#include "BackEnd/recomprogloader.h"
#include "BackEnd/userprogloader.h"
#include "BackEnd/jsonstorage.h"
#include "BackEnd/featureunlockcontroller.h"
#include "socket.h"
#include "onyxapp.h"
#include "EshfProgStringBuilder.h"

#include <algorithm>
#include <unordered_set>
#include <set>
#include <vector>
#include <functional>
#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <cstdlib>  // для abs()


namespace {

template <typename T>
void filterMapByKey(std::map<int, T>& map, const std::vector<int>& keys_to_keep) {
	// Создаем временный набор для быстрого поиска
	std::unordered_set<int> keep_set(keys_to_keep.begin(), keys_to_keep.end());

	// Используем идиому erase-remove для map
	auto it = map.begin();
	while (it != map.end()) {
		if (keep_set.find(it->first) == keep_set.end()) {
			it = map.erase(it);  // Удаляем элемент, если его ключа нет в векторе
		} else {
			++it;
		}
	}
}

std::map<int, SockPtr> cloneSocketMap(const std::map<int, SockPtr>& src)
{
	std::map<int, SockPtr> dst;
	for (const auto& entry : src) {
		if (!entry.second.isNull()) {
			dst.emplace(entry.first, SockPtr::create(*entry.second));
		}
	}
	return dst;
}

std::map<int, InstrPtr> cloneInstrMap(const std::map<int, InstrPtr>& src)
{
	std::map<int, InstrPtr> dst;
	for (const auto& entry : src) {
		if (!entry.second.isNull()) {
			dst.emplace(entry.first, InstrPtr::create(*entry.second));
		}
	}
	return dst;
}

using namespace Onyx;

bool executePreparedUpdate(const QString& connectionName,
                           const QString& sql,
                           const std::function<void(QSqlQuery&)>& bindValues,
                           const char* operationName)
{
	QSqlDatabase db = QSqlDatabase::database(connectionName);
	if (!db.open()) {
		qWarning() << operationName << "failed: cannot open database";
		return false;
	}

	QSqlQuery query(db);
	if (!query.prepare(sql)) {
		qWarning() << operationName << "prepare failed:" << query.lastError().text();
		qWarning() << "SQL was:" << sql;
		return false;
	}

	bindValues(query);

	if (!query.exec()) {
		qWarning() << operationName << "exec failed:" << query.lastError().text();
		qWarning() << "SQL was:" << sql;
		return false;
	}

	return true;
}

std::vector<int> parseCommaSeparatedNumbers(const QString& input) {
	std::vector<int> result;

	if (input.isEmpty()) {
		return result;  // Возвращаем пустой вектор, если строка пуста
	}

	// Разделяем строку по запятым
	QStringList parts = input.split(',', Qt::SkipEmptyParts);

	// Преобразуем каждую часть в число и добавляем в вектор
	for (const QString& part : parts) {
		bool ok = false;
		int number = part.trimmed().toInt(&ok);  // Удаляем пробелы и конвертируем

		if (ok) {
			result.push_back(number);
		}
		// Можно добавить обработку ошибок, если нужно
	}

	return result;
}

QString makeCommaSeparatedNumbers(const std::vector<int>& list) {
	QString res;
	for (int a : list) {
		res += QString("%1,").arg(a);
	}
	if (res.size() != 0)
		res = res.left(res.size()-1);
	return res;
}

/** Pedal_1 / Pedal_2 в Lists: 0 — педаль не назначена; 1..4 — номер сокета (выход). */
int listsPedalColumnToSocketIndex(int v)
{
	if (v <= 0 || v > 4) {
		return -1;
	}
	return v - 1;
}

QString makeSocketName(Onyx::SocType type) {
	// Перевод применяется при (пере)загрузке программы: при смене языка
	// модель сокетов пересобирается, поэтому имена обновляются
	QString socketName = "";
	switch (type) {
	case Onyx::EMPTY:
		socketName = QString("EMPTY");
		break;
	case Onyx::BIPOLAR_1:
		socketName = QCoreApplication::translate("SocketNames", "БИ1");
		break;
	case Onyx::BIPOLAR_2:
		socketName = QCoreApplication::translate("SocketNames", "БИ2");
		break;
	case Onyx::MONOPOLAR_1:
		socketName = QCoreApplication::translate("SocketNames", "МОНО1");
		break;
	case Onyx::MONOPOLAR_2:
		socketName = QCoreApplication::translate("SocketNames", "МОНО2");
		break;
	}
	return socketName;
}

void makeModes(QMap<int, SurgModePtr>& container,
               const QList<QVariantList>& modes,
               const std::map<int, std::map<int, InstrInfo>>& instMap,
               const QVariantList& progItem,
               int socketNum,
               bool isCoag,
               const std::vector<int>& instrFilter = std::vector<int>(),
               bool skipInstrFilter = false) {
	int start = isCoag ? 6 : 3;

	container.insert(1000, SurgModePtr::create(QCoreApplication::translate("Modes", "НЕ ВЫБРАН"),
                                                                   isCoag,
	                                           1,
	                                           1,
	                                           1000,
	                                           std::map<int, InstrInfo>(),
	                                           1000,
	                                           "",
	                                           "",
	                                           false,
	                                           false));  // Num = 1000, Brief = "", Descript = "", isEndo = false, isArgon = false


	for (const auto& item : modes) {
		int modeId = item.at(2).toInt();
		QString modeName = item.at(1).toString();
		int modeNum = item.size() > 3 ? item.at(3).toInt() : 0;  // Num для изображения
		QString modeBrief = item.size() > 4 ? item.at(4).toString() : "";  // Brief_RU для краткого описания
		QString modeDescript = item.size() > 5 ? item.at(5).toString() : "";  // Descript_RU для полного описания

		auto instrs = instMap.find(modeId);
		if (instrs == instMap.end()) {
			continue;
		}

		std::map<int, InstrInfo> tmp = instrs->second;

        // Применяем фильтр только если skipInstrFilter = false
        if (!skipInstrFilter) {
            if (!instrFilter.empty()) {
                filterMapByKey(tmp, instrFilter);
            } else {
                filterMapByKey(tmp, parseCommaSeparatedNumbers(progItem.at(start + 6*socketNum).toString()));
            }
        }

		bool isEndo = item.size() > 6 ? item.at(6).toBool() : false;  // ENDO_REG
		bool isArgon = item.size() > 8 ? item.at(8).toInt() == 1 : false;  // Argon

		SurgModePtr ptr = SurgModePtr::create(modeName,
		                                      isCoag,
		                                      item.at(0).toInt(),
		                                      1,
		                                      modeId,
		                                      tmp,
		                                      modeNum,
		                                      modeBrief,
		                                      modeDescript,
		                                      isEndo,
		                                      isArgon);  // Brief, Descript, isEndo, isArgon
		container.insert(modeId, ptr);
	}
}

bool hasNonZeroDigit(int number, int digitPosition) {
	// Проверяем, что позиция разряда корректна (начиная с 0)
	if (digitPosition < 0) {
		return false;
	}

	// Приводим число к положительному виду для упрощения
	number = abs(number);

	// Вычисляем 10^digitPosition целочисленным способом (без pow)
	int powerOf10 = 1;
	for (int i = 0; i < digitPosition; ++i) {
		powerOf10 *= 10;
	}

	// Делим число на 10^digitPosition, чтобы перенести нужный разряд в конец
	int shiftedNumber = number / powerOf10;

	// Если после сдвига число стало нулём, значит разряд отсутствует
	if (shiftedNumber == 0) {
		return false;
	}

	// Получаем последнюю цифру (нужный разряд)
	int digit = shiftedNumber % 10;

	// Проверяем, что цифра не равна нулю
	return (digit != 0);
}

void filterModeMap(QMap<int, SurgModePtr>& container, const std::vector<int>& allow) {
	auto iter = container.begin();
	while (iter != container.end()) {
		bool contains = iter.key() == 1000;
		// Аргоновые режимы доступны на обоих МОНО-выходах, даже если в Lists
		// они указаны только для МОНО1.
		if (!contains && !iter.value().isNull() && iter.value()->isArgon()) {
			contains = true;
		}
		for (int a : allow) {
			if (iter.key() == a) {
				contains = true;
				break;
			}
		}
		if (contains) {
			++iter;
		} else {
			iter = container.erase(iter);
		}
	}
}

QString makeDbStringMode( const QModelIndex& idx,
                                int listRole,
                                int firstItemRole) {
    QStringList list = idx.data(listRole).toStringList();
    int firstItem = idx.data(firstItemRole).toInt();
    list.removeOne(QString::number(firstItem));
    list.removeOne(QString::number(1000));
    list.prepend(QString::number(firstItem));
    return list.join(',');
}

QString makeDbStringInstr( const QModelIndex& idx,
                          int listRole,
                          int firstItemRole) {
    QStringList list = idx.data(listRole).toStringList();
    int firstItem = idx.data(firstItemRole).toInt();
    list.removeOne(QString::number(firstItem));
    list.prepend(QString::number(firstItem));
    return list.join(',');
}

const QString queryConditionRecom = "Prog_ID = %1 AND Num >= 0 AND Num < 4 ORDER BY Num ASC, id ASC";
const QStringList fields = {"id", "Num", "Prog_ID", /* 0 1 2*/
                            "Bi1Cut_INSTR", "Bi1Cut_MODE", "Bi1Cut_POWER", /* 3 4 5*/
                            "Bi1Coag_INSTR", "Bi1Coag_MODE", "Bi1Coag_POWER", /* 6 7 8*/
                            "Bi2Cut_INSTR", "Bi2Cut_MODE", "Bi2Cut_POWER", /* 9 10 11*/
                            "Bi2Coag_INSTR", "Bi2Coag_MODE", "Bi2Coag_POWER", /* 12 13 14*/
                            "Mono1Cut_INSTR", "Mono1Cut_MODE", "Mono1Cut_POWER", /* 15 16 17*/
                            "Mono1Coag_INSTR", "Mono1Coag_MODE", "Mono1Coag_POWER", /* 18 19 20*/
                            "Mono2Cut_INSTR", "Mono2Cut_MODE", "Mono2Cut_POWER", /* 21 22 23*/
                            "Mono2Coag_INSTR", "Mono2Coag_MODE", "Mono2Coag_POWER", /* 24 25 26*/
                            "Pedal_1", "Pedal_2", "OutEnabled_MASK"};/* 27 28 29 30*/
const   QString saveCurQuery = QString(
                                       "REPLACE INTO Lists ("
                                       "id, Num, Prog_ID, "
                                       "Bi1Cut_INSTR, Bi1Cut_MODE, Bi1Cut_POWER, "
                                       "Bi1Coag_INSTR, Bi1Coag_MODE, Bi1Coag_POWER, "
                                       "Bi2Cut_INSTR, Bi2Cut_MODE, Bi2Cut_POWER, "
                                       "Bi2Coag_INSTR, Bi2Coag_MODE, Bi2Coag_POWER, "
                                       "Mono1Cut_INSTR, Mono1Cut_MODE, Mono1Cut_POWER, "
                                       "Mono1Coag_INSTR, Mono1Coag_MODE, Mono1Coag_POWER, "
                                       "Mono2Cut_INSTR, Mono2Cut_MODE, Mono2Cut_POWER, "
                                       "Mono2Coag_INSTR, Mono2Coag_MODE, Mono2Coag_POWER, "
                                       "Pedal_1, Pedal_2, OutEnabled_MASK"
                                       ") VALUES ("
                                       "1000, %28, 1000, "
                                       "'%1', '%2', %3, "
                                       "'%4', '%5', %6, "
                                       "'%7', '%8', %9, "
                                       "'%10', '%11', %12, "
                                       "'%13', '%14', %15, "
                                       "'%16', '%17', %18, "
                                       "'%19', '%20', %21, "
                                       "'%22', '%23', %24, "
                                       "%25, %26, %27"
                                       ")");
const   QString insertCurQuery = QString(
                                     "INSERT INTO Lists ("
                                     "Num, Prog_ID, "
                                     "Bi1Cut_INSTR, Bi1Cut_MODE, Bi1Cut_POWER, "
                                     "Bi1Coag_INSTR, Bi1Coag_MODE, Bi1Coag_POWER, "
                                     "Bi2Cut_INSTR, Bi2Cut_MODE, Bi2Cut_POWER, "
                                     "Bi2Coag_INSTR, Bi2Coag_MODE, Bi2Coag_POWER, "
                                     "Mono1Cut_INSTR, Mono1Cut_MODE, Mono1Cut_POWER, "
                                     "Mono1Coag_INSTR, Mono1Coag_MODE, Mono1Coag_POWER, "
                                     "Mono2Cut_INSTR, Mono2Cut_MODE, Mono2Cut_POWER, "
                                     "Mono2Coag_INSTR, Mono2Coag_MODE, Mono2Coag_POWER, "
                                     "Pedal_1, Pedal_2, OutEnabled_MASK"
                                     ") VALUES ("
                                     "%28, 1000,"
                                     "'%1', '%2', %3, "
                                     "'%4', '%5', %6, "
                                     "'%7', '%8', %9, "
                                     "'%10', '%11', %12, "
                                     "'%13', '%14', %15, "
                                     "'%16', '%17', %18, "
                                     "'%19', '%20', %21, "
                                     "'%22', '%23', %24, "
                                     "%25, %26, %27"
                                     ")");
const   QString insertUserProgQuery = QString(
                                      "INSERT INTO Lists ("
                                      "Num, Prog_ID,"
                                      "Bi1Cut_INSTR, Bi1Cut_MODE, Bi1Cut_POWER, "
                                      "Bi1Coag_INSTR, Bi1Coag_MODE, Bi1Coag_POWER, "
                                      "Bi2Cut_INSTR, Bi2Cut_MODE, Bi2Cut_POWER, "
                                      "Bi2Coag_INSTR, Bi2Coag_MODE, Bi2Coag_POWER, "
                                      "Mono1Cut_INSTR, Mono1Cut_MODE, Mono1Cut_POWER, "
                                      "Mono1Coag_INSTR, Mono1Coag_MODE, Mono1Coag_POWER, "
                                      "Mono2Cut_INSTR, Mono2Cut_MODE, Mono2Cut_POWER, "
                                      "Mono2Coag_INSTR, Mono2Coag_MODE, Mono2Coag_POWER, "
                                      "Pedal_1, Pedal_2, OutEnabled_MASK"
                                      ") VALUES ("
                                      "%28, %29,"
                                      "'%1', '%2', %3, "
                                      "'%4', '%5', %6, "
                                      "'%7', '%8', %9, "
                                      "'%10', '%11', %12, "
                                      "'%13', '%14', %15, "
                                      "'%16', '%17', %18, "
                                      "'%19', '%20', %21, "
                                      "'%22', '%23', %24, "
                                      "%25, %26, %27"
                                      ")");
const QString queryConditionModes = "BI_MONO = %1 AND CUT_COAG = %2 AND id IN (%3)";

QList<QVariantList> filterModesByFeatureKey(const QList<QVariantList> &modes,
                                            FeatureUnlockController *featureUnlock)
{
	if (!featureUnlock) {
		return modes;
	}

	QList<QVariantList> filtered;
	filtered.reserve(modes.size());
	for (const QVariantList &item : modes) {
		const QString keyField = item.size() > 7 ? item.at(7).toString() : QString();
		if (featureUnlock->isKeyRequirementMet(keyField)) {
			filtered.append(item);
		}
	}
	return filtered;
}

}

ProgLoader::ProgLoader(QObject *parent)
    : QObject{parent}
{
	if (m_dbReaderPtr.isNull()) {
		OnyxApp* app = dynamic_cast<OnyxApp*>(qApp);
		if (app) {
			m_dbReaderPtr = app->getDbReader();
			m_userDbReaderPtr = app->getUserProgDbReader();
		}
	}
}

QSharedPointer<DataBaseReader> ProgLoader::listsDbForProg(int progId) const
{
	if (progId == 1000 || progId > 1000) {
		return m_userDbReaderPtr;
	}
	return m_dbReaderPtr;
}

QSharedPointer<DataBaseReader> ProgLoader::progsDbForProg(int progId) const
{
	if (progId > 1000) {
		return m_userDbReaderPtr;
	}
	return m_dbReaderPtr;
}

void ProgLoader::slotSaveCurrentState()
{
	if (m_socketModelPtr == nullptr
	        || m_socketModelPtr->itemsMap() == nullptr
	        || m_userDbReaderPtr.isNull()) {
		// qWarning() << "Cannot save state: socket model not initialized";
		return;
	}

	int pageCount = m_socketModelPtr->subProgCount();
	QList<QStringList> listsStr = prepSaveState();

	if (!m_userDbReaderPtr->beginTransaction()) {
		qWarning() << "slotSaveCurrentState: failed to begin transaction";
		return;
	}

	// Формируем SQL запрос для REPLACE (INSERT OR UPDATE)
	QString removeQuerry = "DELETE FROM Lists WHERE Prog_ID = 1000";
	if (!m_userDbReaderPtr->executeUpdateQuery(removeQuerry)) {
		m_userDbReaderPtr->rollback();
		qWarning() << "slotSaveCurrentState: failed to clear previous state";
		return;
	}

	for (int i = 0; i < pageCount; ++i) {
		const auto& curList = listsStr.at(i);

		QString query = insertCurQuery
		                // QString query = saveCurQuery
		                .arg(curList.at(0)).arg(curList.at(1)).arg(curList.at(2)) //bi1 cut
		                .arg(curList.at(3)).arg(curList.at(4)).arg(curList.at(5)) //bi1 coag
		                .arg(curList.at(6)).arg(curList.at(7)).arg(curList.at(8)) //bi2 cut
		                .arg(curList.at(9)).arg(curList.at(10)).arg(curList.at(11)) //bi2 coag
		                .arg(curList.at(12)).arg(curList.at(13)).arg(curList.at(14)) //mono1 cut
		                .arg(curList.at(15)).arg(curList.at(16)).arg(curList.at(17)) //mono1 coag
		                .arg(curList.at(18)).arg(curList.at(19)).arg(curList.at(20)) //mono2 cut
		                .arg(curList.at(21)).arg(curList.at(22)).arg(curList.at(23)) //mono2 coag
		                .arg(curList.at(24)).arg(curList.at(25)).arg(curList.at(26)) //pedals and outmask
		                .arg(i);

		if (!m_userDbReaderPtr->executeUpdateQuery(query)) {
			m_userDbReaderPtr->rollback();
			qWarning() << "slotSaveCurrentState: failed to insert page" << i;
			return;
		}
	}

	m_userDbReaderPtr->commit();
}

void ProgLoader::defaultSocketInit(bool clear)
{
	std::vector<std::map<int, SockPtr>> socketMapVector;
	std::vector<std::map<int, InstrPtr >> instrMapVector;
	std::map<int, std::map<int, InstrInfo>>  instrumConstraints;
	QList<QVariantList> progListVariant;
	std::vector<int> allowedModesId;
	std::vector<int> allowedInstrId;

	// Создаём пустую запись с дефолтными значениями
	QVariantList dummyProgItem;
	dummyProgItem << 0 << 0 << 0;  // id, Num, Prog_ID

	// Добавляем пустые строки для всех полей инструментов/режимов/мощностей (24 поля)
	for (int i = 0; i < 24; ++i) {
		dummyProgItem << "";
	}

	// Добавляем дефолтные значения для педалей и маски
	dummyProgItem << 0 << 0 << 255;  // Pedal_1, Pedal_2, OutEnabled_MASK (все сокеты разрешены)

	progListVariant.append(dummyProgItem);
	// qDebug() << "progId = 0: Created dummy program item with" << dummyProgItem.size() << "fields";
	// Получаем все режимы из БД
	QList<QVariantList> allModes = m_dbReaderPtr->slotSendSelectQuery(
	                                   QStringList{"Modes"},
	                                   QStringList{"id", "KEY"},
	                                   QString("1=1%1").arg(deviceModeFilterCondition()));
	allowedModesId.reserve(allModes.size());
	for (const auto& item : allModes) {
		const QString keyField = item.size() > 1 ? item.at(1).toString() : QString();
		if (m_featureUnlock && !m_featureUnlock->isKeyRequirementMet(keyField)) {
			continue;
		}
		allowedModesId.push_back(item.at(0).toInt());
	}

	// Получаем все инструменты из БД
	QList<QVariantList> allInstr = m_dbReaderPtr->slotSendSelectQuery(
	                                   QStringList{"Instruments"},
	                                   QStringList{"id"},
	                                   "");
	for (const auto& item : allInstr)
		allowedInstrId.push_back(item.at(0).toInt());

	// qDebug() << "progId = 0: Loading ALL modes (" << allowedModesId.size() << ") and instruments (" << allowedInstrId.size() << ")";

	//Шаг4---------------------------------------------------------
	QString queryConditionModes = "BI_MONO = %1 AND CUT_COAG = %2 AND id IN (%3)";
	instrumConstraints = getConstraints(allowedModesId);

	//Шаг 6--------------------------------------------------------
	QList<QVariantList> modeNamesListV = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Modes"},
	                                                                        QStringList{DbLocale::column("Name"),"id"},
	                                                                        "");
	QStringList modeNamesList;
	modeNamesList.reserve(modeNamesListV.size());
	for (const auto& iter : modeNamesListV) {
		modeNamesList.append(iter.at(0).toString());
	}

	for (const auto& progItem : progListVariant) {
		socketMapVector.push_back(std::map<int, SockPtr>());
		std::map<int, SockPtr>& socketMap = socketMapVector[socketMapVector.size() - 1];
		for (int i = 0; i < 4; ++i) {
			Onyx::SocType type = Onyx::SocType(i+1);
			SockPtr socket = SockPtr::create(type);
			socketMap[i] = socket;

			socket->setSocketName(makeSocketName(type));

			for (int halfSocket = 0; halfSocket < 2; ++halfSocket ) {
				bool isCoag = (halfSocket == 0);
				QMap<int, SurgModePtr> modes;
				QList<QVariantList> modesList = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Modes"},
				                                                                   QStringList{"MaxPower",DbLocale::column("Name"), "id", "Num", DbLocale::column("Brief"), DbLocale::column("Descript"), "ENDO_REG", "KEY", "Argon"},
				                                                                   queryConditionModes
				                                                                   .arg(socket->socketType() <= Onyx::BIPOLAR_2 ? 0 : 1)
				                                                                   .arg(halfSocket)
				                                                                   .arg(makeCommaSeparatedNumbers(allowedModesId))
				                                                                   + deviceModeFilterCondition());
				modesList = filterModesByFeatureKey(modesList, m_featureUnlock);

				// Сортируем по Num (index 3)
				std::sort(modesList.begin(), modesList.end(),
				          [](const QVariantList& a, const QVariantList& b) {
					return a.at(3).toInt() < b.at(3).toInt();
				});

				std::vector<int> instIdLst;
				std::vector<int> modeIdLst;

				// Используем все инструменты и режимы
				instIdLst = allowedInstrId;
				for (const auto& mode : modesList) {
					modeIdLst.push_back(mode.at(2).toInt());
				}

				makeModes(modes,
				          modesList,
				          instrumConstraints,
				          progItem,
				          i,
				          isCoag,
				          instIdLst);  // Передаём фильтр инструментов

				// Исключаем режим "Термошов" (ID=7) для сокета БИ2 (i=0)
				if (i == 0 && modes.contains(7)) {
					modes.remove(7);
				}

				isCoag ? socket->setCoagModes(modes, modeNamesList)
				       : socket->setCutModes(modes, modeNamesList);

				int firstInstrId;
				int firstModeId;
				int defaultPower;

				firstInstrId = 0;
				firstModeId = 1000;
				defaultPower = 1;

				socket->setModeId(firstModeId, isCoag);
				socket->setInstrumId(firstInstrId, isCoag);

				// Проверка для эндоскопических режимов: если мощность = 1, устанавливаем 11
				auto mode = isCoag ? socket->curCoagMode() : socket->curCutMode();
				if (!mode.isNull() && mode->isEndo()) {
					int endoCut = defaultPower / 10;  // Целочисленное деление вместо floor
					int endoCoag = defaultPower % 10;
					if (endoCut < 1) endoCut = 1;
					else if (endoCut > ENDO_MAX) endoCut = ENDO_MAX;
					if (endoCoag < 1) endoCoag = 1;
					else if (endoCoag > ENDO_MAX) endoCoag = ENDO_MAX;
					defaultPower = endoCut * 10 + endoCoag;
				}

				isCoag ? socket->setCoagModePower(defaultPower)
				        : socket->setCutModePower(defaultPower);
			}
			//МОНО2 КОАГ = 1, БИ1РЕЗ 8
			bool coagEna = hasNonZeroDigit(progItem.at(29).toInt(), (8 - 2*i) - 1 );
			bool cutEna = hasNonZeroDigit(progItem.at(29).toInt(), (8 - 2*i) );
			bool allowSock = cutEna || coagEna;
			socket->setAllowed(allowSock);
			socket->setDisplayMode(Onyx::S_COLLAPSED);
		}
		instrMapVector.push_back(getInstrums());
	}
	m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, !clear);
    if (!m_socketModelPtr.isNull()) {
        m_socketModelPtr->setEndoProgramView(false);
    }
}

bool ProgLoader::programmLoadSocketInit(int progId, bool clear)
{
    if (m_dbReaderPtr.isNull()) {
//        qWarning() << "[ProgFlow] programmLoadSocketInit: нет m_dbReaderPtr";
        return false;
    }
    const QSharedPointer<DataBaseReader> listsDb = listsDbForProg(progId);
    const QSharedPointer<DataBaseReader> progsDb = progsDbForProg(progId);
    if (listsDb.isNull() || progsDb.isNull()) {
        return false;
    }

//    qWarning() << "[ProgFlow] programmLoadSocketInit progId:" << progId << "clear:" << clear;

    QList<QVariantList> progInfo = progsDb->slotSendSelectQuery(QStringList{"Progs"},
                                                  QStringList{"Name_RU", "Scope_ID", "Endo"},
                                                  QString("id = %1").arg(progId));
//    qWarning() << "[ProgFlow] programmLoadSocketInit Progs lookup rows:" << progInfo.size();

    const bool useInlineUserData = (progId == 1000 || m_curLoaderType == ptUser);
    Q_UNUSED(useInlineUserData)
    
	//начинаем прорабатывать прогрузку несекольких экранов
	std::vector<std::map<int, SockPtr>> socketMapVector;
	std::vector<std::map<int, InstrPtr >> instrMapVector;
	std::map<int, std::map<int, InstrInfo>>  instrumConstraints;

	QList<QVariantList> progListVariant;

	// Если progId = 0, создаём фиктивную запись программы
	progListVariant = listsDb->slotSendSelectQuery(QStringList{"Lists"},
	                                                     fields,
	                                                     queryConditionRecom.arg(progId));
	if (progListVariant.size() == 0) {
		return false;
	}
    const bool isEndoProgram = (!progInfo.isEmpty() && progInfo.first().size() > 2)
            ? progInfo.first().at(2).toBool()
            : false;

    if (!clear
        && !m_socketModelPtr.isNull()
        && !m_socketModelPtr->endoProgramView()
        && isEndoProgram) {
        qWarning() << "programmLoadSocketInit: adding endo page to non-endo program is forbidden"
                   << "progId:" << progId;
        emit endoProgramAddBlocked();
        return false;
    }

	//Шаг5---------------------------------------------------------
	//тут какой-то затуп с базой на каких-то прогах, разрешено всего несколько инструментов
	//при этом для выбранных режимов эти инструменты не разрешены
	// for (auto iterItem = instrumConstraints.begin(); iterItem != instrumConstraints.end(); ++iterItem) {
	//     std::map<int, InstrInfo>& item = iterItem->second;
	//     filterMapByKey<InstrInfo>(item, allowedInstrId);
	// }

	//Шаг 6--------------------------------------------------------
	QList<QVariantList> modeNamesListV = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Modes"},
	                                                                        QStringList{DbLocale::column("Name"),"id"},
	                                                                        "");
	QStringList modeNamesList;
	for (const auto& iter : modeNamesListV) {
		modeNamesList.append(iter.at(0).toString());
	}

	int skippedNumOutOfRange = 0;
	std::set<int> acceptedNums;
	for (const auto& progItem : qAsConst(progListVariant)) {
		//ограничения для каждого листа
		const int num = progItem.at(1).toInt();
		if (num < 0 || num >= 4) {
			++skippedNumOutOfRange;
			continue;
		}
		if (acceptedNums.find(num) != acceptedNums.end()) {
			continue;
		}
		acceptedNums.insert(num);
		//Шаг2---------------------------------------------------------
		std::vector<int> allowedModesId = getAllowedModes(progId, progItem);
		if (allowedModesId.empty()) {
//			qWarning() << "[ProgFlow] programmLoadSocketInit: пустой allowedModesId"
//			           << "progId:" << progId
//			           << "listRowId:" << progItem.at(0).toInt()
//			           << "listNum:" << progItem.at(1).toInt();
		}

		//Шаг3---------------------------------------------------------
		std::vector<int> allowedInstrId = getAllowedInstrs(progId, progItem);

		//Шаг4---------------------------------------------------------

		instrumConstraints = getConstraints(allowedModesId);

		//-------------------------------------------------------------
		socketMapVector.push_back(std::map<int, SockPtr>());
		std::map<int, SockPtr>& socketMap = socketMapVector.back();

		for (int i = 0; i < 4; ++i) {
			Onyx::SocType type = Onyx::SocType(i+1);
			SockPtr socket = SockPtr::create(type);
			socketMap[i] = socket;

			socket->setSocketName(makeSocketName(type));

			for (int halfSocket = 0; halfSocket < 2; ++halfSocket ) {
				fillHalfSocket(halfSocket,
				                i,
				                socket,
				                progItem,
				                modeNamesList,
				                allowedModesId,
				                instrumConstraints);
			}
			bool coagEna = hasNonZeroDigit(progItem.at(29).toInt(), (8 - 2*i) - 1 );
			bool cutEna = hasNonZeroDigit(progItem.at(29).toInt(), (8 - 2*i) );

			const int singleIdx = listsPedalColumnToSocketIndex(progItem.at(27).toInt());
			const int doubleIdx = listsPedalColumnToSocketIndex(progItem.at(28).toInt());
			if (doubleIdx >= 0 && i == doubleIdx) {
				socket->setPedal(Onyx::DOUBLE_PED);
			} else if (singleIdx >= 0 && i == singleIdx) {
				socket->setPedal(Onyx::SINGLE_PED);
			} else {
				socket->setPedal(Onyx::NO_PED);
			}

			bool allowSock = cutEna || coagEna;
			socket->setAllowed(allowSock);
			socket->setDisplayMode(Onyx::S_COLLAPSED);
		}
		instrMapVector.push_back(getInstrums());
	}

	if (skippedNumOutOfRange > 0) {
//		qWarning() << "[ProgFlow] programmLoadSocketInit всего пропущено по Num:" << skippedNumOutOfRange;
	}
//	qWarning() << "[ProgFlow] programmLoadSocketInit socketMapVector.size:" << socketMapVector.size();
	if (socketMapVector.empty()) {
//		qWarning() << "[ProgFlow] programmLoadSocketInit: не собрано ни одной страницы (socketMap пуст)";
		return false;
	}

    if (isEndoProgram && socketMapVector.size() == 1 && instrMapVector.size() == 1) {
        // Отдельные экземпляры SOCKET/Instrument на втором листе — иначе общие SockPtr
        // дают связанную мощность и режим при одинаковом mode id на двух листах.
        socketMapVector.push_back(cloneSocketMap(socketMapVector.front()));
        instrMapVector.push_back(cloneInstrMap(instrMapVector.front()));
    }

	m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, !clear);
    m_socketModelPtr->setEndoProgramView(isEndoProgram);
//	qWarning() << "[ProgFlow] programmLoadSocketInit loadProgs завершён, add=" << (!clear);

	if (progId != 1000) {
		slotSaveCurrentState();
	}
	return (true);
}

bool ProgLoader::freeSettingsSocketInit(bool clear)
{
    if (m_dbReaderPtr.isNull() || m_socketModelPtr.isNull()) {
        return false;
    }

    const bool keepEndoView = m_socketModelPtr->endoProgramView();

    struct HalfSelectionState {
        int modeId = 1000;
        int instrId = 1000;
        int power = 1;
    };

    struct SocketSelectionState {
        HalfSelectionState cut;
        HalfSelectionState coag;
    };

    std::array<SocketSelectionState, 4> previousSelections;
    for (int i = 0; i < 4; ++i) {
        const QModelIndex currentIndex = m_socketModelPtr->index(i, 0);
        if (!currentIndex.isValid()) {
            continue;
        }

        previousSelections[i].coag.modeId = currentIndex.data(SocketModel::CoagModeId).toInt();
        previousSelections[i].coag.instrId = currentIndex.data(SocketModel::CoagModeInstrID).toInt();
        previousSelections[i].coag.power = currentIndex.data(SocketModel::CoagModePower).toInt();

        previousSelections[i].cut.modeId = currentIndex.data(SocketModel::CutModeId).toInt();
        previousSelections[i].cut.instrId = currentIndex.data(SocketModel::CutModeInstrID).toInt();
        previousSelections[i].cut.power = currentIndex.data(SocketModel::CutModePower).toInt();
    }
    
    std::vector<std::map<int, SockPtr>> socketMapVector;
    std::vector<std::map<int, InstrPtr >> instrMapVector;
    
    QList<QVariantList> modeNamesListV = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Modes"},
                                                                        QStringList{DbLocale::column("Name"),"id"},
                                                                        "");
    QStringList modeNamesList;
    for (const auto& iter : modeNamesListV)
        modeNamesList.append(iter.at(0).toString());
    
    socketMapVector.push_back(std::map<int, SockPtr>());
    std::map<int, SockPtr>& socketMap = socketMapVector[socketMapVector.size() - 1];
    
    for (int i = 0; i < 4; ++i) {
        Onyx::SocType type = Onyx::SocType(i+1);
        SockPtr socket = SockPtr::create(type);
        socketMap[i] = socket;
        
        socket->setSocketName(makeSocketName(type));
        
        for (int halfSocket = 0; halfSocket < 2; ++halfSocket) {
            bool isCoag = (halfSocket == 0);
            QMap<int, SurgModePtr> modes;
            
            QString queryConditionModes = "BI_MONO = %1 AND CUT_COAG = %2";
            QList<QVariantList> modesList = m_dbReaderPtr->slotSendSelectQuery(
                        QStringList{"Modes"},
                        QStringList{"MaxPower",DbLocale::column("Name"), "id", "Num",
                                    DbLocale::column("Brief"), DbLocale::column("Descript"), "ENDO_REG", "KEY", "Argon"},
                        queryConditionModes
                                    .arg(socket->socketType() <= Onyx::BIPOLAR_2 ? 0 : 1)
                                    .arg(halfSocket)
                                    + deviceModeFilterCondition());
            modesList = filterModesByFeatureKey(modesList, m_featureUnlock);
            
            std::sort(modesList.begin(), modesList.end(),
                [](const QVariantList& a, const QVariantList& b) {
                    return a.at(3).toInt() < b.at(3).toInt();
                });
            
            QList<int> allModesId;
            for (const auto& mode : modesList) {
                allModesId.append(mode.at(2).toInt());
            }

            std::vector<int> allModesIdVec;
            allModesIdVec.reserve(allModesId.size());
            for (int modeId : allModesId) {
                allModesIdVec.push_back(modeId);
            }
            std::map<int, std::map<int, Onyx::InstrInfo>> instrumConstraints = getConstraints(allModesIdVec);
            
            makeModes(modes, modesList, instrumConstraints, QVariantList(), i, isCoag, {}, true);
            
            if (i == 0 && modes.contains(7)) {
                modes.remove(7);
            }
            
            isCoag ? socket->setCoagModes(modes, modeNamesList)
                   : socket->setCutModes(modes, modeNamesList);
            
            int firstModeId = 1000;
            if (!modes.isEmpty() && modes.firstKey() != 1000) {
                firstModeId = modes.firstKey();
            }

            const HalfSelectionState& halfState = isCoag
                    ? previousSelections[i].coag
                    : previousSelections[i].cut;

            if (!socket->setModeId(halfState.modeId, isCoag)) {
                socket->setModeId(firstModeId, isCoag);
            }

            if (halfState.instrId > 0 && halfState.instrId != 1000) {
                socket->setInstrumId(halfState.instrId, isCoag);
            }

            const int powerValue = std::max(1, halfState.power);
            isCoag ? socket->setCoagModePower(powerValue) : socket->setCutModePower(powerValue);
        }
        
        socket->setAllowed(true);
        socket->setDisplayMode(Onyx::S_COLLAPSED);
    }
    
    instrMapVector.push_back(getInstrums());
    
    if (socketMapVector.empty())
        return false;

    if (clear) {
        // Для "Свободных установок" сохраняем остальные листы и
        // заменяем только текущий, чтобы не терялась структура подпрограмм.
        std::vector<std::map<int, SockPtr>> allSocketMaps = m_socketModelPtr->getSocketsCopy();
        std::vector<std::map<int, InstrPtr>> allInstrMaps = m_socketModelPtr->getInstrCopy();

        const int currentIdx = m_socketModelPtr->subProgIdx();
        if (allSocketMaps.empty() || allInstrMaps.empty()
            || currentIdx < 0
            || currentIdx >= static_cast<int>(allSocketMaps.size())
            || currentIdx >= static_cast<int>(allInstrMaps.size())) {
            m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, false);
            if (keepEndoView
                && socketMapVector.size() == 1
                && instrMapVector.size() == 1) {
                m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, true);
            }
            m_socketModelPtr->setEndoProgramView(keepEndoView);
            return true;
        }

        allSocketMaps[currentIdx] = socketMapVector.front();
        allInstrMaps[currentIdx] = instrMapVector.front();
        if (keepEndoView && allSocketMaps.size() == 1 && allInstrMaps.size() == 1) {
            allSocketMaps.push_back(allSocketMaps.front());
            allInstrMaps.push_back(allInstrMaps.front());
        }
        m_socketModelPtr->loadProgs(allSocketMaps, allInstrMaps, false);
        m_socketModelPtr->setSubProgIdx(currentIdx);
        m_socketModelPtr->setEndoProgramView(keepEndoView);
        return true;
    }

    m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, true);
    if (keepEndoView
        && socketMapVector.size() == 1
        && instrMapVector.size() == 1) {
        m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, true);
    }
    m_socketModelPtr->setEndoProgramView(keepEndoView);
    return true;
}

std::map<int, QString> ProgLoader::getProgs(int scopeID)
{
	//захардкодили, но это нужно знать
	const std::unique_ptr<ProgLoaderBase> loader{getLoader(m_curLoaderType)};
	const auto programs = loader->getPrograms(scopeID);
	m_lastProgSubLists.clear();
	if (m_curLoaderType == ptRecom) {
		const auto* recomLoader = dynamic_cast<RecomProgLoader*>(loader.get());
		if (recomLoader) {
			m_lastProgSubLists = recomLoader->cachedSubLists();
		}
	}
	return programs;
}

std::map<int, QString> ProgLoader::getCategories()
{
	const std::unique_ptr<ProgLoaderBase> loader{getLoader(m_curLoaderType)};
	auto categories = loader->getCategories();
	if (!m_featureUnlock) {
		return categories;
	}

	std::map<int, QString> filtered;
	for (const auto &item : categories) {
		if (!m_featureUnlock->isScopeLocked(item.first)) {
			filtered.insert(item);
		}
	}
	return filtered;
}

void ProgLoader::deleteUserProg(int id)
{
	if (m_userDbReaderPtr.isNull()) {
		return;
	}
	const QString removeQuery = "DELETE FROM Lists WHERE Prog_ID = %1";
	m_userDbReaderPtr->executeUpdateQuery(removeQuery.arg(id));

	const QString removeProgQuery = "DELETE FROM Progs WHERE id = %1";
	m_userDbReaderPtr->executeUpdateQuery(removeProgQuery.arg(id));

	m_userDbReaderPtr->commit();

}

void ProgLoader::renameUserProg(int id, const QString &name)
{
	if (m_userDbReaderPtr.isNull()) {
		return;
	}
	const QString trimmedName = name.trimmed();
	if (trimmedName.isEmpty()) {
		return;
	}

	const QString conn = m_userDbReaderPtr->connectionName();

	bool ok = executePreparedUpdate(
	            conn,
	            "UPDATE Progs "
	            "SET Name_RU = ?, Name_ES = ?, Name_EN = ? "
	            "WHERE id = ?",
	            [trimmedName, id](QSqlQuery& query) {
		query.addBindValue(trimmedName);
		query.addBindValue(trimmedName);
		query.addBindValue(trimmedName);
		query.addBindValue(id);
	},
	            "renameUserProg");

	// Fallback for databases without ES/EN columns.
	if (!ok) {
		ok = executePreparedUpdate(
		            conn,
		            "UPDATE Progs "
		            "SET Name_RU = ? "
		            "WHERE id = ?",
		            [trimmedName, id](QSqlQuery& query) {
			query.addBindValue(trimmedName);
			query.addBindValue(id);
		},
		            "renameUserProg_fallback_ru");
	}

	if (ok) {
		m_userDbReaderPtr->commit();
		const QList<QVariantList> rows = m_userDbReaderPtr->slotSendSelectQuery(
		            QStringList{"Progs"},
		            QStringList{"Name_RU"},
		            QString("id = %1").arg(id));
		if (rows.isEmpty()) {
			qWarning() << "renameUserProg: no row after update for id" << id;
			return;
		}
		const auto& row = rows.first();
		const QString ru = row.at(0).toString().trimmed();
		if (ru != trimmedName) {
			qWarning() << "renameUserProg: value mismatch after update for id" << id
			           << "expected:" << trimmedName
			           << "actual:" << ru;
		}
	} else {
		qWarning() << "renameUserProg: update failed for id" << id
		           << "name:" << trimmedName;
	}
}

void ProgLoader::deleteUserScope(int id)
{
	if (m_userDbReaderPtr.isNull()) {
		return;
	}
	const QString queryCondition = "Scope_ID = %1";
	QList<QVariantList> progListVariant =
	        m_userDbReaderPtr->slotSendSelectQuery(QStringList{"Progs"},
	                                          QStringList{"id"},
	                                          queryCondition.arg(id));
	QStringList progIds;
	progIds.reserve(progListVariant.size());
	for (const auto& item : progListVariant) {
		progIds.append(QString::number(item.at(0).toInt()));
	}

	const QString removeProgsQuery = "DELETE FROM Progs WHERE Scope_ID = %1";
	m_userDbReaderPtr->executeUpdateQuery(removeProgsQuery.arg(id));

	if (!progIds.isEmpty()) {
		const QString removeListsQuery = "DELETE FROM Lists WHERE Prog_ID IN (%1)";
		m_userDbReaderPtr->executeUpdateQuery(removeListsQuery.arg(progIds.join(", ")));
	}

	const QString removeScopeQuery = "DELETE FROM Scopes WHERE id = %1";
	m_userDbReaderPtr->executeUpdateQuery(removeScopeQuery.arg(id));

	m_userDbReaderPtr->commit();

}

void ProgLoader::renameUserScope(int id, const QString &name)
{
	if (m_userDbReaderPtr.isNull()) {
		return;
	}
	const QString trimmedName = name.trimmed();
	if (trimmedName.isEmpty()) {
		return;
	}

	const QString conn = m_userDbReaderPtr->connectionName();

	qWarning() << "renameUserScope start id" << id << "name" << trimmedName;
	bool ok = executePreparedUpdate(
	            conn,
	            "UPDATE Scopes "
	            "SET Name_RU = ?, Name_ES = ?, Name_EN = ? "
	            "WHERE id = ?",
	            [trimmedName, id](QSqlQuery& query) {
		query.addBindValue(trimmedName);
		query.addBindValue(trimmedName);
		query.addBindValue(trimmedName);
		query.addBindValue(id);
	},
	            "renameUserScope");

	// Fallback for databases without ES/EN columns.
	if (!ok) {
		ok = executePreparedUpdate(
		            conn,
		            "UPDATE Scopes "
		            "SET Name_RU = ? "
		            "WHERE id = ?",
		            [trimmedName, id](QSqlQuery& query) {
			query.addBindValue(trimmedName);
			query.addBindValue(id);
		},
		            "renameUserScope_fallback_ru");
	}

	if (ok) {
		m_userDbReaderPtr->commit();
		const QList<QVariantList> rows = m_userDbReaderPtr->slotSendSelectQuery(
		            QStringList{"Scopes"},
		            QStringList{"Name_RU"},
		            QString("id = %1").arg(id));
		if (rows.isEmpty()) {
			qWarning() << "renameUserScope: no row after update for id" << id;
			return;
		}
		const auto& row = rows.first();
		const QString ru = row.at(0).toString().trimmed();
		if (ru != trimmedName) {
			qWarning() << "renameUserScope: value mismatch after update for id" << id
			           << "expected:" << trimmedName
			           << "actual:" << ru;
		}
	} else {
		qWarning() << "renameUserScope: update failed for id" << id
		           << "name:" << trimmedName;
	}
}

std::map<int, std::map<int, Onyx::InstrInfo> > ProgLoader::getConstraints(const std::vector<int>& idList)
{
	std::map<int, std::map<int, Onyx::InstrInfo> > result;
	const QString queryCondition = "Mode_ID = %1";

	for (int i : idList) {
		QList<QVariantList> intstrListForMode = m_dbReaderPtr->slotSendSelectQuery(QStringList{"ModInstr"},
		                                                                           QStringList{"Instr_ID","Min_Power","Mid_Power","Max_Power"},
		                                                                           queryCondition.arg(i));
		std::map<int, Onyx::InstrInfo>& modeMap = result[i];
		for (const auto& item : intstrListForMode) {
			int a = item.at(0).toInt();
			int b = item.at(1).toInt();
			int c = item.at(2).toInt();
			int d = item.at(3).toInt();
			Onyx::InstrInfo bla = Onyx::InstrInfo(a,
			                                      b,
			                                      c,
			                                      d);
			modeMap[a] = bla;
		}
	}
	return result;
}

std::map<int, InstrPtr > ProgLoader::getInstrums()
{
	std::map<int, InstrPtr> result;
	QList<QVariantList> instrListForMode = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Instruments"},
	                                                                          QStringList{"id","Num","BI_MONO",DbLocale::column("Name"),DbLocale::column("Brief")},
	                                                                          "");
	/* int id, int legacyNumber, const QString& name, bool mono */
	for (const auto& item : instrListForMode) {
		InstrPtr ptr = InstrPtr::create(item.at(0).toInt(),
		                                item.at(1).toInt(),
		                                item.at(3).toString(),
		                                item.at(2).toInt() == 1 ? true : false);
		ptr->setDescription(item.at(4).toString());
		result[item.at(0).toInt()] = ptr;
	}
	return result;
}

void ProgLoader::saveUserProg(const QString &scopeName, const QString &progName)
{
	if (m_socketModelPtr == nullptr
	        || m_socketModelPtr->itemsMap() == nullptr
	        || m_userDbReaderPtr.isNull()) {
		// qWarning() << "Cannot save state: socket model not initialized";
		return;
	}

	int idToUse = -1;
	int biggestKnownId = -1;
    const int endoFlag = (m_socketModelPtr->endoProgramView() ? 1 : 0);

	auto getScopeId = [this](const QString& name, int& bigId) -> int {
		int res = -1;
		const std::unique_ptr<ProgLoaderBase> loader{getLoader(ptUser)};

		auto list = loader->getCategories();
		for (const auto& item : list) {
			if (item.second == name) {
				res = item.first;
				break;
			}
		}
		if (!list.empty()) {
			bigId = list.rbegin()->first;
		}
		return res;
	};

	int scopeId = getScopeId(scopeName, biggestKnownId);

	if (scopeId == -1) {
		//тут добавить новый скоуп
		const QString escapedScopeName = QString(scopeName).replace(QLatin1Char('\''), QLatin1String("''"));
		const   QString insertUserProgNameQuery = QString(
		                                              "INSERT INTO Scopes "
		                                              "(id, Num, Name_RU, Name_EN, Name_ES)"
		                                              " VALUES "
		                                              "(%1, %1, '%2', '%2', '%2')");
		int id = biggestKnownId > 1000 ? biggestKnownId + 1 : 1001;
		if (!m_userDbReaderPtr->executeUpdateQuery(insertUserProgNameQuery
		                                       .arg(id)
		                                       .arg(escapedScopeName))) {
			return;
		} else {
			m_userDbReaderPtr->commit();
			scopeId = id;
		}
	}

	auto nextUserProgId = [this]() -> int {
		int maxUserProgId = 1000;
		const QList<QVariantList> userProgIds = m_userDbReaderPtr->slotSendSelectQuery(
		            QStringList{"Progs"},
		            QStringList{"id"},
		            "id > 1000");
		for (const auto& row : userProgIds) {
			if (!row.isEmpty()) {
				maxUserProgId = std::max(maxUserProgId, row.at(0).toInt());
			}
		}
		return maxUserProgId + 1;
	};

	const QString escapedProgName = QString(progName).replace("'", "''");
	QList<QVariantList> existingProgRows = m_userDbReaderPtr->slotSendSelectQuery(
	            QStringList{"Progs"},
	            QStringList{"id"},
	            QString("Scope_ID = %1 AND Name_RU = '%2' AND id > 1000")
	            .arg(scopeId)
	            .arg(escapedProgName));

	if (!existingProgRows.isEmpty() && !existingProgRows.first().isEmpty()) {
		idToUse = existingProgRows.first().at(0).toInt();
        m_userDbReaderPtr->executeUpdateQuery(
                    QString("UPDATE Progs SET Endo = %1 WHERE id = %2")
                    .arg(endoFlag)
                    .arg(idToUse));
        m_userDbReaderPtr->commit();
	} else {
		const int newUserProgId = nextUserProgId();
		const  QString insertUserProgNameQuery = QString(
		                                             "INSERT INTO Progs "
		                                             "(id, Prog_NUM, Argon, Name_RU, Name_EN, Name_ES, Scope_ID, Sub_NUM, Endo)"
		                                             " VALUES "
		                                             "(%1, 0, 0, '%2', '%2', '%2', %3, 0, %4)");

		if (!m_userDbReaderPtr->executeUpdateQuery(insertUserProgNameQuery
		                                       .arg(newUserProgId)
		                                       .arg(escapedProgName)
		                                       .arg(scopeId)
		                                       .arg(endoFlag))) {
            const QString fallbackInsertUserProgNameQuery = QString(
                        "INSERT INTO Progs "
                        "(id, Prog_NUM, Argon, Name_RU, Name_EN, Name_ES, Scope_ID, Sub_NUM)"
                        " VALUES "
                        "(%1, 0, 0, '%2', '%2', '%2', %3, 0)");
            if (!m_userDbReaderPtr->executeUpdateQuery(fallbackInsertUserProgNameQuery
                                                   .arg(newUserProgId)
                                                   .arg(escapedProgName)
                                                   .arg(scopeId))) {
                return;
            }
            m_userDbReaderPtr->executeUpdateQuery(
                        QString("UPDATE Progs SET Endo = %1 WHERE id = %2")
                        .arg(endoFlag)
                        .arg(newUserProgId));
            m_userDbReaderPtr->commit();
        } else {
            m_userDbReaderPtr->commit();
        }
		idToUse = newUserProgId;
	}
	int pageCount = m_socketModelPtr->subProgCount();
	QList<QStringList> listsStr = prepSaveState();

	QString deleteOldListsQuery = QString("DELETE FROM Lists WHERE Prog_ID = %1").arg(idToUse);
	if (!m_userDbReaderPtr->executeUpdateQuery(deleteOldListsQuery)) {
	} else {
		m_userDbReaderPtr->commit();
	}

	for (int i = 0; i < pageCount; ++i) {
		const auto& curList = listsStr.at(i);
		QString query = insertUserProgQuery
		                // QString query = saveCurQuery
		                .arg(curList.at(0)).arg(curList.at(1)).arg(curList.at(2)) //bi1 cut
		                .arg(curList.at(3)).arg(curList.at(4)).arg(curList.at(5)) //bi1 coag
		                .arg(curList.at(6)).arg(curList.at(7)).arg(curList.at(8)) //bi2 cut
		                .arg(curList.at(9)).arg(curList.at(10)).arg(curList.at(11)) //bi2 coag
		                .arg(curList.at(12)).arg(curList.at(13)).arg(curList.at(14)) //mono1 cut
		                .arg(curList.at(15)).arg(curList.at(16)).arg(curList.at(17)) //mono1 coag
		                .arg(curList.at(18)).arg(curList.at(19)).arg(curList.at(20)) //mono2 cut
		                .arg(curList.at(21)).arg(curList.at(22)).arg(curList.at(23)) //mono2 coag
		                .arg(curList.at(24)).arg(curList.at(25)).arg(curList.at(26)) //pedals and outmask
		                .arg(i).arg(idToUse);

		if (!m_userDbReaderPtr->executeUpdateQuery(query)) {
		}
	}
	m_userDbReaderPtr->commit();
}

void ProgLoader::deleteAllUserProgs()
{
    if (m_userDbReaderPtr.isNull()) {
        qWarning() << "Cannot delete user progs: database not initialized";
        return;
    }
    
    QList<QVariantList> userProgs = m_userDbReaderPtr->slotSendSelectQuery(
        QStringList{"Progs"},
        QStringList{"id"},
        "Scope_ID >= 1000");
    
    qWarning() << "Найдено пользовательских программ:" << userProgs.size();
    
    for (const auto& prog : userProgs) {
        int progId = prog.at(0).toInt();
        
        QString deleteListsQuery = QString("DELETE FROM Lists WHERE Prog_ID = %1").arg(progId);
        if (!m_userDbReaderPtr->executeUpdateQuery(deleteListsQuery)) {
            qWarning() << "Failed to delete Lists for prog ID:" << progId;
        }
    }
    
    QString deleteProgsQuery = "DELETE FROM Progs WHERE Scope_ID >= 1000";
    if (!m_userDbReaderPtr->executeUpdateQuery(deleteProgsQuery)) {
        qWarning() << "Failed to delete user programs";
    } else {
        m_userDbReaderPtr->commit();
        qWarning() << "Все пользовательские программы удалены";
	}
}

ProgLoaderBase *ProgLoader::getLoader(progType type)
{

	switch (type) {
	case ptRecom:
		return new RecomProgLoader(deviceHasArgon());
	case ptUser:
		return new UserProgLoader(deviceHasArgon());
	default:
		break;
	}
	return nullptr;
}

void ProgLoader::setJsonStorage(JsonStorage *jsonStorage)
{
	m_jsonStorage = jsonStorage;
}

void ProgLoader::setFeatureUnlockController(FeatureUnlockController *controller)
{
	m_featureUnlock = controller;
	if (m_featureUnlock && !m_dbReaderPtr.isNull()) {
		m_featureUnlock->setDatabaseReader(m_dbReaderPtr.data());
	}
}

bool ProgLoader::deviceHasArgon() const
{
	if (m_jsonStorage.isNull()) {
		return true;
	}

	const QString deviceType = m_jsonStorage->readString(QStringLiteral("deviceType"),
	                                                     QStringLiteral("ONYX-AM")).trimmed().toUpper();
	return deviceType != QStringLiteral("ONYX-M");
}

QString ProgLoader::deviceModeFilterCondition() const
{
	// Без газового тракта скрываем режимы с Modes.Argon = 1
	return deviceHasArgon() ? QString() : QStringLiteral(" AND IFNULL(Argon, 0) = 0");
}

std::vector<int> ProgLoader::filterModesForDevice(const std::vector<int>& modeIds) const
{
	if (deviceHasArgon() || modeIds.empty() || m_dbReaderPtr.isNull()) {
		return modeIds;
	}

	QList<QVariantList> allowedModeRows = m_dbReaderPtr->slotSendSelectQuery(
	            QStringList{"Modes"},
	            QStringList{"id"},
	            QString("id IN (%1)%2")
	            .arg(makeCommaSeparatedNumbers(modeIds))
	            .arg(deviceModeFilterCondition()));

	std::unordered_set<int> allowedModeIds;
	allowedModeIds.reserve(static_cast<size_t>(allowedModeRows.size()));
	for (const auto& row : allowedModeRows) {
		allowedModeIds.insert(row.at(0).toInt());
	}

	std::vector<int> filtered;
	filtered.reserve(modeIds.size());
	for (int modeId : modeIds) {
		if (modeId == 1000 || allowedModeIds.find(modeId) != allowedModeIds.end()) {
			filtered.push_back(modeId);
		}
	}
	return filtered;
}

std::vector<int> ProgLoader::getAllowedInstrs(int progId, const QVariantList& progItem)
{
	std::vector<int> allowedInstrId;
	const bool useInlineUserData = (progId == 1000 || m_curLoaderType == ptUser);
	if (!useInlineUserData) {
		QList<QVariantList> allowedInstr
		        = m_dbReaderPtr->slotSendSelectQuery(QStringList{"EnableInstr"},
		                                             QStringList{"Instr_ID"},
		                                             QString("List_ID = %1").arg(progItem.at(0).toUInt()));

		for (const auto& item : allowedInstr) {
			allowedInstrId.push_back(item.at(0).toInt());
		}
	} else {
		std::vector<int> allowedInstrId__;
		for (int i = 0; i < 8; ++i) {
			// allowedInstrId__.push_back(progItem.at(3 + 3*i).toInt());
			QString allowed = progItem.at(3 + 3*i).toString();
			QStringList list = allowed.split(',');
			for (const auto& instr : list) {
				allowedInstrId__.push_back(instr.toInt());
			}
		}
		std::sort(allowedInstrId__.begin(), allowedInstrId__.end());
		auto last = std::unique(allowedInstrId__.begin(), allowedInstrId__.end());
		allowedInstrId__.erase(last, allowedInstrId__.end());
		allowedInstrId = allowedInstrId__;
	}
	return allowedInstrId;

}

std::vector<int> ProgLoader::getAllowedModes(int progId, const QVariantList& progItem)
{
	std::vector<int> allowedModesId;
	const bool useInlineUserData = (progId == 1000 || m_curLoaderType == ptUser);
	if (!useInlineUserData) {
		const int listId = progItem.at(0).toInt();
		QList<QVariantList> allowedModes
		        = m_dbReaderPtr->slotSendSelectQuery(QStringList{"EnableModes"},
		                                             QStringList{"Mode_ID"},
		                                             QString("Prog_ID = %1").arg(progId));
		// На части БД рекомендованные режимы привязаны не к Prog_ID, а к List_ID.
		if (allowedModes.isEmpty()) {
//			qWarning() << "[ProgFlow] getAllowedModes: пусто по Prog_ID, fallback на List_ID"
//			           << "progId:" << progId << "listId:" << listId;
			allowedModes = m_dbReaderPtr->slotSendSelectQuery(QStringList{"EnableModes"},
			                                                  QStringList{"Mode_ID"},
			                                                  QString("List_ID = %1").arg(listId));
		}
		allowedModesId.reserve(allowedModes.size());
		for (const auto& item : allowedModes) {
			allowedModesId.push_back(item.at(0).toInt());
		}
		// Последний fallback: извлекаем режимы из строки Lists (как для user-программ),
		// чтобы загрузка программы не ломалась даже при отсутствующих связях в EnableModes.
		if (allowedModesId.empty()) {
//			qWarning() << "[ProgFlow] getAllowedModes: пусто и по List_ID, fallback из Lists полей"
//			           << "progId:" << progId << "listId:" << listId;
			for (int i = 0; i < 8; ++i) {
				QString allowed = progItem.at(4 + 3*i).toString();
				QStringList list = allowed.split(',', Qt::SkipEmptyParts);
				for (const auto& mode : list) {
					allowedModesId.push_back(mode.toInt());
				}
			}
			std::sort(allowedModesId.begin(), allowedModesId.end());
			auto last = std::unique(allowedModesId.begin(), allowedModesId.end());
			allowedModesId.erase(last, allowedModesId.end());
//			qWarning() << "[ProgFlow] getAllowedModes: fallback из Lists дал modeCount:"
//			           << allowedModesId.size();
		}
	} else {
		// std::vector<int> allowedModesId__;
		//просто разрешаем все назначенные инструменты и режимы, если такие допустимы на сокетах, без дополнительных масок
		for (int i = 0; i < 8; ++i) {
			QString allowed = progItem.at(4 + 3*i).toString();
			QStringList list = allowed.split(',');
			for (const auto& mode : list) {
				allowedModesId.push_back(mode.toInt());
			}
		}
		std::sort(allowedModesId.begin(), allowedModesId.end());
		if (allowedModesId.size() > 1) {
			// std::unordered_set<int> check;
			auto last = std::unique(allowedModesId.begin(), allowedModesId.end());
			allowedModesId.erase(last, allowedModesId.end());
		}

		// allowedModesId = QList<int>::fromVector(QVector<int>(allowedModesId__.begin(), allowedModesId__.end()));
	}
	return filterModesForDevice(allowedModesId);

}

void ProgLoader::fillHalfSocket(int halfSocket,
                                int socketNumber,
                                SockPtr socket,
                                const QVariantList& progItem,
                                const QStringList& modeNamesList,
                                const std::vector<int>& allowedModesId,
                                const std::map<int, std::map<int, InstrInfo>>& instrumConstraints)
{
	bool isCoag = (halfSocket == 0);
	const int biMonoFlag = (socket->socketType() <= Onyx::BIPOLAR_2 ? 0 : 1);
	QMap<int, SurgModePtr> modes;
	QList<QVariantList> modesList;
	if (!allowedModesId.empty()) {
		modesList = m_dbReaderPtr->slotSendSelectQuery(
		            QStringList{"Modes"},
		            QStringList{"MaxPower",DbLocale::column("Name"), "id", "Num",
		                        DbLocale::column("Brief"), DbLocale::column("Descript"), "ENDO_REG", "KEY", "Argon"},
		            queryConditionModes
		            .arg(biMonoFlag)
		            .arg(halfSocket)
		            .arg(makeCommaSeparatedNumbers(allowedModesId))
		            + deviceModeFilterCondition());
		modesList = filterModesByFeatureKey(modesList, m_featureUnlock);
	}

	// Сортируем по Num (index 3)
	std::sort(modesList.begin(), modesList.end(),
	          [](const QVariantList& a, const QVariantList& b) {
		return a.at(3).toInt() < b.at(3).toInt();
	});

	int start = isCoag ? 6 : 3;
	std::vector<int> instIdLst = parseCommaSeparatedNumbers(progItem.at(start + 6*socketNumber).toString());;
	std::vector<int> modeIdLst = parseCommaSeparatedNumbers(progItem.at(start + 1 + 6*socketNumber).toString());

	makeModes(modes,
	          modesList,
	          instrumConstraints,
	          progItem,
	          socketNumber,
	          isCoag,
	          instIdLst);  // Передаём фильтр инструментов

	filterModeMap(modes, modeIdLst);

	// Исключаем режим "Термошов" (ID=7) для сокета БИ2 (i=0)
	if (socketNumber == 0 && modes.contains(7)) {
		modes.remove(7);
	}

	isCoag ? socket->setCoagModes(modes, modeNamesList)
	       : socket->setCutModes(modes, modeNamesList);

	int firstInstrId;
	int firstModeId;
	int defaultPower;

	firstInstrId = instIdLst.size() == 0 ? 0 : instIdLst.at(0);
	firstModeId = modeIdLst.size() == 0 ? 1000 : modeIdLst.at(0);
	defaultPower = progItem.at(start + 2 + 6*socketNumber).toInt();
	defaultPower = std::max(1, defaultPower);

	if (!socket->setModeId(firstModeId, isCoag)) {
		socket->setModeId(1000, isCoag);
	}
	socket->setInstrumId(firstInstrId, isCoag);

	// Проверка для эндоскопических режимов: если мощность = 1, устанавливаем 11
	auto mode = isCoag ? socket->curCoagMode() : socket->curCutMode();
	if (!mode.isNull() && mode->isEndo()) {
		int endoCut = defaultPower / 10;  // Целочисленное деление вместо floor
		int endoCoag = defaultPower % 10;
		if (endoCut < 1)
			endoCut = 1;
		else if (endoCut > ENDO_MAX)
			endoCut = ENDO_MAX;
		if (endoCoag < 1)
			endoCoag = 1;
		else if (endoCoag > ENDO_MAX)
			endoCoag = ENDO_MAX;
		defaultPower = endoCut * 10 + endoCoag;
	}

	isCoag ? socket->setCoagModePower(defaultPower)
	       : socket->setCutModePower(defaultPower);
}

void ProgLoader::setSocketModelPtr(QSharedPointer<SocketModel> newSocketModelPtr)
{
	m_socketModelPtr = newSocketModelPtr;
}

int ProgLoader::addUserScope(const QString &name)
{
	if (m_userDbReaderPtr.isNull()) {
		return -1;
	}
	const QString escapedName = QString(name).replace(QLatin1Char('\''), QLatin1String("''"));
	const   QString insertUserProgNameQuery = QString(
	                                              "INSERT INTO Scopes ("
	                                              "id, Num, Name_RU, Name_EN, Name_ES"
	                                              ") VALUES ("
	                                              "%1, %1, '%2', '%2', '%2')");

	int scopeId = -1;
	{
		QList<QVariantList> scopes = m_userDbReaderPtr->slotSendSelectQuery(QStringList{"Scopes"},
		                                                                QStringList{"id"},
		                                                                "id >= 1000 ORDER BY id");
		if (scopes.isEmpty()) {
			scopeId = 1001;
		} else {
			scopeId = scopes.last().at(0).toInt() + 1;
		}
	}
	if (scopeId == -1) {
		return scopeId;
	}
	if (!m_userDbReaderPtr->executeUpdateQuery(insertUserProgNameQuery.arg(scopeId).arg(escapedName))) {
		qDebug() << "Failed to save new scope";
		return -1;
	}
	m_userDbReaderPtr->commit();
	return scopeId;
}

bool ProgLoader::loadCurrentState()
{
	if (m_socketModelPtr.isNull())
		return false;
	bool res = programmLoadSocketInit(1000, true);
	return res;
}

void ProgLoader::setCurLoaderType(progType newCurLoaderType)
{
	m_curLoaderType = newCurLoaderType;
}

QList<QStringList> ProgLoader::prepSaveState()
{
	QList<QStringList> listsStr;
	EshfProgStringBuilder builder;
	int pageCount = m_socketModelPtr->subProgCount();
	builder.setState(m_socketModelPtr->getSocketsCopy(),
	                 m_socketModelPtr->getInstrCopy());

	//вообще можно, начиная отсюда, впиливать выполнение в отдельном потоке

	listsStr.reserve(pageCount);

	for (int i = 0; i < pageCount; ++i) {
		listsStr.push_back({});
		auto& curList = listsStr.back();
		for (int j = 0; j < 4; ++j) {
			curList.append(builder.makeSocketString(j,i));
		}
		curList.append(builder.makePedalString(i));
		curList.append(builder.makeOutEnableMask(i));
	}
	return listsStr;
}

