#include "EshfProgStringBuilder.h"

template<typename Key, typename Value>
void uniteMaps(
    std::map<Key, Value>& destination,
    const std::map<Key, Value>& source) {
	for (const auto& [key, value] : source) {
		auto it = destination.find(key);
		if (it != destination.end()) {
			continue;
		} else {
			destination[key] = value;
		}
	}
}

EshfProgStringBuilder::EshfProgStringBuilder()
{

}

QStringList EshfProgStringBuilder::makeSocketString(size_t socketNum, size_t pageNum)
{
	QStringList res;
	if (m_sockets.empty()) {
		return res;
	}
	if (socketNum > 3) {
		return res;
	}
	if (pageNum >= m_sockets.size()) {
		return res;
	}
	// for (int i = 0; i < 4; ++i) {
	    SockPtr sock = m_sockets.at(pageNum).at(socketNum);
		res.append(makeInstrumString(sock, false));
		res.append(makeModeString(sock, false));
		res.append(makePowerString(sock, false));
		res.append(makeInstrumString(sock, true));
		res.append(makeModeString(sock, true));
		res.append(makePowerString(sock, true));
	// }

	return res;
}

QStringList EshfProgStringBuilder::makePedalString(size_t pageNum)
{
	QStringList res;
	if (m_sockets.empty()) {
		return res;
	}
	if (pageNum >= m_sockets.size()) {
		return res;
	}
	// Lists: Pedal_1 — одноклавишная, Pedal_2 — двухклавишная; 0 — педаль не назначена; 1..4 — номер сокета.
	res.append(QStringLiteral("0"));
	res.append(QStringLiteral("0"));
	const auto& page = m_sockets.at(pageNum);
	for (int i = 0; i < 4; ++i) {
		if (page.at(i)->pedal() == Onyx::SINGLE_PED) {
			res[0] = QString::number(i + 1);
		}
		if (page.at(i)->pedal() == Onyx::DOUBLE_PED) {
			res[1] = QString::number(i + 1);
		}
	}
	return res;
}

QString EshfProgStringBuilder::makeOutEnableMask(size_t pageNum)
{
	//заглушка
	return QString::number(11111111);
}

void EshfProgStringBuilder::setState(const std::vector<std::map<int, SockPtr> >& sockets,
                                    const std::vector<std::map<int, InstrPtr> >& instrs)
{
	if (sockets.size() != instrs.size()) {
		return;
	}
	m_instrs = instrs;
	m_sockets = sockets;
}

QString EshfProgStringBuilder::makeModeString(SockPtr sock, bool isCoag)
{
	QStringList modes = isCoag ? sock->coagModeNamesIds() : sock->cutModeNamesIds();
	if (modes.empty()) {
		//ветка где режимов нет
	} else {
		const int selectedModeId = isCoag ? sock->coagModeId() : sock->cutModeId();
		const QString selectedModeIdStr = QString::number(selectedModeId);
		const int selectedPos = modes.indexOf(selectedModeIdStr);
		if (selectedPos >= 0) {
			modes.prepend(modes.takeAt(selectedPos));
		} else if (selectedModeId > 0 && selectedModeId != ESHF::NO_MODE) {
			// Если выбранный режим не попал в список (редкий кейс несогласованности),
			// явно фиксируем его первым, чтобы при загрузке не сместился выбор.
			modes.prepend(selectedModeIdStr);
		}
	}
	//положительная ветка, когда есть режимы и выбран режим
	// modes.swapItemsAt(0, isCoag ? sock->coagModeIndex() : sock->cutModeIndex());
	return (modes.join(','));

}

QString EshfProgStringBuilder::makeInstrumString(SockPtr sock, bool isCoag)
{
	QStringList res;
	CSurgModePtr mode = isCoag ? sock->curCoagMode() : sock->curCutMode();
	if (mode.isNull()) {
		return "";
	}

	std::map<int, Onyx::InstrInfo> all;

	int lim = isCoag ? sock->coagModeCount() : sock->cutModeCount();
	for (int mode = 0; mode < lim; ++mode) {
		const auto& modePtr = sock->getMode(mode, isCoag);
		uniteMaps(all, modePtr->InstrConstraints() );
	}
	for (const auto& [key, item] : all) {
		res.append(QString::number(item.id));
	}
	res.removeAll(QString::number(-1));

	int instrId = isCoag ? mode->selectedInstrId()
	                    : mode->selectedInstrId();
	res.removeAll(QString::number(instrId));

	if (instrId >= 0) {
		res.prepend(QString::number(instrId));
	}

	return (res.join(','));
}

QString EshfProgStringBuilder::makePowerString(SockPtr sock, bool isCoag)
{
	return QString::number(isCoag ? sock->coagModePower() : sock->cutModePower());
}
