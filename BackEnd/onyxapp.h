#ifndef ONYXAPP_H
#define ONYXAPP_H

#include <QGuiApplication>
#include <QObject>
#include <QSharedPointer>

#include "databasereader.h"

class OnyxApp : public QGuiApplication
{
	Q_OBJECT
public:
	OnyxApp(int &argc, char **argv, int flags = ApplicationFlags);
	/// Каталог (eshfDb.db), только чтение.
	QSharedPointer<DataBaseReader> getDbReader();
	/// Пользовательские Scopes / Progs / Lists (userProg.db).
	QSharedPointer<DataBaseReader> getUserProgDbReader();
	/// Ленивое создание читателей БД (до первого обращения можно не трогать диск).
	void ensureDatabases();

private:
	QSharedPointer<DataBaseReader> m_dbReader;
	QSharedPointer<DataBaseReader> m_userProgDbReader;
};

#endif // ONYXAPP_H
