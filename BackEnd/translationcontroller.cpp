#include "translationcontroller.h"
#include "apppaths.h"
#include "dblocale.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QSaveFile>
#include <QTextStream>

TranslationController::TranslationController(QQmlApplicationEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_language(QStringLiteral("ru"))
    , m_availableLanguages({QStringLiteral("ru"), QStringLiteral("en"), QStringLiteral("es")})
{
}

QString TranslationController::language() const
{
    return m_language;
}

QStringList TranslationController::availableLanguages() const
{
    return m_availableLanguages;
}

bool TranslationController::splashUpdateBusy() const
{
    return m_splashUpdateBusy;
}

void TranslationController::setSplashUpdateBusy(bool busy)
{
    if (m_splashUpdateBusy == busy) {
        return;
    }
    m_splashUpdateBusy = busy;
    emit splashUpdateBusyChanged();
}

QString TranslationController::normalizedLanguage(const QString &language) const
{
    const QString normalized = language.trimmed().toLower();
    return m_availableLanguages.contains(normalized) ? normalized : QStringLiteral("ru");
}

QString TranslationController::nextLanguage(const QString &language) const
{
    const QString normalized = normalizedLanguage(language);
    const int index = m_availableLanguages.indexOf(normalized);
    const int nextIndex = (index + 1) % m_availableLanguages.size();
    return m_availableLanguages.at(nextIndex);
}

bool TranslationController::setLanguage(const QString &language)
{
    if (m_splashUpdateBusy) {
        return false;
    }

    const QString normalized = normalizedLanguage(language);

    if (normalized != m_language) {
        if (!installLanguage(normalized)) {
            return false;
        }
        m_language = normalized;
        DbLocale::setLanguage(normalized);
        emit languageChanged();
    }

    updatePlymouthStartSplash(normalized);
    return true;
}

bool TranslationController::installLanguage(const QString &language)
{
    QCoreApplication::removeTranslator(&m_translator);

    if (language != QStringLiteral("ru")) {
        const QString translationPath = QStringLiteral(":/translations/UserInterface_%1.qm").arg(language);
        if (!m_translator.load(translationPath)) {
            qWarning() << "Failed to load translation" << translationPath;
            return false;
        }

        QCoreApplication::installTranslator(&m_translator);
    }

    if (m_engine) {
        m_engine->retranslate();
    }

    return true;
}

void TranslationController::writeSplashLanguageMarker(const QString &language) const
{
    const QString markerPath = AppPaths::instance().startSplashDir()
            + QStringLiteral("/.active-lang");
    QSaveFile marker(markerPath);
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open splash language marker" << markerPath;
        return;
    }
    QTextStream(&marker) << language;
    if (!marker.commit()) {
        qWarning() << "Failed to write splash language marker" << markerPath;
    }
}

void TranslationController::updatePlymouthStartSplash(const QString &language)
{
    if (m_splashUpdateBusy) {
        return;
    }

    // Desktop Ubuntu (xcb/wayland): не трогаем Plymouth и start.png.
    // На ROC сервис запускает UI с QT_QPA_PLATFORM=eglfs.
    const QString platform = QGuiApplication::platformName();
    if (!platform.startsWith(QLatin1String("eglfs"))) {
        return;
    }

    const AppPaths &paths = AppPaths::instance();
    const QString sourcePath = paths.startSplashForLanguage(language);
    const QString destPath = paths.startSplashPath();
    const QString markerPath = paths.startSplashDir() + QStringLiteral("/.active-lang");

    if (!QFileInfo::exists(sourcePath)) {
        qWarning() << "Plymouth splash missing:" << sourcePath;
        return;
    }

    QString activeLang;
    {
        QFile marker(markerPath);
        if (marker.open(QIODevice::ReadOnly | QIODevice::Text)) {
            activeLang = QString::fromUtf8(marker.readAll()).trimmed().toLower();
        }
    }

    const bool alreadyApplied = (activeLang == language)
            && QFileInfo::exists(destPath);
    if (alreadyApplied) {
        return;
    }

    if (!QDir().mkpath(paths.startSplashDir())) {
        qWarning() << "Cannot create splash dir:" << paths.startSplashDir();
        return;
    }

    if (QFileInfo::exists(destPath) && !QFile::remove(destPath)) {
        qWarning() << "Failed to replace Plymouth splash" << destPath;
        return;
    }
    if (!QFile::copy(sourcePath, destPath)) {
        qWarning() << "Failed to copy Plymouth splash" << sourcePath << "->" << destPath;
        return;
    }

    qInfo() << "Plymouth splash updated:" << sourcePath << "->" << destPath;

    const QString helper = QStringLiteral("/usr/local/sbin/fotek-plymouth-splash");
    if (!QFileInfo::exists(helper)) {
        qWarning() << "Plymouth splash helper missing:" << helper
                   << "(FOTEK start.png updated, boot splash unchanged until helper runs)";
        writeSplashLanguageMarker(language);
        return;
    }

    if (m_splashUpdateProcess) {
        m_splashUpdateProcess->kill();
        m_splashUpdateProcess->deleteLater();
        m_splashUpdateProcess.clear();
    }

    auto *proc = new QProcess(this);
    m_splashUpdateProcess = proc;
    setSplashUpdateBusy(true);

    QObject::connect(proc,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     this,
                     [this, language, proc](int exitCode, QProcess::ExitStatus exitStatus) {
                         const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
                         const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
                         if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                             writeSplashLanguageMarker(language);
                             qInfo() << "Plymouth splash helper finished for language" << language
                                     << out;
                         } else {
                             qWarning() << "Plymouth splash helper failed:"
                                        << "code" << exitCode
                                        << "err" << err
                                        << "out" << out;
                         }
                         if (m_splashUpdateProcess == proc) {
                             m_splashUpdateProcess.clear();
                         }
                         proc->deleteLater();
                         setSplashUpdateBusy(false);
                     });

    proc->start(QStringLiteral("sudo"),
                {QStringLiteral("-n"), helper, destPath});
    if (!proc->waitForStarted(3000)) {
        qWarning() << "Failed to start Plymouth splash helper via sudo -n";
        m_splashUpdateProcess.clear();
        proc->deleteLater();
        setSplashUpdateBusy(false);
        return;
    }
    qInfo() << "Plymouth splash helper started for language" << language;
}
