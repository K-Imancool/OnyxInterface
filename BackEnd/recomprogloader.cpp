#include "recomprogloader.h"
#include "onyxapp.h"


RecomProgLoader::RecomProgLoader(QObject *parent)
    : ProgLoaderBase{parent}
{
	if (m_dbReader.isNull()) {
		OnyxApp* app = dynamic_cast<OnyxApp*>(qApp);
		if (app) {
			m_dbReader = app->getDbReader();
		}
	}
}

std::map<int, QString> RecomProgLoader::getPrograms(int scopeID)
{
	//захардкодили, но это нужно знать
	bool isMyselfArgon = false;

	QString queryCondition = "Scope_ID = %1 AND (Argon = 0 OR Argon = %2)";

	QList<QVariantList> progListVariant = m_dbReader->slotSendSelectQuery(QStringList{"Progs"},
	                                                                      QStringList{"Name_RU","id", "Prog_NUM", "Subprog_RU"},
	                                                                      queryCondition.arg(scopeID).arg(isMyselfArgon ? 2 : 1));

	std::map<int, QString> progList;
	for (const auto& item : progListVariant) {
		// Prog tmp;
		bool isMainProg = item.at(2).toInt() % 10 == 0 ? true : false;
		int id = item.at(1).toInt();
		QString name = item.at(isMainProg ? 0 : 3).toString();
		progList.insert_or_assign(id, name);
	}
	return progList;
}

std::map<int, QString> RecomProgLoader::getCategories()
{
	QList<QVariantList> scopeListVariant
	        = m_dbReader->slotSendSelectQuery(QStringList{"Scopes"},
	                                          QStringList{"id", "Name_RU"},
	                                          "id < 1000");

	std::map<int, QString> scopeList;
	for (const auto& item : scopeListVariant) {
		bool ok;
		int id = item.at(0).toInt(&ok);
		QString name = item.at(1).toString();
		if (!ok) {
			continue;
		}
		scopeList.insert_or_assign(id, name);
	}
	return scopeList;
}
