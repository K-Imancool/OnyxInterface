#include "onyxapp.h"
#include "apppaths.h"

#include <QStyleHints>

OnyxApp::OnyxApp(int &argc, char **argv, int flags) :
QGuiApplication(argc, argv, flags)
{
    const AppPaths &paths = AppPaths::instance();
    m_dbReader = QSharedPointer<DataBaseReader>::create(
        paths.eshfDbPath(),
        QStringLiteral("eshfCatalog"),
        true);

    m_userProgDbReader = QSharedPointer<DataBaseReader>::create(
        paths.userProgDbPath(),
        QStringLiteral("userProgStore"),
        false);

    // Возвращаем исходные настройки styleHints, как было до отладки
    styleHints()->setMousePressAndHoldInterval(1200);
    styleHints()->setMouseDoubleClickInterval(400);
}

QSharedPointer<DataBaseReader> OnyxApp::getDbReader()
{
    return m_dbReader;
}

QSharedPointer<DataBaseReader> OnyxApp::getUserProgDbReader()
{
    return m_userProgDbReader;
}
