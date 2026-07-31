#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "SettingsScreen/wifimodule/NetworkDiscover.h"
#include "SettingsScreen/updatemodule/updateclient.h"
#include "SettingsScreen/updatemodule/remoteupdater.h"
#include "BackEnd/controlcenter.h"
#include "BackEnd/instrimageprovider.h"
#include "BackEnd/systemmonitor.h"
#include "BackEnd/keygenerator.h"
#include "BackEnd/onyxapp.h"
#include "qqmlcontext.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QResource>
#include <QSaveFile>
#include <QScopedPointer>
#include <QDateTime>
#include <QTextStream>
#include <QThread>
#include <QProcess>
#include "BackEnd/loggingcategories.h"
#include "BackEnd/linkstm.h"
#include "BackEnd/jsonstorage.h"
#include "BackEnd/HttpUploadController.h"
#include "BackEnd/McFirmwareVersionsBridge.h"
#include "BackEnd/DeviceLogManager.h"
#include "BackEnd/UpdateLogManager.h"
#include "BackEnd/datetimecontroller.h"
#include "BackEnd/translationcontroller.h"
#include "BackEnd/featureunlockcontroller.h"
#include "BackEnd/apppaths.h"
#include "BackEnd/gstreamervideoplayer.h"
#include "appversion.h"

#include <gst/gst.h>

#include <cstdlib>

// Умный указатель на файл логирования
QScopedPointer<QFile>   m_logFile;

// Класс для связи с stm по uart
LinkStm* m_linkStm;

// Класс для сохранения всяких настроечных штук
JsonStorage* m_savedJson;

// Объявляение обработчика для логов
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

QString extractBundledQmlGlPlugin()
{
    QFile resource(QStringLiteral(":/gstreamer/libgstqmlgl.so"));
    if (!resource.open(QIODevice::ReadOnly)) {
        qWarning() << "Bundled qmlglsink resource is unavailable";
        return QString();
    }

    const QString pluginDir = QDir::tempPath()
            + QStringLiteral("/qtpr-gstreamer");
    if (!QDir().mkpath(pluginDir)) {
        qWarning() << "Cannot create bundled GStreamer plugin directory:"
                   << pluginDir;
        return QString();
    }

    const QString pluginPath = pluginDir
            + QStringLiteral("/libgstqmlgl.so");
    QSaveFile output(pluginPath);
    if (!output.open(QIODevice::WriteOnly)
            || output.write(resource.readAll()) < 0
            || !output.commit()) {
        qWarning() << "Cannot extract bundled qmlglsink to:" << pluginPath;
        return QString();
    }

    QFile::setPermissions(
                pluginPath,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner
                | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                | QFileDevice::ExeGroup | QFileDevice::ReadOther
                | QFileDevice::ExeOther);
    return pluginPath;
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    // Тач → мышь для MouseArea и прочих QML-элементов без своего touch-обработчика.
    // false ломает касания: SButton/поля ввода работают, MouseArea — нет.
    // Но это если в дисплее прошивка как мультитач, если он как мышь, то false, может быть и лучше
    QCoreApplication::setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, true);

    // Настройки для Qt Multimedia
    qputenv("QT_GSTREAMER_USE_PLAYBIN_VOLUME", "1");
    // qputenv("GST_DEBUG", "1");  // Минимальная отладка (1=ERROR, 2=WARNING, 3=INFO)
    qputenv("QT_MULTIMEDIA_PREFERRED_PLUGINS", "gstreamer");
    
    // Настройка приоритетов декодеров H.264
    // В системе доступен только openh264dec (avdec_h264 отсутствует)
    // Для установки avdec_h264: sudo apt install gstreamer1.0-libav
    // v4l2slh264dec - аппаратный (не работает стабильно)
    // qputenv("GST_PLUGIN_FEATURE_RANK", "v4l2slh264dec:NONE");
    
    // Используем ximagesink для вывода видео с отключенной синхронизацией
    // sync=false позволяет не сбрасывать буферы при отставании
    // qputenv("QT_GSTREAMER_VIDEOSINK", "ximagesink");
    // qputenv("QT_GSTREAMER_CAMERABIN_VIDEOSINK", "ximagesink");
    
    // Настройки буферизации и обработки кадров
    qputenv("GST_BUFFER_DURATION", "1000000000");  // 1 секунда буферизации (в наносекундах)

    // qmlglsink должен использовать тот же EGL/OpenGL ES контекст, что и Qt Quick.
    if (qEnvironmentVariableIsEmpty("GST_GL_PLATFORM")) {
        qputenv("GST_GL_PLATFORM", "egl");
    }
    if (qEnvironmentVariableIsEmpty("GST_GL_API")) {
        qputenv("GST_GL_API", "gles2");
    }
    
    // Настройки аудио - используем PulseAudio с явным указанием устройства
    // Доступные устройства (pactl list sinks):
    // - alsa_output.platform-hdmi-sound.stereo-fallback (HDMI)
    // - alsa_output.platform-rk809-sound.stereo-fallback (Analog/наушники) ✓
    // qputenv("QT_GSTREAMER_PLAYBIN_AUDIOSINK", "pulsesink");
    
    // Указываем использовать аналоговый выход (RK809) вместо HDMI
    // qputenv("PULSE_SINK", "alsa_output.platform-rk809-sound.stereo-fallback");
    
    // Переключаем RK809 на динамик (SPK) вместо наушников (HP)
    // Playback Mux: 0=HP (наушники), 1=SPK (динамик через GPIO)
    // QProcess::execute("amixer", QStringList() << "-c" << "0" << "cset" << "numid=4" << "0");
    
    // Включаем аудио в playbin
    // qputenv("QT_GSTREAMER_PLAYBIN_FLAGS", "audio+video+soft-colorbalance+soft-volume");
    
    ///Добавляем модуль клавиатуры
    qputenv("QT_IM_MODULE", QByteArray("cutekeyboard"));
    ///Отключаем курсор мыши на embedded-системе
    // qputenv("QT_QPA_EGLFS_HIDECURSOR", "1");
    
    // Оптимизация производительности QML
    qputenv("QSG_RENDER_LOOP", "threaded");
    qputenv("QML_DISABLE_DISK_CACHE", "0");
    
    // Настройки для touch-устройств (как было до отладки)
    qputenv("QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS", "rotate=0");
    // Не отключаем *.debug — иначе qDebug() не вызывает messageHandler и не виден в SSH
    qputenv("QT_LOGGING_RULES", "qt.qpa.input=false");

    Q_INIT_RESOURCE(backend);
    const QString bundledQmlGlPath = extractBundledQmlGlPlugin();
    if (!bundledQmlGlPath.isEmpty()) {
        QByteArray pluginPath = QFileInfo(bundledQmlGlPath)
                .absolutePath().toLocal8Bit();
        const QByteArray existingPluginPath =
                qgetenv("GST_PLUGIN_PATH_1_0");
        if (!existingPluginPath.isEmpty()) {
            pluginPath += ':';
            pluginPath += existingPluginPath;
        }
        qputenv("GST_PLUGIN_PATH_1_0", pluginPath);
        qputenv("GST_REGISTRY", QFileInfo(bundledQmlGlPath)
                .absolutePath().append(QStringLiteral("/registry.bin"))
                .toLocal8Bit());
    }

    AppPaths::initializeFromArgs(argc, argv);

    GError *gstError = nullptr;
    if (!gst_init_check(&argc, &argv, &gstError)) {
        qCritical() << "GStreamer initialization failed:"
                    << (gstError ? gstError->message : "unknown error");
        if (gstError) {
            g_error_free(gstError);
        }
        return EXIT_FAILURE;
    }

    OnyxApp app(argc, argv);
    QCoreApplication::setApplicationVersion(kOnyxAppVersion);

    // Устанавливаем кастомный обработчик для вывода только имени файла (без пути)
    qInstallMessageHandler(messageHandler);

    // Системный qmlglsink может быть собран против несовместимого системного Qt.
    // Сначала пробуем плагин, собранный против Qt приложения и установленный рядом.
    if (!bundledQmlGlPath.isEmpty()) {
        GError *pluginError = nullptr;
        GstPlugin *plugin = gst_plugin_load_file(
                    bundledQmlGlPath.toLocal8Bit().constData(), &pluginError);
        if (plugin) {
            gst_object_unref(plugin);
        } else {
            qWarning() << "Bundled qmlglsink failed to load:"
                       << (pluginError ? pluginError->message : "unknown error");
        }
        if (pluginError) {
            g_error_free(pluginError);
        }
    }

    // Загрузка qmlglsink регистрирует GstGLVideoItem до разбора QML.
    GstElement *qmlGlSinkProbe =
            gst_element_factory_make("qmlglsink", "qml-registration-probe");
    bool qmlGlAvailable = false;
    if (qmlGlSinkProbe) {
        GstElementFactory *factory = gst_element_get_factory(qmlGlSinkProbe);
        GstPlugin *plugin = factory
                ? gst_plugin_feature_get_plugin(GST_PLUGIN_FEATURE(factory))
                : nullptr;
        const QString loadedPluginPath = plugin && gst_plugin_get_filename(plugin)
                ? QFileInfo(QString::fromLocal8Bit(
                                gst_plugin_get_filename(plugin)))
                  .canonicalFilePath()
                : QString();
        qmlGlAvailable = !bundledQmlGlPath.isEmpty()
                && loadedPluginPath
                == QFileInfo(bundledQmlGlPath).canonicalFilePath();

        if (qmlGlAvailable) {
            qInfo() << "Using bundled qmlglsink:" << loadedPluginPath;
        } else {
            qWarning() << "Refusing incompatible qmlglsink:"
                       << loadedPluginPath;
        }

        if (plugin) {
            gst_object_unref(plugin);
        }
        gst_object_unref(qmlGlSinkProbe);
    }
    if (!qmlGlAvailable) {
        qWarning() << "GStreamer qmlglsink plugin is unavailable;"
                      " video player is disabled";
    }

    // Включаем QML debugger для удаленной отладки
    // Использование: приложение -qmljsdebugger=port:3768,block
    // Или через переменную окружения: QT_QML_DEBUG=1
    // Для удаленной отладки: -qmljsdebugger=port:3768,host:IP_АДРЕС_ХОСТА
    // Для локальной отладки: -qmljsdebugger=port:3768,block
    // Для отладки без блокировки: -qmljsdebugger=port:3768

    // Устанавливаем файл логирования,
    // m_logFile.reset(new QFile(AppPaths::instance().onyxLogDir() + "/logFile.txt"));
    // Открываем файл логирования
    // m_logFile.data()->open(QFile::Append | QFile::Text);
    // Устанавливаем обработчик
    // qInstallMessageHandler(messageHandler);

    NetworkControl::registerNetworkControl();
    UpdateClient::registerUpdateClient();
    RemoteUpdater::registerRemoteUpdater();
    ControlCenter::registerHandles();
    GStreamerVideoPlayer::registerQmlType();

    QSharedPointer<ControlCenter> ctrl  = QSharedPointer<ControlCenter>::create(nullptr);

    // Создаём монитор системы
    SystemMonitor *sysMonitor = new SystemMonitor();
    
    // Создаём генератор секретных ключей
    KeyGenerator *keyGen = new KeyGenerator();

    auto *featureUnlock = new FeatureUnlockController(&app);
    featureUnlock->setKeyGenerator(keyGen);

    auto *dateTimeController = new DateTimeController(&app);

    QQmlApplicationEngine engine;
    auto *translationController = new TranslationController(&engine, &app);
    engine.rootContext()->setContextProperty(
                QStringLiteral("qmlGlAvailable"), qmlGlAvailable);
    engine.rootContext()->setContextProperty("theModel", ctrl->getSocketModel());
    engine.rootContext()->setContextProperty("Editor", ctrl->getModeEditor());
    engine.rootContext()->setContextProperty("recomHandle", ctrl->getHandle());
    engine.rootContext()->setContextProperty("appControl", ctrl.data());
    engine.rootContext()->setContextProperty("periphHandle", ctrl->getPeripheryHandle());

    engine.rootContext()->setContextProperty("sysMonitor", sysMonitor);
    engine.rootContext()->setContextProperty("keyGenerator", keyGen);
    engine.rootContext()->setContextProperty("dateTimeController", dateTimeController);
    engine.rootContext()->setContextProperty("translationController", translationController);
    engine.rootContext()->setContextProperty("appVersion", QCoreApplication::applicationVersion());
    engine.rootContext()->setContextProperty(QStringLiteral("AppPaths"), &AppPaths::instance());

    QVariantMap *initMap = new QVariantMap();
    initMap->insert("boot", 0);
    initMap->insert("language", "ru");
    initMap->insert("serialNumber", "");
    initMap->insert("deviceType", "");
    initMap->insert("featureNotes", "");
    initMap->insert("serviceMenuNoPassword", "0");
    initMap->insert("endoscopyEnabled", "0");
    initMap->insert("totalRuntimeMs", 0);
    initMap->insert("totalActivationMs", 0);
    initMap->insert("httpUploadListenAddress", "");
    initMap->insert("httpUploadPublicBaseUrl", "");
    initMap->insert("httpUploadTrustProxyHeaders", "0");
    initMap->insert("volume", 7);
    m_savedJson = new JsonStorage(nullptr, initMap);
    ctrl->setJsonStorage(m_savedJson);
    featureUnlock->setJsonStorage(m_savedJson);
    ctrl->setFeatureUnlockController(featureUnlock);
    engine.rootContext()->setContextProperty("savedJson", m_savedJson);
    engine.rootContext()->setContextProperty("featureUnlock", featureUnlock);

    // Язык до загрузки QML — иначе первый кадр с русским qsTr, потом мигание перевода
    const QString savedLanguage = translationController->normalizedLanguage(
                m_savedJson->readString(QStringLiteral("language"), QStringLiteral("ru")));
    translationController->setLanguage(savedLanguage);

    auto *deviceLog = new DeviceLogManager(m_savedJson, ctrl->getSocketModel(), &app);
    engine.rootContext()->setContextProperty(QStringLiteral("deviceLog"), deviceLog);
    deviceLog->beginSession();

    auto *updateLog = new UpdateLogManager(&app);
    engine.rootContext()->setContextProperty(QStringLiteral("updateLog"), updateLog);

    auto *globalRemoteUpdater = new RemoteUpdater(&app);
    globalRemoteUpdater->setSerialNumber(m_savedJson->readString(QStringLiteral("serialNumber")));
    const QString savedApiUrl = m_savedJson->readString(QStringLiteral("uiUpdaterApiBaseUrl"));
    if (!savedApiUrl.isEmpty()) {
        globalRemoteUpdater->setApiBaseUrl(savedApiUrl);
    }
    const QString savedTunnelHost = m_savedJson->readString(QStringLiteral("uiUpdaterTunnelHost"));
    if (!savedTunnelHost.isEmpty()) {
        globalRemoteUpdater->setTunnelUserHost(savedTunnelHost);
    }
    engine.rootContext()->setContextProperty(QStringLiteral("remoteUpdater"), globalRemoteUpdater);

    auto *httpUpload = new HttpUploadController(&app);
    httpUpload->setJsonStorage(m_savedJson);
    engine.rootContext()->setContextProperty(QStringLiteral("httpUpload"), httpUpload);

    auto *mcFirmware = new McFirmwareVersionsBridge(&app);
    engine.rootContext()->setContextProperty(QStringLiteral("mcFirmware"), mcFirmware);

    engine.addImageProvider(QLatin1String("instrums"), new InstrImageProvider);
    engine.addImageProvider(QLatin1String("instruments"), new InstrImageProvider);
    engine.addImageProvider(QLatin1String("modes"), new InstrImageProvider);
    engine.addImageProvider(QLatin1String("scopes"), new InstrImageProvider);

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
    // Добавляем пути для поиска QML модулей на удалённой машине - c чего вдруг мы используем дефолтные qt?!
    //чтобы на классные баг нарваться связанные с разными qml файлами?!
     engine.addImportPath("/usr/lib/aarch64-linux-gnu/qt5/qml");//
     engine.addImportPath("/usr/lib/qt5/qml");
    engine.load(url);

    QJsonValue boot;
    m_savedJson->read("boot", &boot);

    // Класс для связи с stm по uart
    m_linkStm = new LinkStm();
    // Откуда грузиться stm
    m_linkStm->setBoot(static_cast<LinkStm::BootChoice>(boot.toInt()));

    QObject::connect(m_linkStm, &LinkStm::sigFirmwareVersionsChanged,
                     mcFirmware, &McFirmwareVersionsBridge::setModules,
                     Qt::QueuedConnection);
    m_linkStm->publishFirmwareVersions();

    httpUpload->setLinkStm(m_linkStm);
    QObject::connect(m_linkStm, &LinkStm::firmwareUpdateParseError,
                     httpUpload, &HttpUploadController::onMcFirmwareParseError, Qt::QueuedConnection);
    QObject::connect(m_linkStm, &LinkStm::sigUpdateProgress,
                     httpUpload, &HttpUploadController::setMcFirmwareUpdateProgress, Qt::QueuedConnection);
    QObject::connect(m_linkStm, &LinkStm::sigActivationStartedDetails,
                     deviceLog, &DeviceLogManager::onActivationStarted, Qt::QueuedConnection);
    QObject::connect(m_linkStm, &LinkStm::sigStopActivation,
                     deviceLog, &DeviceLogManager::onActivationStopped, Qt::QueuedConnection);
    QObject::connect(m_linkStm, &LinkStm::sigStopActivation,
                     deviceLog, &DeviceLogManager::onWarningCode, Qt::QueuedConnection);
    QObject::connect(m_linkStm, &LinkStm::sigErrorForLog,
                     deviceLog, &DeviceLogManager::onWarningCode, Qt::QueuedConnection);
    ctrl->setDeviceLogManager(deviceLog);

    // Переносим LinkStm в отдельный поток для работы с UART
    QThread *linkStmThread = new QThread();
    m_linkStm->moveToThread(linkStmThread);

    QObject::connect(linkStmThread, &QThread::started,
                     m_linkStm, &LinkStm::start);
    QObject::connect(linkStmThread, &QThread::finished,
                     m_linkStm, &QObject::deleteLater);

    // Корректное завершение потока при выходе из приложения
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     ctrl.data(), &ControlCenter::flushPendingSave,
                     Qt::DirectConnection);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     deviceLog, &DeviceLogManager::finalizeSession,
                     Qt::DirectConnection);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     linkStmThread, &QThread::quit);
    QObject::connect(linkStmThread, &QThread::finished,
                     linkStmThread, &QObject::deleteLater);

    linkStmThread->start();

    // Связываем LinkStm с ControlCenter для обработки UART-данных
    ctrl->setLinkStm(m_linkStm);
    ctrl->setVolumeLevel(m_savedJson->readInt(QStringLiteral("volume"), 7));

    qDebug() << "Start";

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

    // Извлекаем только имя файла из полного пути
    QFileInfo fileInfo(context.file);
    QString fileName = fileInfo.fileName();
    
    // Определяем префикс уровня сообщения
    QString levelPrefix;
    switch (type) {
    case QtInfoMsg:     levelPrefix = "INFO"; break;
    case QtDebugMsg:    levelPrefix = "DEBUG"; break;
    case QtWarningMsg:  levelPrefix = "WARNING"; break;
    case QtCriticalMsg:  levelPrefix = "CRITICAL"; break;
    case QtFatalMsg:    levelPrefix = "FATAL"; break;
    default: break;
    }

    // Формируем строку с информацией о файле и строке
    QString logLine = QString("%1 [%2:%3] %4")
                          .arg(levelPrefix)
                          .arg(fileName)
                          .arg(context.line)
                          .arg(msg);

    // Записываем в файл (если файл открыт)
    if (m_logFile && m_logFile->isOpen()) {
        QTextStream out(m_logFile.data());
        out << QDateTime::currentDateTime().toString("dd-MM-yyyy hh:mm:ss.zzz ") 
            << logLine << Qt::endl;
    }

    // stderr — то, что видно при запуске по SSH с ПК
    const QByteArray local = logLine.toLocal8Bit();
    fputs(local.constData(), stderr);
    fputc('\n', stderr);
    fflush(stderr);

    if (type == QtFatalMsg)
        abort();
}
