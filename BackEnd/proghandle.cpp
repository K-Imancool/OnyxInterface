#include <QDebug>
#include "proghandle.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QStringList>

#include "proghandle.h"

namespace {
template <typename T, typename P>
QList<T> keys(const std::map<T, P>& map) {
	QList<T> res;
	res.reserve(map.size());
	for (const auto&  item: map) {
		res.push_back(item.first);
	}
	return res;
}
template <typename T, typename P>
QList<P> values(const std::map<T, P>& map) {
	QList<P> res;
	res.reserve(map.size());
	for (const auto&  item: map) {
		res.push_back(item.second);
	}
	return res;
}
}

ProgHandle::ProgHandle(QObject *parent)
    : QObject{parent}
{
	connect(this, &ProgHandle::isRecomProgsChanged,
	        this, [this] () {
		emit updateScopes(m_isRecomProgs);

	});
}

void ProgHandle::loadRecommendedProg(int recomProgIdx, bool clear)
{
	if (recomProgIdx < 0 || recomProgIdx >= m_progs.size()) {
		return;
	}
	auto iter = m_progs.begin();
	for (int a = 0; a < recomProgIdx; a++) {
		++iter;
	}
	emit signalRecomProgChosen(iter->first, clear);
}

void ProgHandle::notifyEndoProgramMixRejected()
{
	emit endoProgramMixRejected();
}

void ProgHandle::loadFreeSettings()
{
    emit signalFreeSettingsRequested();
}

void ProgHandle::loadEmptyFreeSettings()
{
    emit signalEmptyFreeSettingsRequested();
}

void ProgHandle::loadLastSettings()
{
    emit signalLastSettingsRequested();
}

void ProgHandle::saveCurrentState()
{
    emit signalSaveCurrentStateRequested();
}

void ProgHandle::deleteAllUserProgs()
{
    emit signalDeleteAllUserProgs();
}

void ProgHandle::removeSubProg()
{
	emit signalRemoveSub();
}

void ProgHandle::saveProg(const QString &scopeName, const QString &progName)
{
	emit signalSaveName(scopeName, progName);
}

bool ProgHandle::userProgExists(const QString &scopeName, const QString &progName)
{
	bool result = false;
	emit userProgExistsRequested(scopeName, progName, &result);
	return result;
}

void ProgHandle::addEmptyDefault()
{
	emit signalAddEmptyDefault(false);
}

void ProgHandle::copyCurrent()
{
	emit signalCopyCurrent();
}

void ProgHandle::deleteProgRequest(int index)
{
	if (m_isRecomProgs) {
		return;
	}
	if (index < 0 || m_progs.size() <= index) {
		return;
	}
	auto iter = m_progs.begin();
	for (int i = 0; i < index; ++i) {
		iter++;
	}
	int idToDelete = iter->first;
	emit signalDeleteProg(idToDelete);
}

void ProgHandle::renameProgRequest(int index, const QString &name)
{
	if (m_isRecomProgs) {
		qWarning() << "renameProgRequest skipped: current list is recommended";
		return;
	}
	if (index < 0 || m_progs.size() <= index) {
		qWarning() << "renameProgRequest skipped: index out of range" << index
		           << "size" << static_cast<int>(m_progs.size());
		return;
	}
	auto iter = m_progs.begin();
	for (int i = 0; i < index; ++i) {
		iter++;
	}
	int idToRename = iter->first;
	emit signalRenameProg(idToRename, name);
}

void ProgHandle::permitAll()
{
	emit signalUnlockProg();
}

QStringList ProgHandle::scopeNameList() const
{
	return values(m_scopes);
}

QStringList ProgHandle::progNameList() const
{
	return values(m_progs);
}

QList<int> ProgHandle::scopeIdList() const
{
	return keys(m_scopes);
}

QList<int> ProgHandle::progIdList() const
{
	const auto& progs = m_progs;
	QList<int> res;
	res.reserve(progs.size());
	for (const auto& item : progs) {
		res.append(item.first);
	}
	return res;
}

int ProgHandle::scopeIdx() const
{
	return m_scopeIdx;
}

void ProgHandle::setScopeIdx(int newScopeIdx)
{
	if (m_scopes.size() <= newScopeIdx)
		return;
	// if (m_scopeIdx == newScopeIdx)
	//     return;
	m_scopeIdx = newScopeIdx;
	auto iter = m_scopes.begin();
	for (int i = 0; i < newScopeIdx; ++i) {
		iter++;
	}
	// iter += newScopeIdx;
	emit signalScopeRequest(iter->first);
	emit scopeIdxChanged();
}


void ProgHandle::setProgList(const std::map<int, QString>& lst/*, bool isRecom*/)
{
//	qDebug() << "[ProgFlow] setProgList size:" << lst.size()
//	         << "isRecomProgs:" << m_isRecomProgs;
	if (!lst.empty()) {
		auto it = lst.cbegin();
//		qDebug() << "[ProgFlow] setProgList first entry id:" << it->first << "name:" << it->second;
	}
	m_progs = lst;
}

void ProgHandle::setProgSubLists(const QList<QList<QPair<int, QString>>>& subLists)
{
	m_progSubLists = subLists;
	while (m_progSubLists.size() < static_cast<int>(m_progs.size())) {
		m_progSubLists.append(QList<QPair<int, QString>>());
	}
	emit progNameListChanged();
}

bool ProgHandle::hasSubPrograms(int index) const
{
	return index >= 0
	        && index < m_progSubLists.size()
	        && !m_progSubLists.at(index).isEmpty();
}

QVariantList ProgHandle::subProgramsAt(int index) const
{
	QVariantList result;
	if (index < 0 || index >= m_progSubLists.size()) {
		return result;
	}

	for (const auto& subProgram : m_progSubLists.at(index)) {
		QVariantMap entry;
		entry.insert(QStringLiteral("id"), subProgram.first);
		entry.insert(QStringLiteral("name"), subProgram.second);
		result.append(entry);
	}
	return result;
}

void ProgHandle::setScopeList(const std::map<int, QString> &scopes/*, bool isRecom*/)
{
//	qDebug() << "[ProgFlow] setScopeList scopes:" << scopes.size();
	int previousScopeId = -1;
	if (!m_scopes.empty() && m_scopeIdx >= 0 && static_cast<size_t>(m_scopeIdx) < m_scopes.size()) {
		auto iter = m_scopes.begin();
		for (int i = 0; i < m_scopeIdx; ++i) {
			++iter;
		}
		previousScopeId = iter->first;
	}

	const int previousScopeIdx = m_scopeIdx;
	m_scopes = scopes;
	emit scopeNameListChanged();

	if (m_scopes.empty()) {
		m_scopeIdx = 0;
		m_progs.clear();
		m_progSubLists.clear();
		emit progNameListChanged();
		emit scopeIdxChanged();
		return;
	}

	int restoredScopeIdx = -1;
	if (previousScopeId >= 0) {
		int idx = 0;
		for (auto iter = m_scopes.begin(); iter != m_scopes.end(); ++iter, ++idx) {
			if (iter->first == previousScopeId) {
				restoredScopeIdx = idx;
				break;
			}
		}
	}

	if (restoredScopeIdx < 0) {
		restoredScopeIdx = previousScopeIdx;
		if (restoredScopeIdx < 0) {
			restoredScopeIdx = 0;
		} else if (static_cast<size_t>(restoredScopeIdx) >= m_scopes.size()) {
			restoredScopeIdx = static_cast<int>(m_scopes.size()) - 1;
		}
	}

	setScopeIdx(restoredScopeIdx);
}

bool ProgHandle::isRecomProgs() const
{
	return m_isRecomProgs;
}

void ProgHandle::setIsRecomProgs(bool newIsRecomProgs)
{
	m_isRecomProgs = newIsRecomProgs;
	emit isRecomProgsChanged();
}

void ProgHandle::deleteScopeRequest(int index)
{
	if (m_isRecomProgs) {
		return;
	}
	if (index < 0 || m_scopes.size() <= index) {
		return;
	}
	auto iter = m_scopes.begin();
	for (int i = 0; i < index; ++i) {
		iter++;
	}
	int idToDelete = iter->first;
	emit signalDeleteScope(idToDelete);
}

void ProgHandle::renameScopeRequest(int index, const QString &name)
{
	if (m_isRecomProgs) {
		qWarning() << "renameScopeRequest skipped: current list is recommended";
		return;
	}
	if (index < 0 || m_scopes.size() <= index) {
		qWarning() << "renameScopeRequest skipped: index out of range" << index
		           << "size" << static_cast<int>(m_scopes.size());
		return;
	}
	auto iter = m_scopes.begin();
	for (int i = 0; i < index; ++i) {
		iter++;
	}
	int idToRename = iter->first;
	qWarning() << "renameScopeRequest emit signalRenameScope id" << idToRename
	           << "newName" << name;
	emit signalRenameScope(idToRename, name);
}

QString ProgHandle::readTextFile(const QString& filePath)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qWarning() << "ProgHandle: Cannot open file:" << filePath;
		return QString();
	}

	QTextStream in(&file);
	QString content = in.readAll();
	file.close();

	return content;
}

QStringList ProgHandle::scanVideoFiles(const QString& folderPath)
{
	///whyyyy on earth VIDEO got in PROG loader?
	///
	QDir dir(folderPath);
	if (!dir.exists()) {
		// qWarning() << "ProgHandle: Video folder does not exist:" << folderPath;
		return QStringList();
	}

	// Поддерживаемые форматы видео
	QStringList filters;
	filters << "*.mp4" << "*.MP4"
	        << "*.avi" << "*.AVI"
	        << "*.mkv" << "*.MKV"
	        << "*.mov" << "*.MOV"
	        << "*.wmv" << "*.WMV"
	        << "*.flv" << "*.FLV"
	        << "*.webm" << "*.WEBM"
	        << "*.m4v" << "*.M4V"
	        << "*.mpeg" << "*.MPEG"
	        << "*.mpg" << "*.MPG";

	dir.setNameFilters(filters);
	dir.setFilter(QDir::Files | QDir::Readable);
	dir.setSorting(QDir::Name);

	QStringList videoFiles = dir.entryList();

	// qDebug() << "ProgHandle: Found" << videoFiles.size() << "video files in" << folderPath;

	return videoFiles;
}


