#ifndef TRANSLATIONCONTROLLER_H
#define TRANSLATIONCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTranslator>

class QProcess;
class QQmlApplicationEngine;

class TranslationController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(bool splashUpdateBusy READ splashUpdateBusy NOTIFY splashUpdateBusyChanged)

public:
    explicit TranslationController(QQmlApplicationEngine *engine, QObject *parent = nullptr);

    QString language() const;
    QStringList availableLanguages() const;
    bool splashUpdateBusy() const;

    Q_INVOKABLE QString normalizedLanguage(const QString &language) const;
    Q_INVOKABLE QString nextLanguage(const QString &language) const;

public Q_SLOTS:
    bool setLanguage(const QString &language);

Q_SIGNALS:
    void languageChanged();
    void splashUpdateBusyChanged();

private:
    bool installLanguage(const QString &language);
    void updatePlymouthStartSplash(const QString &language);
    void setSplashUpdateBusy(bool busy);
    void writeSplashLanguageMarker(const QString &language) const;

    QPointer<QQmlApplicationEngine> m_engine;
    QTranslator m_translator;
    QString m_language;
    const QStringList m_availableLanguages;
    bool m_splashUpdateBusy = false;
    QPointer<QProcess> m_splashUpdateProcess;
};

#endif // TRANSLATIONCONTROLLER_H
