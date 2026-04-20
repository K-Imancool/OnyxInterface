#ifndef PROGLOADERBASE_H
#define PROGLOADERBASE_H

#include "BackEnd/databasereader.h"
#include <QObject>
#include <QSharedPointer>
#include <map>
#include <QString>


class ProgLoaderBase : public QObject
{
	Q_OBJECT
public:
	explicit ProgLoaderBase(QObject *parent = nullptr)
	    : QObject{parent} {};

	virtual std::map<int, QString> getPrograms(int scopeID) = 0;
	virtual std::map<int, QString> getCategories() = 0;
	virtual int type() = 0;
	// virtual void deleteProg(int id) = 0;

signals:

protected:
	QSharedPointer<DataBaseReader> m_dbReader;

};

#endif // PROGLOADERBASE_H
