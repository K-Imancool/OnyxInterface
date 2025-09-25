#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "SettingsScreen/wifimodule/NetworkDiscover.h"
#include "SettingsScreen/updatemodule/updateclient.h"
#include "BackEnd/controlcenter.h"
#include "BackEnd/instrimageprovider.h"
#include "qqmlcontext.h"

#include <QFile>
#include <QDir>
#include <QLoggingCategory>
#include <QScopedPointer>
#include <QDateTime>
#include <QTextStream>
#include "BackEnd/loggingcategories.h"
#include "BackEnd/linkstm.h"
#include "BackEnd/jsonstorage.h"

// Умный указатель на файл логирования
QScopedPointer<QFile>   m_logFile;

// Класс для связи с stm по uart
LinkStm* m_linkStm;

// Класс для сохранения всяких настроечных штук
JsonStorage* m_savedJson;

// Объявляение обработчика для логов
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);


int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    ///Добавляем модуль клавиатуры
    qputenv("QT_IM_MODULE", QByteArray("cutekeyboard"));

    QGuiApplication app(argc, argv);


    // Устанавливаем файл логирования,
    // внимательно сверьтесь с тем, какой используете путь для файла
    m_logFile.reset(new QFile("/home/kikorik/OnyxLog/logFile.txt"));
    // Открываем файл логирования
    m_logFile.data()->open(QFile::Append | QFile::Text);
    // Устанавливаем обработчик
    qInstallMessageHandler(messageHandler);
    qInfo(logInfo()) << "Включение";


    NetworkControl::registerNetworkControl();
    UpdateClient::registerUpdateClient();
    ControlCenter::registerControl();

    QSharedPointer<ControlCenter> ctrl  = QSharedPointer<ControlCenter>::create(nullptr);
    ctrl->init();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theModel", ctrl->getSocketModel());
    engine.rootContext()->setContextProperty("Editor", ctrl->editor());
    engine.rootContext()->setContextProperty("recomHandle", ctrl->getHandle());
    engine.addImageProvider(QLatin1String("instrums"), new InstrImageProvider);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    //этот вызов для загрузки элемента pullToRefresshHandler
    engine.addImportPath("qrc:/");
    engine.load(url);

    // Сохраняемые значения лежат в json-файле
    QVariantMap* initMap = new QVariantMap();
    initMap->insert("boot", 0);
    m_savedJson = new JsonStorage(nullptr, initMap);
    QJsonValue boot;
    m_savedJson->read("boot", &boot);

    // Класс для связи с stm по uart
    m_linkStm = new LinkStm();
    qDebug(logInfo()) << "new LinkStm";
    // Откуда грузиться stm
    m_linkStm->setBoot(static_cast<LinkStm::BootChoice>(boot.toInt()));
    // Привязываем сигналы
//    QObject::connect(m_linkStm, &LinkStm::recieveData, this, &MainWindow::testDisplay);
//    QObject::connect(m_linkStm, &LinkStm::error, this, &MainWindow::displayUartError);

    return app.exec();
}

// Реализация обработчика
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (type == QtCriticalMsg || type == QtWarningMsg) {
        if (msg.startsWith("Failed to move cursor") ||
            msg.startsWith("Could not set cursor") ||
            msg.startsWith("Could not set DRM") ||
            msg.startsWith("Could not queue DRM")) return;    // фильтруем ворнинги, чтобы не забивать лог
    }

    // Открываем поток записи в файл
    QTextStream out(m_logFile.data());
    // Записываем дату записи
    out << QDateTime::currentDateTime().toString("dd-MM-yyyy hh:mm:ss.zzz ");
    // По типу определяем, к какому уровню относится сообщение
    switch (type) {
    case QtInfoMsg:     out << "INF "; break;
    case QtDebugMsg:    out << "DBG "; break;
    case QtWarningMsg:  out << "WRN "; break;
    case QtCriticalMsg: out << "CRT "; break;
    case QtFatalMsg:    out << "FTL "; break;
    default: break;
    }
    // Записываем в вывод категорию сообщения и само сообщение
    if (type != QtDebugMsg) {
        out << context.category << ": "
            << msg << Qt::endl;
        out.flush();    // Очищаем буферизированные данные
    }

    // То же самое выводим в консоль
    QTextStream debugOut(stdout);
    debugOut << context.category << ": "
        << msg << Qt::endl;
    debugOut.flush();    // Очищаем буферизированные данные
}
