#pragma once

#include "socket.h"
#include "instrument.h"
// #include "socketmodel.h"
#include <QString>
#include <vector>
#include <map>

/**
 * @brief The EshfProgStringBuilder class обеспечивает получение пригодных
 * для базы данных строк, описывающих набор сокетов
 * @details Задумка в том, чтобы не дёргать ради сохранения модель, висящую в гуи-потоке.
 * таким образом можно будет вообще сохранение в отдельный поток откинуть
 */
class EshfProgStringBuilder
{
public:
	EshfProgStringBuilder();
	QStringList makeSocketString(size_t socketNum, size_t pageNum);
	QStringList makePedalString(size_t pageNum);
	QString makeOutEnableMask(size_t pageNum);
	void setState(const std::vector<std::map<int, SockPtr> > &sockets,
	              const std::vector<std::map<int, InstrPtr> > &instrs);
private:
	QString makeModeString(SockPtr sock, bool isCoag);
	QString makeInstrumString(SockPtr sock, /*InstrPtr inst,*/ bool isCoag);
	QString makePowerString(SockPtr sock, bool isCoag);

	std::vector<std::map<int, SockPtr>> m_sockets;
	std::vector<std::map<int, InstrPtr>> m_instrs;
};

// #endif // SOCKETREADER_H
