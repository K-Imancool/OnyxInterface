#include "featureunlockcontroller.h"
#include "jsonstorage.h"
#include "keygenerator.h"
#include "databasereader.h"
#include "surgicalmode.h"

#include <QDebug>

namespace {

constexpr auto kActivatedKeysStorageKey = "activatedFeatureKeys";
constexpr auto kLegacyEndoscopyKey = "endoscopyEnabled";

QSet<int> collectKeysFromRows(const QList<QVariantList> &rows, int columnIndex)
{
    QSet<int> discovered;
    for (const QVariantList &row : rows) {
        if (row.size() <= columnIndex) {
            continue;
        }
        discovered.unite(FeatureUnlockController::parseKeyRequirement(row.at(columnIndex).toString()));
    }
    return discovered;
}

} // namespace

FeatureUnlockController::FeatureUnlockController(QObject *parent)
    : QObject(parent)
{
}

void FeatureUnlockController::setJsonStorage(JsonStorage *storage)
{
    m_jsonStorage = storage;
    migrateLegacyEndoscopyFlag();
}

void FeatureUnlockController::setKeyGenerator(KeyGenerator *generator)
{
    m_keyGenerator = generator;
}

void FeatureUnlockController::setDatabaseReader(DataBaseReader *reader)
{
    m_dbReader = reader;
}

QSet<int> FeatureUnlockController::parseKeyRequirement(const QString &keyField)
{
    QSet<int> required;
    const QString trimmed = keyField.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("0")) {
        return required;
    }

    const QStringList parts = trimmed.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        bool ok = false;
        const int keyNumber = part.trimmed().toInt(&ok);
        if (ok && keyNumber > 0) {
            required.insert(keyNumber);
        }
    }
    return required;
}

QString FeatureUnlockController::featureCodeForKey(int keyNumber)
{
    return QString::number(keyNumber);
}

QSet<int> FeatureUnlockController::activatedKeys() const
{
    QSet<int> keys;
    if (!m_jsonStorage) {
        return keys;
    }

    const QString stored = m_jsonStorage->readString(QString::fromLatin1(kActivatedKeysStorageKey), QString());
    for (const QString &part : stored.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int keyNumber = part.trimmed().toInt(&ok);
        if (ok && keyNumber > 0) {
            keys.insert(keyNumber);
        }
    }
    return keys;
}

void FeatureUnlockController::activateKey(int keyNumber)
{
    if (keyNumber <= 0 || !m_jsonStorage) {
        return;
    }

    QSet<int> keys = activatedKeys();
    if (keys.contains(keyNumber)) {
        return;
    }

    keys.insert(keyNumber);
    QStringList parts;
    for (int key : keys) {
        parts.append(QString::number(key));
    }
    parts.sort(Qt::CaseSensitive);
    m_jsonStorage->saveString(QString::fromLatin1(kActivatedKeysStorageKey), parts.join(QLatin1Char(',')));
    emit activatedKeysChanged();
}

void FeatureUnlockController::deactivateKey(int keyNumber)
{
    if (keyNumber <= 0 || !m_jsonStorage) {
        return;
    }

    QSet<int> keys = activatedKeys();
    if (!keys.remove(keyNumber)) {
        return;
    }

    QStringList parts;
    for (int key : keys) {
        parts.append(QString::number(key));
    }
    parts.sort(Qt::CaseSensitive);
    m_jsonStorage->saveString(QString::fromLatin1(kActivatedKeysStorageKey), parts.join(QLatin1Char(',')));
    emit activatedKeysChanged();
}

void FeatureUnlockController::setKeyActivated(int keyNumber, bool activated)
{
    if (activated) {
        activateKey(keyNumber);
    } else {
        deactivateKey(keyNumber);
    }
}

bool FeatureUnlockController::areAllKeysActivated(int maxKeyNumber) const
{
    if (maxKeyNumber <= 0) {
        return true;
    }

    for (int keyNumber = 1; keyNumber <= maxKeyNumber; ++keyNumber) {
        if (!isKeyActivated(keyNumber)) {
            return false;
        }
    }
    return true;
}

void FeatureUnlockController::setAllKeysActivated(bool activated, int maxKeyNumber)
{
    if (maxKeyNumber <= 0 || !m_jsonStorage) {
        return;
    }

    QSet<int> keys;
    if (activated) {
        for (int keyNumber = 1; keyNumber <= maxKeyNumber; ++keyNumber) {
            keys.insert(keyNumber);
        }
    }

    if (activatedKeys() == keys) {
        return;
    }

    QStringList parts;
    for (int keyNumber : keys) {
        parts.append(QString::number(keyNumber));
    }
    parts.sort(Qt::CaseSensitive);
    m_jsonStorage->saveString(QString::fromLatin1(kActivatedKeysStorageKey), parts.join(QLatin1Char(',')));
    emit activatedKeysChanged();
}

void FeatureUnlockController::migrateLegacyEndoscopyFlag()
{
    if (!m_jsonStorage) {
        return;
    }

    if (m_jsonStorage->readString(QString::fromLatin1(kLegacyEndoscopyKey), QStringLiteral("0")) != QStringLiteral("1")) {
        return;
    }

    activateKey(1);
    m_jsonStorage->saveString(QString::fromLatin1(kLegacyEndoscopyKey), QStringLiteral("0"));
}

bool FeatureUnlockController::isKeyActivated(int keyNumber) const
{
    return keyNumber > 0 && activatedKeys().contains(keyNumber);
}

bool FeatureUnlockController::isKeyRequirementMet(const QString &keyField) const
{
    const QSet<int> required = parseKeyRequirement(keyField);
    if (required.isEmpty()) {
        return true;
    }

    const QSet<int> active = activatedKeys();
    for (int keyNumber : required) {
        if (active.contains(keyNumber)) {
            return true;
        }
    }
    return false;
}

bool FeatureUnlockController::isScopeLocked(int scopeId) const
{
    if (!m_dbReader || scopeId < 0) {
        return false;
    }

    const QList<QVariantList> rows = m_dbReader->slotSendSelectQuery(
                QStringList{QStringLiteral("Scopes")},
                QStringList{QStringLiteral("KEY")},
                QStringLiteral("id = %1").arg(scopeId));
    if (rows.isEmpty() || rows.first().isEmpty()) {
        return false;
    }

    return !isKeyRequirementMet(rows.first().at(0).toString());
}

bool FeatureUnlockController::isModeLocked(int modeId) const
{
    if (!m_dbReader || modeId <= 0 || modeId == ESHF::NO_MODE) {
        return false;
    }

    const QList<QVariantList> rows = m_dbReader->slotSendSelectQuery(
                QStringList{QStringLiteral("Modes")},
                QStringList{QStringLiteral("KEY")},
                QStringLiteral("id = %1").arg(modeId));
    if (rows.isEmpty() || rows.first().isEmpty()) {
        return false;
    }

    return !isKeyRequirementMet(rows.first().at(0).toString());
}

QSet<int> FeatureUnlockController::discoverKeyNumbers() const
{
    QSet<int> discovered;
    if (!m_dbReader) {
        return discovered;
    }

    discovered.unite(collectKeysFromRows(
                         m_dbReader->slotSendSelectQuery(QStringList{QStringLiteral("Scopes")},
                                                         QStringList{QStringLiteral("KEY")},
                                                         QStringLiteral("id < 1000")),
                         0));
    discovered.unite(collectKeysFromRows(
                         m_dbReader->slotSendSelectQuery(QStringList{QStringLiteral("Modes")},
                                                         QStringList{QStringLiteral("KEY")},
                                                         QString()),
                         0));
    return discovered;
}

int FeatureUnlockController::tryActivateWithUnlockKey(int serialNumber,
                                                      const QString &deviceType,
                                                      const QString &key)
{
    if (!m_keyGenerator || serialNumber < 260000 || serialNumber > 1000000) {
        return -1;
    }

    const QString normalizedKey = key.trimmed();
    if (normalizedKey.length() != 12) {
        return -1;
    }

    QSet<int> candidates = discoverKeyNumbers();
    if (candidates.isEmpty()) {
        for (int fallback = 1; fallback <= 9; ++fallback) {
            candidates.insert(fallback);
        }
    }

    QList<int> sortedCandidates = candidates.values();
    std::sort(sortedCandidates.begin(), sortedCandidates.end());

    for (int keyNumber : sortedCandidates) {
        if (isKeyActivated(keyNumber)) {
            continue;
        }
        if (m_keyGenerator->validateUnlockKey(serialNumber,
                                              deviceType,
                                              featureCodeForKey(keyNumber),
                                              normalizedKey)) {
            activateKey(keyNumber);
            return keyNumber;
        }
    }

    return -1;
}

QString FeatureUnlockController::activatedKeysText() const
{
    QStringList parts;
    QList<int> keys = activatedKeys().values();
    std::sort(keys.begin(), keys.end());
    for (int keyNumber : keys) {
        parts.append(QString::number(keyNumber));
    }
    return parts.join(QLatin1Char(','));
}
