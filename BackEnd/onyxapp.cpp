#include "onyxapp.h"
#include "apppaths.h"

#include <QStyleHints>

OnyxApp::OnyxApp(int &argc, char **argv, int flags) :
QGuiApplication(argc, argv, flags)
{
    // Возвращаем исходные настройки styleHints, как было до отладки
    styleHints()->setMousePressAndHoldInterval(1200);
    styleHints()->setMouseDoubleClickInterval(400);
}

void OnyxApp::ensureDatabases()
{
    if (!m_dbReader.isNull() && !m_userProgDbReader.isNull()) {
        return;
    }

    const AppPaths &paths = AppPaths::instance();
    if (m_dbReader.isNull()) {
        m_dbReader = QSharedPointer<DataBaseReader>::create(
            paths.eshfDbPath(),
            QStringLiteral("eshfCatalog"),
            true);
    }
    if (m_userProgDbReader.isNull()) {
        m_userProgDbReader = QSharedPointer<DataBaseReader>::create(
            paths.userProgDbPath(),
            QStringLiteral("userProgStore"),
            false);
    }
}

QSharedPointer<DataBaseReader> OnyxApp::getDbReader()
{
    ensureDatabases();
    return m_dbReader;
}

QSharedPointer<DataBaseReader> OnyxApp::getUserProgDbReader()
{
    ensureDatabases();
    return m_userProgDbReader;
}
