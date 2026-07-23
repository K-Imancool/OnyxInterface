#include "apppaths.h"

#include <QDir>
#include <QUrl>

#include <cstdlib>

namespace {

QString normalizeRoot(QString root)
{
    root = root.trimmed();
    if (root.startsWith(QLatin1Char('"')) && root.endsWith(QLatin1Char('"')) && root.size() >= 2) {
        root = root.mid(1, root.size() - 2);
    }
    if (root.endsWith(QLatin1Char('/')) && root.size() > 1) {
        root.chop(1);
    }
    return QDir::cleanPath(root);
}

QString defaultFotekRoot()
{
    return QDir::homePath() + QStringLiteral("/FOTEK");
}

QString defaultReleasesRoot()
{
    return QDir::homePath() + QStringLiteral("/releases");
}

QString takeArgValue(int argc, char *argv[], int &i, const QLatin1String &keyEq, const QLatin1String &key)
{
    const QString arg = QString::fromLocal8Bit(argv[i]);
    if (arg.startsWith(keyEq)) {
        return arg.mid(keyEq.size());
    }
    if (arg == key && i + 1 < argc) {
        ++i;
        return QString::fromLocal8Bit(argv[i]);
    }
    return QString();
}

} // namespace

AppPaths &AppPaths::instance()
{
    static AppPaths paths;
    return paths;
}

void AppPaths::initializeFromArgs(int argc, char *argv[])
{
    QString fotekRoot;
    QString releasesRoot;

    if (const char *envRoot = std::getenv("ONYX_FOTEK_ROOT")) {
        const QString fromEnv = QString::fromLocal8Bit(envRoot).trimmed();
        if (!fromEnv.isEmpty()) {
            fotekRoot = fromEnv;
        }
    }
    if (const char *envRoot = std::getenv("ONYX_RELEASES_ROOT")) {
        const QString fromEnv = QString::fromLocal8Bit(envRoot).trimmed();
        if (!fromEnv.isEmpty()) {
            releasesRoot = fromEnv;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const QString fotek = takeArgValue(argc, argv, i,
                                           QLatin1String("--fotek-root="),
                                           QLatin1String("--fotek-root"));
        if (!fotek.isEmpty()) {
            fotekRoot = fotek;
            continue;
        }
        const QString releases = takeArgValue(argc, argv, i,
                                              QLatin1String("--releases-root="),
                                              QLatin1String("--releases-root"));
        if (!releases.isEmpty()) {
            releasesRoot = releases;
        }
    }

    if (fotekRoot.isEmpty()) {
        fotekRoot = defaultFotekRoot();
    }
    if (releasesRoot.isEmpty()) {
        releasesRoot = defaultReleasesRoot();
    }

    instance().setFotekRoot(fotekRoot);
    instance().setReleasesRoot(releasesRoot);
}

AppPaths::AppPaths(QObject *parent)
    : QObject(parent)
    , m_fotekRoot(defaultFotekRoot())
    , m_releasesRoot(defaultReleasesRoot())
{
}

void AppPaths::setFotekRoot(const QString &root)
{
    m_fotekRoot = normalizeRoot(root);
}

void AppPaths::setReleasesRoot(const QString &root)
{
    m_releasesRoot = normalizeRoot(root);
}

QString AppPaths::fotekRoot() const
{
    return m_fotekRoot;
}

QString AppPaths::imagesDir() const
{
    return m_fotekRoot + QStringLiteral("/Images");
}

QString AppPaths::iconsDir() const
{
    return imagesDir() + QStringLiteral("/icons");
}

QString AppPaths::neIconsDir() const
{
    return imagesDir() + QStringLiteral("/ne");
}

QString AppPaths::instrumentsDir() const
{
    return imagesDir() + QStringLiteral("/instruments");
}

QString AppPaths::modesDir() const
{
    return imagesDir() + QStringLiteral("/modes");
}

QString AppPaths::scopesDir() const
{
    return imagesDir() + QStringLiteral("/scopes");
}

QString AppPaths::videoDir() const
{
    return m_fotekRoot + QStringLiteral("/Video");
}

QString AppPaths::downloadDir() const
{
    return m_fotekRoot + QStringLiteral("/TestFolder/RECIEVE");
}

QString AppPaths::eshfDbPath() const
{
    return m_fotekRoot + QStringLiteral("/eshfDb.db");
}

QString AppPaths::userProgDbPath() const
{
    return m_fotekRoot + QStringLiteral("/userProg.db");
}

QString AppPaths::logoPath() const
{
    return imagesDir() + QStringLiteral("/logo.png");
}

QString AppPaths::onyxLogDir() const
{
    return QDir::homePath() + QStringLiteral("/OnyxLog");
}

QString AppPaths::legacySaveJsonPath() const
{
    return m_fotekRoot + QStringLiteral("/OnyxLog/save.json");
}

QString AppPaths::releasesRoot() const
{
    return m_releasesRoot;
}

QString AppPaths::releasesSubdir(const QString &name) const
{
    return m_releasesRoot + QLatin1Char('/') + name;
}

QString AppPaths::releasesMainDir() const
{
    return releasesSubdir(QStringLiteral("main"));
}

QString AppPaths::releasesMediaDir() const
{
    return releasesSubdir(QStringLiteral("media"));
}

QString AppPaths::releasesComDir() const
{
    return releasesSubdir(QStringLiteral("com"));
}

QString AppPaths::releasesArgDir() const
{
    return releasesSubdir(QStringLiteral("arg"));
}

QString AppPaths::releasesGenDir() const
{
    return releasesSubdir(QStringLiteral("gen"));
}

QString AppPaths::releasesMainVersionDir(const QString &version) const
{
    return releasesMainDir() + QLatin1Char('/') + version;
}

QString AppPaths::releasesMediaVersionDir(const QString &version) const
{
    return releasesMediaDir() + QLatin1Char('/') + version;
}

QString AppPaths::releasesMainBinaryPath(const QString &version) const
{
    return releasesMainVersionDir(version) + QStringLiteral("/UserInterface");
}

QString AppPaths::releasesFirmwareHexPath(const QString &subdir,
                                          const QString &filePrefixUpper,
                                          const QString &version) const
{
    return releasesSubdir(subdir) + QLatin1Char('/')
            + filePrefixUpper + QLatin1Char('-') + version + QStringLiteral(".hex");
}

QString AppPaths::dirToUrl(const QString &dirPath) const
{
    QString path = QDir::cleanPath(dirPath);
    if (!path.endsWith(QLatin1Char('/'))) {
        path += QLatin1Char('/');
    }
    return QUrl::fromLocalFile(path).toString();
}

QString AppPaths::iconsBaseUrl() const
{
    return dirToUrl(iconsDir());
}

QString AppPaths::neIconsBaseUrl() const
{
    return dirToUrl(neIconsDir());
}

QUrl AppPaths::logoUrl() const
{
    return QUrl::fromLocalFile(logoPath());
}
