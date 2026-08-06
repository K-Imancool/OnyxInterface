#include "jsonstorage.h"
#include "apppaths.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <QVariantMap>
#include <QDebug>

#include <fcntl.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// JsonStorage — постоянное хранилище настроек приложения (save.json).
//
// Файл: /var/lib/qtpr/save.json (см. jsonstorage.h).
// Используется из QML через контекстное свойство savedJson.
//
// Критично для встраиваемой платформы (Armbian на одноплатнике): при резком
// обесточивании обычная запись через QFile::write() может остаться только в
// page cache ядра и не попасть на носитель. После перезагрузки приложение
// читает старую версию файла — настройки «сбрасываются».
//
// Поэтому запись реализована как:
//   1) QSaveFile — запись во временный файл и атомарный rename (без «битого»
//      save.json при сбое посередине записи);
//   2) fsync файла — принудительный сброс данных на диск до commit();
//   3) fsync каталога — фиксация метаданных (имя файла после rename).
// ---------------------------------------------------------------------------

namespace {

// Синхронизирует каталог на диск.
//
// После rename() новый inode уже на диске, но запись каталога (связь имя→файл)
// может ещё быть в кэше. fsync по дескриптору каталога (O_DIRECTORY) снижает
// риск потери файла или «невидимости» save.json после внезапного отключения
// питания. На некоторых ФС/ядрах шаг может не поддерживаться — тогда пишем
// предупреждение, но основная защита уже обеспечена fsync'ом самого файла.
bool fsyncDirectory(const QString &path)
{
    const QByteArray encodedPath = QFile::encodeName(path);
    const int dirFd = ::open(encodedPath.constData(), O_RDONLY | O_DIRECTORY);
    if (dirFd < 0) {
        qWarning() << "JsonStorage: failed to open directory for sync" << path;
        return false;
    }

    const bool synced = ::fsync(dirFd) == 0;
    if (!synced) {
        qWarning() << "JsonStorage: failed to sync directory" << path;
    }
    ::close(dirFd);
    return synced;
}

// Записывает весь JSON-объект в filePath одной транзакцией.
//
// Возвращает true только если данные гарантированно дошли до носителя
// (в пределах гарантий ОС/ФС). При любой ошибке целевой filePath не
// перезаписывается частично — QSaveFile::cancelWriting() откатывает temp.
bool writeJsonObjectToFile(const QString &filePath, const QJsonObject &object)
{
    // QSaveFile пишет во временный файл рядом с целевым и делает rename при commit().
    QSaveFile jsonFile(filePath);
    if (!jsonFile.open(QIODevice::WriteOnly)) {
        qWarning() << "JsonStorage: failed to open" << filePath
                   << "for writing:" << jsonFile.errorString();
        return false;
    }

    const QByteArray jsonBytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    const qint64 written = jsonFile.write(jsonBytes);
    if (written != jsonBytes.size()) {
        qWarning() << "JsonStorage: failed to fully write" << filePath
                   << "- written" << written << "of" << jsonBytes.size()
                   << "bytes:" << jsonFile.errorString();
        jsonFile.cancelWriting();
        return false;
    }

    // flush() сбрасывает буферы Qt; fsync() — буферы ядра на физический носитель.
    // Делаем это до commit(), пока открыт дескриптор временного файла.
    if (!jsonFile.flush() || ::fsync(jsonFile.handle()) != 0) {
        qWarning() << "JsonStorage: failed to sync" << filePath
                   << "before commit:" << jsonFile.errorString();
        jsonFile.cancelWriting();
        return false;
    }

    // Атомарная подмена save.json готовым содержимым.
    if (!jsonFile.commit()) {
        qWarning() << "JsonStorage: failed to commit" << filePath
                   << ":" << jsonFile.errorString();
        return false;
    }

    fsyncDirectory(QFileInfo(filePath).absolutePath());
    return true;
}

} // namespace

JsonStorage::JsonStorage(QObject *parent, QVariantMap* initMap)
    : QObject{parent}
{
    // Каталог /var/lib/qtpr может отсутствовать на первом запуске или после
    // прошивки — создаём заранее, иначе последующая запись не сработает.
    QFileInfo info(JSON_FILE_NAME);
    QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(dir.absolutePath())) {
        qWarning() << "JsonStorage: failed to create directory" << dir.absolutePath();
    }

    // Одноразовая миграция со старого пути (<fotekRoot>/OnyxLog/save.json),
    // если новый продакшен-файл ещё не создан. Копия, не move — старый файл
    // остаётся как резерв на случай отката.
    const QString legacyPath = AppPaths::instance().legacySaveJsonPath();
    if (!QFile::exists(JSON_FILE_NAME) && QFile::exists(legacyPath)) {
        if (!QFile::copy(legacyPath, JSON_FILE_NAME)) {
            qWarning() << "JsonStorage: failed to migrate legacy file from"
                       << legacyPath << "to" << JSON_FILE_NAME;
        }
    }

    QFile jsonFile(JSON_FILE_NAME);
    if (QFile::exists(JSON_FILE_NAME) && jsonFile.open(QIODevice::ReadOnly)) {
        // Нормальный путь: загружаем существующие настройки в память.
        const QByteArray jsonData = jsonFile.readAll();
        m_document = QJsonDocument::fromJson(jsonData);
        m_object = m_document.object();
        jsonFile.close();
    } else {
        // Первый запуск, повреждённый или недоступный файл — инициализация
        // дефолтами из main.cpp (initMap) и немедленная запись на диск.
        if (initMap) {
            m_object = QJsonObject::fromVariantMap(*initMap);
        } else {
            qWarning() << "JsonStorage: initMap is null, starting with empty object";
            m_object = QJsonObject();
        }
        m_document.setObject(m_object);
        writeJsonObjectToFile(JSON_FILE_NAME, m_object);
    }
}

JsonStorage::~JsonStorage()
{
    // Явного save() при уничтожении нет: каждый save()/saveString() уже
    // синхронно пишет полный JSON на диск (см. writeJsonObjectToFile).
}

void JsonStorage::save(QString key, QJsonValue data)
{
    // Обновляем in-memory копию и сразу перезаписываем весь файл.
    // Модель «один ключ — один полный dump»: просто и надёжно для небольшого
    // save.json; при росте числа частых записей можно перейти на батчинг.
    m_object[key] = data;
    m_document.setObject(m_object);

    QFileInfo info(JSON_FILE_NAME);
    QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(dir.absolutePath())) {
        qWarning() << "JsonStorage: failed to create directory" << dir.absolutePath();
        return;
    }

    writeJsonObjectToFile(JSON_FILE_NAME, m_object);
}

bool JsonStorage::read(QString key, QJsonValue* data)
{
    if (!m_object.contains(key))
        return false;
    QJsonValue savedData = m_object[key];
    *data = savedData;
    return true;
}

void JsonStorage::saveString(const QString& key, const QString& value)
{
    // Обёртка для QML: все строковые флаги (endoscopyEnabled, serviceMenuNoPassword
    // и т.д.) хранятся как строки "0"/"1" в JSON.
    save(key, QJsonValue(value));
}

QString JsonStorage::readString(const QString& key, const QString& defaultValue) const
{
    if (!m_object.contains(key))
        return defaultValue;
    return m_object.value(key).toString(defaultValue);
}

void JsonStorage::saveInt(const QString& key, int value)
{
    save(key, QJsonValue(value));
}

int JsonStorage::readInt(const QString& key, int defaultValue) const
{
    if (!m_object.contains(key))
        return defaultValue;
    return m_object.value(key).toInt(defaultValue);
}
