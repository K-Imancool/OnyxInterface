#ifndef APPPATHS_H
#define APPPATHS_H

#include <QObject>
#include <QString>
#include <QUrl>

/**
 * Единый центр путей к данным FOTEK, журналам и releases.
 *
 * По умолчанию: ~/FOTEK, ~/OnyxLog, ~/releases
 * Переопределение для десктопа:
 *   - ONYX_FOTEK_ROOT / --fotek-root
 *   - ONYX_RELEASES_ROOT / --releases-root
 *
 * Вызывать AppPaths::initializeFromArgs() до создания OnyxApp.
 */
class AppPaths : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString fotekRoot READ fotekRoot CONSTANT)
    Q_PROPERTY(QString imagesDir READ imagesDir CONSTANT)
    Q_PROPERTY(QString iconsDir READ iconsDir CONSTANT)
    Q_PROPERTY(QString neIconsDir READ neIconsDir CONSTANT)
    Q_PROPERTY(QString instrumentsDir READ instrumentsDir CONSTANT)
    Q_PROPERTY(QString modesDir READ modesDir CONSTANT)
    Q_PROPERTY(QString scopesDir READ scopesDir CONSTANT)
    Q_PROPERTY(QString videoDir READ videoDir CONSTANT)
    Q_PROPERTY(QString soundsDir READ soundsDir CONSTANT)
    Q_PROPERTY(QString downloadDir READ downloadDir CONSTANT)
    Q_PROPERTY(QString eshfDbPath READ eshfDbPath CONSTANT)
    Q_PROPERTY(QString userProgDbPath READ userProgDbPath CONSTANT)
    Q_PROPERTY(QString logoPath READ logoPath CONSTANT)
    Q_PROPERTY(QString onyxLogDir READ onyxLogDir CONSTANT)
    Q_PROPERTY(QString legacySaveJsonPath READ legacySaveJsonPath CONSTANT)
    Q_PROPERTY(QString releasesRoot READ releasesRoot CONSTANT)
    Q_PROPERTY(QString releasesMainDir READ releasesMainDir CONSTANT)
    Q_PROPERTY(QString releasesMediaDir READ releasesMediaDir CONSTANT)
    Q_PROPERTY(QString releasesComDir READ releasesComDir CONSTANT)
    Q_PROPERTY(QString releasesArgDir READ releasesArgDir CONSTANT)
    Q_PROPERTY(QString releasesGenDir READ releasesGenDir CONSTANT)

    Q_PROPERTY(QString iconsBaseUrl READ iconsBaseUrl CONSTANT)
    Q_PROPERTY(QString neIconsBaseUrl READ neIconsBaseUrl CONSTANT)
    Q_PROPERTY(QUrl logoUrl READ logoUrl CONSTANT)

public:
    static AppPaths &instance();

    /**
     * Разбор argv / env. Безопасно вызывать до QApplication.
     * FOTEK: ONYX_FOTEK_ROOT, --fotek-root
     * Releases: ONYX_RELEASES_ROOT, --releases-root
     */
    static void initializeFromArgs(int argc, char *argv[]);

    QString fotekRoot() const;

    QString imagesDir() const;
    QString iconsDir() const;
    QString neIconsDir() const;
    QString instrumentsDir() const;
    QString modesDir() const;
    QString scopesDir() const;
    QString videoDir() const;
    QString soundsDir() const;
    QString downloadDir() const;

    QString eshfDbPath() const;
    QString userProgDbPath() const;
    QString logoPath() const;
    QString startSplashDir() const;
    QString startSplashPath() const;
    QString startSplashForLanguage(const QString &language) const;

    /** ~/OnyxLog — журналы (не внутри FOTEK). */
    QString onyxLogDir() const;
    /** Легаси: <fotekRoot>/OnyxLog/save.json */
    QString legacySaveJsonPath() const;

    /** По умолчанию ~/releases. */
    QString releasesRoot() const;
    QString releasesMainDir() const;
    QString releasesMediaDir() const;
    QString releasesComDir() const;
    QString releasesArgDir() const;
    QString releasesGenDir() const;

    Q_INVOKABLE QString releasesSubdir(const QString &name) const;
    Q_INVOKABLE QString releasesMainVersionDir(const QString &version) const;
    Q_INVOKABLE QString releasesMediaVersionDir(const QString &version) const;
    Q_INVOKABLE QString releasesMainBinaryPath(const QString &version) const;
    Q_INVOKABLE QString releasesFirmwareHexPath(const QString &subdir,
                                                const QString &filePrefixUpper,
                                                const QString &version) const;

    /** file://…/ с завершающим '/' — удобно для конкатенации в QML. */
    QString iconsBaseUrl() const;
    QString neIconsBaseUrl() const;
    QUrl logoUrl() const;

    /** file:// URL каталога с завершающим '/'. */
    Q_INVOKABLE QString dirToUrl(const QString &dirPath) const;

private:
    explicit AppPaths(QObject *parent = nullptr);
    Q_DISABLE_COPY(AppPaths)

    void setFotekRoot(const QString &root);
    void setReleasesRoot(const QString &root);

    QString m_fotekRoot;
    QString m_releasesRoot;
};

#endif // APPPATHS_H
