#ifndef SOCKETMODEEDITOR_H
#define SOCKETMODEEDITOR_H

#include <QObject>
#include <QVariant>
#include <QVariantMap>

#include "socketmodel.h"

class SocketModeEditor : public QObject
{
    Q_OBJECT
    /**
     * @property Q_PROPERTY(bool hasChanges READ hasChanges NOTIFY hasChangesChanged)
     * @brief Флаг наличия изменений для сокета
     *
     * Это свойство содержит фдаг наличия изменения в настройках сокета по сравнению с изначальными
     *
     * @accessors hasChanges()
     * @notifier hasChangesChanged()
     */
    Q_PROPERTY(bool hasChanges READ hasChanges NOTIFY hasChangesChanged)

    /**
     * @property Q_PROPERTY(QStringList modeNames READ modeNames NOTIFY parametersLoaded)
     * @brief Список имён режимов
     *
     * Это свойство содержит список доступных имён режимов для редактируемого сокета
     *
     * @accessors modeNames()
     * @notifier parametersLoaded()
     */
    Q_PROPERTY(QStringList modeNames READ modeNames NOTIFY parametersLoaded)

    /**
     * @property Q_PROPERTY(QStringList instrList READ instrList NOTIFY parametersLoaded)
     * @brief Список имён доступных инструменто для выбранного режима
     *
     * @accessors instrList()
     * @notifier parametersLoaded()
     */
    Q_PROPERTY(QStringList instrList READ instrList NOTIFY parametersLoaded)

    /**
     * @property Q_PROPERTY(QString socketName READ socketName NOTIFY parametersLoaded)
     * @brief Текущее имя редактируемого сокета
     *
     * @accessors socketName()
     * @notifier parametersLoaded()
     */
    Q_PROPERTY(QString socketName READ socketName NOTIFY parametersLoaded)

    /**
     * @property Q_PROPERTY(QVariantMap currentMode READ currentMode NOTIFY currentParamsChanged)
     * @brief Полный перечень текущих настроек в диалоге
     *
     * Это свойство содержит QVariantMap всех настроек для редактируемого режима
     *
     * @accessors currentMode()
     * @notifier currentParamsChanged()
     */
    Q_PROPERTY(QVariantMap currentMode READ currentMode NOTIFY currentParamsChanged)

    /**
     * @property Q_PROPERTY(int currentModeIndex READ currentModeIndex WRITE setCurrentModeIndex NOTIFY currentModeIndexChanged)
     * @brief Относительный индекс выбранного режима
     *
     * @accessors currentModeIndex(), setCurrentModeIndex
     * @notifier currentModeIndexChanged()
     */
    Q_PROPERTY(int currentModeIndex READ currentModeIndex WRITE setCurrentModeIndex NOTIFY currentModeIndexChanged)

    /**
     * @property Q_PROPERTY(int currentInstrIndex READ currentInstrIndex WRITE setCurrentInstrIndex NOTIFY currentInstrChanged)
     * @brief Индекс выбранного инструмента
     *
     * @accessors currentInstrIndex(), setCurrentInstrIndex()
     * @notifier currentInstrChanged()
     */
    Q_PROPERTY(int currentInstrIndex READ currentInstrIndex WRITE setCurrentInstrIndex NOTIFY currentInstrChanged)

    /**
     * @property Q_PROPERTY(int currentInstrIndex READ currentInstrIndex WRITE setCurrentInstrIndex NOTIFY currentInstrChanged)
     * @brief текущая мощность
     *
     * @accessors currentInstrIndex(), setCurrentInstrIndex()
     * @notifier currentInstrChanged()
     */
    Q_PROPERTY(int currentPower READ currentPower NOTIFY currentParamsChanged)

    Q_PROPERTY(bool isEndo READ isEndo NOTIFY currentParamsChanged)


    Q_PROPERTY(int instrID READ instrID NOTIFY currentInstrChanged)
    Q_PROPERTY(int lowPowerBound READ lowPowerBound NOTIFY currentInstrChanged)
    Q_PROPERTY(int midPowerBound READ midPowerBound NOTIFY currentInstrChanged)
    Q_PROPERTY(int highPowerBound READ highPowerBound NOTIFY currentInstrChanged)
    Q_PROPERTY(QString modeBrief READ modeBrief NOTIFY currentModeIndexChanged)
    Q_PROPERTY(QString modeDescript READ modeDescript NOTIFY currentModeIndexChanged)
    Q_PROPERTY(QString instrBrief READ instrBrief NOTIFY currentInstrChanged)
    Q_PROPERTY(bool isCoag READ isCoag NOTIFY parametersLoaded)

public:
    explicit SocketModeEditor(QSharedPointer<SocketModel> model, QObject *parent = nullptr);

    Q_INVOKABLE void initialize(int socket, int mode, bool isCoag);
    Q_INVOKABLE void loadModeParameters(int modeIndex);
    Q_INVOKABLE void updateCurrentParameters(const QVariantMap ms);
    Q_INVOKABLE void updateParameter(const QString& paramName, const QVariant& value);
    Q_INVOKABLE void commitChanges();
    Q_INVOKABLE void rollBack();

    Q_INVOKABLE QStringList modeNames() const;
    Q_INVOKABLE QStringList instrList() const;
    Q_INVOKABLE QStringList modeNamesIds() const;
    Q_INVOKABLE QStringList modeNamesNums() const;
    Q_INVOKABLE QStringList instrListIds() const;
    Q_INVOKABLE QStringList instrListNums() const;
    Q_INVOKABLE QVariantMap currentMode() const;
    Q_INVOKABLE QString socketName() const;
    Q_INVOKABLE int currentModeIndex() const;
    Q_INVOKABLE void setCurrentModeIndex(int index);

    Q_INVOKABLE bool hasChanges() const;
    Q_INVOKABLE void setAutoModeDirty(bool dirty);

    int currentInstrIndex() const;

    void setCurrentInstrIndex(int newCurrentInstrIndex);

    int lowPowerBound() const;

    int midPowerBound() const;

    int highPowerBound() const;

    int instrID() const;

    bool isCoag() const;

    int currentPower() const;

    QString modeDescript() const;

    QString modeBrief() const;

    QString instrBrief() const;

    bool isEndo() const;

signals:
    void currentParamsChanged();
    void currentModeIndexChanged();
    void parametersLoaded();
    void editingFinished(bool);

    void hasChangesChanged();

    void currentInstrChanged();

    void currentPowerChang();

private:
    struct ModeCacheEntry {
        QVariantMap parameters;
        QStringList instrList;
        QStringList instrListIds;
        QStringList instrListNums;
        int selectedInstrIndex = -1;
    };

    void preloadModeCache();
    bool hasCachedModeData(int modeIndex) const;

    bool checkChanges();
    QVariantMap fetchModeParameters(int modeIndex);
    bool isParamsEqual(const QVariantMap& a, const QVariantMap& b) const;

    int m_socketRow;
    int m_socketID;
    QStringList m_modeNames;
    QStringList m_modeNameIds;
    QStringList m_modeNameNums;
    QStringList m_instrList;
    QStringList m_instrListIds;
    QStringList m_instrListNums;
    QVariantMap m_currentParameters;
    QString m_socketName;
    int m_currentModeIndex = -1;
    int m_originalModeIndex = -1;
    int m_currentInstrIndex = -1;
    int m_preferredInstrId = -1;
    QVariantMap m_originalParameters;
    QSharedPointer<SocketModel> m_model;

    bool m_hasChanges;
    bool m_autoModeDirty = false;
    bool m_isCoag;
    int m_lowPowerBound;
    int m_midPowerBound;
    int m_highPowerBound;
    int m_instrID;
    QString m_modeDescript;
    QString m_modeBrief;
    QVector<ModeCacheEntry> m_modeCache;
};
#endif // SOCKETMODEEDITOR_H
