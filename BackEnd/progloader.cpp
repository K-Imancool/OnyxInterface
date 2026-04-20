#include "progloader.h"
#include "BackEnd/recomprogloader.h"
#include "BackEnd/userprogloader.h"
#include "socket.h"
#include "onyxapp.h"
#include "EshfProgStringBuilder.h"

#include <unordered_set>
#include <vector>
#include <QDebug>
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

using namespace Onyx;

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

QString makeSocketName(Onyx::SocType type) {
    QString socketName = "";
    switch (type) {
    case Onyx::EMPTY:
        socketName = QString("EMPTY");
        break;
    case Onyx::BIPOLAR_1:
        socketName = QString("БИ1");
        break;
    case Onyx::BIPOLAR_2:
        socketName = QString("БИ2");
        break;
    case Onyx::MONOPOLAR_1:
        socketName = QString("МОНО1");
        break;
    case Onyx::MONOPOLAR_2:
        socketName = QString("МОНО2");
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

    container.insert(1000, SurgModePtr::create(ESHF::modesNames.last(),
                                                                   isCoag,
                                                                   1,
                                                                   1,
                                                                   1000,
                                                                   std::map<int, InstrInfo>(),
                                                                   1000,
                                                                   "",
                                                                   "",
                                                                   false));  // Num = 1000, Brief = "", Descript = "", isEndo = false


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

		SurgModePtr ptr = SurgModePtr::create(modeName,
		                                      isCoag,
		                                      item.at(0).toInt(),
		                                      1,
		                                      modeId,
		                                      tmp,
		                                      modeNum,
		                                      modeBrief,
		                                      modeDescript,
		                                      isEndo);  // Передаём Brief, Descript и isEndo
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


QString makeDbStringInstr( const QModelIndex& idx,
                          int listRole,
                          int firstItemRole) {
    QStringList list = idx.data(listRole).toStringList();
    int firstItem = idx.data(firstItemRole).toInt();
    list.removeOne(QString::number(firstItem));
    list.prepend(QString::number(firstItem));
    return list.join(',');
}

    const QString queryConditionRecom = "Prog_ID = %1 AND Num >= 0 AND Num < 4";
    const QString queryConditionUser = "User_ID = %1";
    const QStringList fields = {"id", "Num", "Prog_ID", /* 0 1 2*/
                          "Bi1Cut_INSTR", "Bi1Cut_MODE", "Bi1Cut_POWER", /* 3 4 5*/
                          "Bi1Coag_INSTR", "Bi1Coag_MODE", "Bi1Coag_POWER", /* 6 7 8*/
                          "Bi2Cut_INSTR", "Bi2Cut_MODE", "Bi2Cut_POWER", /* 9 10 11*/
                          "Bi2Coag_INSTR", "Bi2Coag_MODE", "Bi2Coag_POWER", /* 12 13 14*/
                          "Mono1Cut_INSTR", "Mono1Cut_MODE", "Mono1Cut_POWER", /* 15 16 17*/
                          "Mono1Coag_INSTR", "Mono1Coag_MODE", "Mono1Coag_POWER", /* 18 19 20*/
                          "Mono2Cut_INSTR", "Mono2Cut_MODE", "Mono2Cut_POWER", /* 21 22 23*/
                          "Mono2Coag_INSTR", "Mono2Coag_MODE", "Mono2Coag_POWER", /* 24 25 26*/
                          "Pedal_1", "Pedal_2", "OutEnabled_MASK"};/* 27 28 29*/
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
                                       "%28, 1000, "
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

}

ProgLoader::ProgLoader(QObject *parent)
    : QObject{parent}
{
	if (m_dbReaderPtr.isNull()) {
		OnyxApp* app = dynamic_cast<OnyxApp*>(qApp);
		if (app) {
			m_dbReaderPtr = app->getDbReader();
		}
	}
}

void ProgLoader::slotSaveCurrentState()
{
	if (m_socketModelPtr == nullptr
	        || m_socketModelPtr->itemsMap() == nullptr) {
		// qWarning() << "Cannot save state: socket model not initialized";
		return;
	}

	int pageCount = m_socketModelPtr->subProgCount();
	QList<QStringList> listsStr = prepSaveState();

	// Формируем SQL запрос для REPLACE (INSERT OR UPDATE)
	QString removeQuerry = "DELETE FROM Lists WHERE Prog_ID = 1000";
	m_dbReaderPtr->executeUpdateQuery(removeQuerry);
	// qDebug() << "saving N pages - " << subCount;
	for (int i = 0; i < pageCount; ++i) {
		const auto& curList = listsStr.at(i);

		QString query = insertCurQuery
		                // QString query = saveCurQuery
		                .arg(curList.at(0)).arg(curList.at(1)).arg(curList.at(2)) //mono1 cut
		                .arg(curList.at(3)).arg(curList.at(4)).arg(curList.at(5)) //mono1 coag
		                .arg(curList.at(6)).arg(curList.at(7)).arg(curList.at(8)) //mono2 cut
		                .arg(curList.at(9)).arg(curList.at(10)).arg(curList.at(11)) //mono2 coag
		                .arg(curList.at(12)).arg(curList.at(13)).arg(curList.at(14)) //bi1 cut
		                .arg(curList.at(15)).arg(curList.at(16)).arg(curList.at(17)) //bi1 coag
		                .arg(curList.at(18)).arg(curList.at(19)).arg(curList.at(20)) //bi2 cut
		                .arg(curList.at(21)).arg(curList.at(22)).arg(curList.at(23)) //bi2 coag
		                .arg(curList.at(24)).arg(curList.at(25)).arg(curList.at(26)) //pedals and outmask
		                .arg(i + 1);

		if (!m_dbReaderPtr->executeUpdateQuery(query)) {
			// qDebug() << "Failed to save current state";
		}
	}
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
                                       QStringList{"id"},
                                       "");
    allowedModesId.reserve(allModes.size());
    for (const auto& item : allModes) {
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
                                                                            QStringList{"Name_RU","id"},
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
                                                                                   QStringList{"MaxPower","Name_RU", "id", "Num", "Brief_RU", "Descript_RU", "ENDO_REG"},
                                                                                   queryConditionModes
                                                                                   .arg(socket->socketType() <= Onyx::BIPOLAR_2 ? 0 : 1)
                                                                                   .arg(halfSocket)
                                                                                   .arg(makeCommaSeparatedNumbers(allowedModesId)));

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

        if (clear) {
            qDebug() << "[ProgLoader] clear=true, using only first valid list row";
            break;
        }
    }
    m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, !clear);
}

bool ProgLoader::programmLoadSocketInit(int progId, bool clear)
{
	qDebug() << "[ProgLoader] programmLoadSocketInit progId=" << progId
	         << " clear=" << clear;
    //начинаем прорабатывать прогрузку несекольких экранов
    std::vector<std::map<int, SockPtr>> socketMapVector;
    std::vector<std::map<int, InstrPtr >> instrMapVector;
    std::map<int, std::map<int, InstrInfo>>  instrumConstraints;

    QList<QVariantList> progListVariant;

    // Если progId = 0, создаём фиктивную запись программы
    progListVariant = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Lists"},
                                                         fields,
                                                         queryConditionRecom.arg(progId));
	qDebug() << "[ProgLoader] Lists rows for progId" << progId << "=" << progListVariant.size();
    if (progListVariant.size() == 0) {
		qDebug() << "[ProgLoader] no rows in Lists for progId=" << progId;
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
                                                                            QStringList{"Name_RU","id"},
                                                                            "");
    QStringList modeNamesList;
    for (const auto& iter : modeNamesListV) {
        modeNamesList.append(iter.at(0).toString());
    }

    for (const auto& progItem : qAsConst(progListVariant)) {
        qDebug() << "[ProgLoader] row id=" << progItem.at(0).toInt()
                 << " num=" << progItem.at(1).toInt()
                 << " progId=" << progItem.at(2).toInt()
                 << " bi1CutModeRaw=" << progItem.at(4).toString()
                 << " bi1CoagModeRaw=" << progItem.at(7).toString()
                 << " mono1CutModeRaw=" << progItem.at(16).toString()
                 << " mono1CoagModeRaw=" << progItem.at(19).toString()
                 << " outMask=" << progItem.at(29).toInt();
        //ограничения для каждого листа
        if (progItem.at(1).toInt() < 0
                || progItem.at(1).toInt() >= 4) {
			qDebug() << "[ProgLoader] skipped row due to invalid Num="
			         << progItem.at(1).toInt()
			         << " rowId=" << progItem.at(0).toInt()
			         << " progIdField=" << progItem.at(2).toInt();
            continue;
        }
        //Шаг2---------------------------------------------------------
        std::vector<int> allowedModesId = getAllowedModes(progId, progItem);
        qDebug() << "[ProgLoader] row" << progItem.at(0).toInt()
                 << " allowedModesId.size=" << static_cast<int>(allowedModesId.size());

        //Шаг3---------------------------------------------------------
        std::vector<int> allowedInstrId = getAllowedInstrs(progId, progItem);
        qDebug() << "[ProgLoader] row" << progItem.at(0).toInt()
                 << " allowedInstrId.size=" << static_cast<int>(allowedInstrId.size());

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

            int monoPedSocket = progItem.at(27).toInt();
            if (i == monoPedSocket) {
                socket->setPedal(Onyx::SINGLE_PED);
            }
            int doublePedSocket = progItem.at(28).toInt();
            if (i == doublePedSocket) {
                socket->setPedal(Onyx::DOUBLE_PED);
            }

            bool allowSock = cutEna || coagEna;
            socket->setAllowed(allowSock);
            socket->setDisplayMode(Onyx::S_COLLAPSED);
        }
        instrMapVector.push_back(getInstrums());

        if (clear) {
            qDebug() << "[ProgLoader] clear=true, stop after first valid page";
            break;
        }
    }

    if (socketMapVector.empty())
	{
		qDebug() << "[ProgLoader] socketMapVector is empty after processing rows";
        return false;
	}

    m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, !clear);
	qDebug() << "[ProgLoader] loadProgs completed pages=" << socketMapVector.size();
	qDebug() << "[ProgLoader] model subProgCount=" << m_socketModelPtr->subProgCount()
	         << " subProgIdx=" << m_socketModelPtr->subProgIdx();

    if (progId != 1000) {
		qDebug() << "[ProgLoader] triggering slotSaveCurrentState after load";
        slotSaveCurrentState();
    }
    return (true);
}

std::map<int, QString> ProgLoader::getProgs(int scopeID)
{
	//захардкодили, но это нужно знать
	const std::unique_ptr<ProgLoaderBase> loader{getLoader(m_curLoaderType)};
	auto progs = loader->getPrograms(scopeID);
	qDebug() << "[ProgLoader] getProgs scopeID=" << scopeID
	         << " loaderType=" << m_curLoaderType
	         << " count=" << static_cast<int>(progs.size());
	return progs;
}

std::map<int, QString> ProgLoader::getCategories()
{
	const std::unique_ptr<ProgLoaderBase> loader{getLoader(m_curLoaderType)};
	auto scopes = loader->getCategories();
	qDebug() << "[ProgLoader] getCategories loaderType=" << m_curLoaderType
	         << " count=" << static_cast<int>(scopes.size());
	return scopes;
}

void ProgLoader::deleteUserProg(int id)
{
	const QString removeQuery = "DELETE FROM Lists WHERE Prog_ID = %1";
	m_dbReaderPtr->executeUpdateQuery(removeQuery.arg(id));

	const QString removeProgQuery = "DELETE FROM Progs WHERE id = %1";
	m_dbReaderPtr->executeUpdateQuery(removeProgQuery.arg(id));

}

void ProgLoader::renameUserProg(int id, const QString &name)
{
	const QString query = "UPDATE Progs "
	                      "SET "
	                      "Name_RU = %1,"
	                      "Name_ES = %1,"
	                      "Name_EN = %1 "
	                      "WHERE id = %2";

	m_dbReaderPtr->executeUpdateQuery(query.arg(name).arg(id));
}

void ProgLoader::deleteUserScope(int id)
{
    const QString removeScopeQuery = "DELETE FROM Scopes WHERE id = %1";
    m_dbReaderPtr->executeUpdateQuery(removeScopeQuery.arg(id));

    const QString queryCondition = "Scope_ID = %1";
    QList<QVariantList> progListVariant =
            m_dbReaderPtr->slotSendSelectQuery(QStringList{"Progs"},
                                              QStringList{"id"},
                                              queryCondition.arg(id));
    QStringList progIds;
    progIds.reserve(progListVariant.size());
    for (const auto& item : progListVariant) {
        progIds.append(QString::number(item.at(0).toInt()));
    }

    const QString removeProgsQuery = "DELETE FROM Progs WHERE Scope_ID = %1";
    m_dbReaderPtr->executeUpdateQuery(removeProgsQuery.arg(id));

    const QString removeListsQuery = "DELETE FROM Lists WHERE Prog_ID IN (%1)";
    if (!progIds.isEmpty()) {
        m_dbReaderPtr->executeUpdateQuery(removeListsQuery.arg(progIds.join(", ")));
    }

}

void ProgLoader::renameUserScope(int id, const QString &name)
{
    const QString query = "UPDATE Scopes "
                          "SET "
                          "Name_RU = %1,"
                          "Name_ES = %1,"
                          "Name_EN = %1 "
                          "WHERE id = %2";

    m_dbReaderPtr->executeUpdateQuery(query.arg(name).arg(id));
}

bool ProgLoader::freeSettingsSocketInit(bool clear)
{
    if (m_dbReaderPtr.isNull() || m_socketModelPtr.isNull()) {
        return false;
    }

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
                                                                        QStringList{"Name_RU","id"},
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
                        QStringList{"MaxPower","Name_RU", "id", "Num",
                                    "Brief_RU", "Descript_RU", "ENDO_REG"},
                        queryConditionModes
                                    .arg(socket->socketType() <= Onyx::BIPOLAR_2 ? 0 : 1)
                                    .arg(halfSocket));
            
            std::sort(modesList.begin(), modesList.end(),
                [](const QVariantList& a, const QVariantList& b) {
                    return a.at(3).toInt() < b.at(3).toInt();
                });
            
            std::vector<int> allModesId;
            allModesId.reserve(modesList.size());
            for (const auto& mode : modesList) {
                allModesId.push_back(mode.at(2).toInt());
            }
            
            std::map<int, std::map<int, Onyx::InstrInfo>> instrumConstraints = getConstraints(allModesId);
            
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
    
    m_socketModelPtr->loadProgs(socketMapVector, instrMapVector, !clear);
    m_currentLoadedProgId = -1;
    m_currentLoadedProgName.clear();
    m_currentLoadedScopeId = -1;
    return true;
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
	                                                                          QStringList{"id","Num","BI_MONO","Name_Ru","Brief_Ru"},
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
            || m_socketModelPtr->itemsMap() == nullptr) {
        // qWarning() << "Cannot save state: socket model not initialized";
        return;
    }

    int idToUse = -1;
    int biggestKnownId = -1;

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
        const   QString insertUserProgNameQuery = QString(
                                                      "INSERT INTO Scopes "
                                                      "(id, Num, Name_RU, Name_EN, Name_ES)"
                                                      " VALUES "
                                                      "(%1, %1, '%2', '%2', '%2')");
        int id = biggestKnownId > 1000 ? biggestKnownId + 1 : 1001;
        if (!m_dbReaderPtr->executeUpdateQuery(insertUserProgNameQuery
                                               .arg(id)
                                               .arg(scopeName))) {
            qDebug() << "Failed to add new SCOPE";
            return;
        } else {
            m_dbReaderPtr->commit();
            scopeId = id;
        }
    }

    const  QString insertUserProgNameQuery = QString(
                                                 "INSERT INTO Progs "
                                                 "(Prog_NUM, Argon, Name_RU, Name_EN, Name_ES, Scope_ID, Sub_NUM)"
                                                 " VALUES "
                                                 "(0, 0, '%1', '%1', '%1', %2, 0)");

    if (!m_dbReaderPtr->executeUpdateQuery(insertUserProgNameQuery
                                           .arg(progName)
                                           .arg(scopeId))) {
        qDebug() << "Failed to add Prog";
    } else {
        m_dbReaderPtr->commit();
    }

    QList<QVariantList> progListVariant = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Progs"},
                                                                             QStringList{"id", "Name_RU"},
                                                                             QString("Scope_ID = %1").arg(scopeId));
    if (progListVariant.isEmpty()) {
        qDebug() << "нету таких прог";
        return;
    }
    if (progListVariant.size() >= 1) {
        int min = INT_MIN;
        for (const auto& item : progListVariant) {
            if (item.at(1).toString() != progName)
                continue;
            if (item.at(0).toInt() > min) {
                min = item.at(0).toInt();
                idToUse = min;
            }
        }
    }
    if (idToUse == -1) {
        qDebug() << "idToUse == -1";
        return;
    }
    int pageCount = m_socketModelPtr->subProgCount();
    QList<QStringList> listsStr = prepSaveState();

    for (int i = 0; i < pageCount; ++i) {
        const auto& curList = listsStr.at(i);

        QString query = insertUserProgQuery
                        // QString query = saveCurQuery
                        .arg(curList.at(0)).arg(curList.at(1)).arg(curList.at(2)) //mono1 cut
                        .arg(curList.at(3)).arg(curList.at(4)).arg(curList.at(5)) //mono1 coag
                        .arg(curList.at(6)).arg(curList.at(7)).arg(curList.at(8)) //mono2 cut
                        .arg(curList.at(9)).arg(curList.at(10)).arg(curList.at(11)) //mono2 coag
                        .arg(curList.at(12)).arg(curList.at(13)).arg(curList.at(14)) //bi1 cut
                        .arg(curList.at(15)).arg(curList.at(16)).arg(curList.at(17)) //bi1 coag
                        .arg(curList.at(18)).arg(curList.at(19)).arg(curList.at(20)) //bi2 cut
                        .arg(curList.at(21)).arg(curList.at(22)).arg(curList.at(23)) //bi2 coag
                        .arg(curList.at(24)).arg(curList.at(25)).arg(curList.at(26)) //pedals and outmask
                        .arg(i + 1).arg(idToUse);

        if (!m_dbReaderPtr->executeUpdateQuery(query)) {
            // qDebug() << "Failed to save current state";
        }
    }
}

ProgLoaderBase *ProgLoader::getLoader(progType type)
{

	switch (type) {
	case ptRecom:
		return new RecomProgLoader();
	case ptUser:
		return new UserProgLoader();
	default:
		break;
	}
	return nullptr;
}

std::vector<int> ProgLoader::getAllowedInstrs(int progId, const QVariantList& progItem)
{
	std::vector<int> allowedInstrId;
	if (progId < 1000) {
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
	if (progId < 1000) {
		QList<QVariantList> allowedModes
		        = m_dbReaderPtr->slotSendSelectQuery(QStringList{"EnableModes"},
		                                             QStringList{"Mode_ID"},
		                                             QString("Prog_ID = %1").arg(progId));
		// QString("List_ID = %1").arg(progItem.at(0).toUInt()));
		allowedModesId.reserve(allowedModes.size());
		for (const auto& item : allowedModes) {
			allowedModesId.push_back(item.at(0).toInt());
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
	return allowedModesId;

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
	QMap<int, SurgModePtr> modes;
	QList<QVariantList> modesList = m_dbReaderPtr->slotSendSelectQuery(
	                                    QStringList{"Modes"},
	                                    QStringList{"MaxPower","Name_RU", "id", "Num",
	                                                "Brief_RU", "Descript_RU", "ENDO_REG"},
	                                    queryConditionModes
	                                    .arg(socket->socketType() <= Onyx::BIPOLAR_2 ? 0 : 1)
	                                    .arg(halfSocket)
	                                    .arg(makeCommaSeparatedNumbers(allowedModesId)));

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

	socket->setModeId(firstModeId, isCoag);
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
	const   QString insertUserProgNameQuery = QString(
	                                              "INSERT INTO Scopes ("
	                                              "id, Num, Name"
	                                              ") VALUES ("
	                                              "'%1', '%1', '%2')");

	int scopeId = -1;
	QVariantList scope;
	{
		QList<QVariantList> scopes = m_dbReaderPtr->slotSendSelectQuery(QStringList{"Scopes"},
		                                                                QStringList{"id", "Name"},
		                                                                "");
		int lastIndex = scopes.last().at(0).toInt();
		if (lastIndex < 1000) {
			scopeId = 1000;
		} else {
			scopeId = lastIndex + 1;
		}
	}
	if (scopeId == -1) {
		return scopeId;
	}
	if (!m_dbReaderPtr->executeUpdateQuery(insertUserProgNameQuery.arg(scopeId).arg(name))) {
		qDebug() << "Failed to save new scope";
		return scopeId;
	}
	m_dbReaderPtr->commit();
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

