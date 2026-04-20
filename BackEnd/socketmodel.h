#ifndef SOCKETMODEL_H
#define SOCKETMODEL_H

#include <QAbstractItemModel>
#include <QObject>
#include <QSharedPointer>
#include <map>
#include <QMutex>
#include "instrument.h"

#include "socket.h"

constexpr int ENDO_MAX = 3;

using SocketStrings = std::array<std::array<QString, 3>, 8>; //instrId, modeId, power

class SocketModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int subProgIdx READ subProgIdx WRITE setSubProgIdx NOTIFY subProgIdxChanged FINAL)
    Q_PROPERTY(int subProgCount READ subProgCount NOTIFY subProgCountChanged FINAL)
public:
    enum SocketRoles {
        SocketStatus = Qt::UserRole+1,
        SocketEnabled,
        SocketAllowed,
        SocketDisplayMode,
        SocketName,
        SocketPolarity,
        SocketPedal,
        SocketAllowedPedal,
        SocketUartInfo,
        CoagModeIndex,
        CoagModeId,
        CoagModeIdList,
        CoagModeNum,
        CoagModeBrief,
        CoagModeDescript,
        CoagModeName,
        CoagModePower,
        CoagModeMinPower,
        CoagModeMaxPower,
        CoagModeInstrName,
        CoagModeInstrImage,
        CoagModeInstrIndex,
        CoagModeInstrID,
        CoagModeInstrIdList,
        CoagModeInstrNum,
        CoagModeIsEndo,
        CoagModesNames,
        CutModeIndex,
        CutModeId,
        CutModeIdList,
        CutModeNum,
        CutModeBrief,
        CutModeDescript,
        CutModeName,
        CutModePower,
        CutModeMinPower,
        CutModeMaxPower,
        CutModeInstrName,
        CutModeInstrImage,
        CutModeInstrIndex,
        CutModeInstrID,
        CutModeInstrIdList,
        CutModeInstrNum,
        CutModeIsEndo,
        CutModesNames,
    };
    Q_ENUM(SocketRoles)

    explicit SocketModel(QObject *parent = nullptr);
    void init(QList<SockPtr> m_items );

    // QAbstractItemModel interface
public:
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Q_INVOKABLE QVariantMap modeParam(int socketId, int modeIndeex, bool isCoag) const;

    Q_INVOKABLE void qmlSetData(int row, const QVariant &value, const QString& roleName);

    /**
     * @brief Это костыль по дерганью состояний сокетов после ресета модели
     */
    Q_INVOKABLE void recalcCollapsed();

    QStringList modeNames(int socketID, bool isCoag) const;
    QStringList instrumNames(int socketId, int modeIndex, bool isCoag) const;
    QStringList modeNamesIds(int socketID, bool isCoag) const;
    QStringList instrumNamesIds(int socketId, int modeIndex, bool isCoag) const;
    QStringList instrumNamesNums(int socketId, int modeIndex, bool isCoag) const;
    int selectedInstrumIndexByMode(int socketId, int modeIndex, bool isCoag);
    int selectedInstrumIdByMode(int socketId, int modeIndex, bool isCoag);

    InstrPtr getInstrumentById(int id) const;
    void copyCurrentList();

public slots:
    void slotRemoveSubProg();

public:
    void startActivation(int socketId, bool isCut);
    void stopActivation();

    bool commitModeChange(int socketId, int modeINdex, const QVariantMap& param);
    SockPtr socketByName(const QString& socket) const;
    SockPtr socketById(int id) const;
    void expandSocket(int socketId);

    std::map<int, SockPtr>* itemsMap() const { return m_itemsMapPtr; }
    std::map<int, InstrPtr>* instrMap() const { return m_instrMapPtr; }

    void loadProgs( const std::vector<std::map<int, SockPtr >> &itemsMapVect,
                    const std::vector<std::map<int, InstrPtr >> &instrMapVect,
                    bool add = false);

    int subProgIdx() const;
    int subProgCount() const;
    void setSubProgIdx(int newIndex);

    /**
     * @brief getSocketsCopy возвращает копию данных лежащих в модели
     * @details да, и указатели тоже на копии объектов, так надо
     * @return
     */
    std::vector<std::map<int, SockPtr >> getSocketsCopy();

    /**
     * @brief getInstrCopy возвращает копию данных лежащих в модели
     * @details да, и указатели тоже на копии объектов, так надо
     * @return
     */
    std::vector<std::map<int, InstrPtr >> getInstrCopy();

signals:
    void subProgIdxChanged();
    void subProgCountChanged();

//методы
private:
    void removeSubProg(int index);
    void addList(const std::map<int, SockPtr > &itemsMap,
                 const std::map<int, InstrPtr > &newInstrumMap);

    virtual QHash<int, QByteArray> roleNames() const override final;

    int roleIntByName(const QString& name);

    void socketCollapser(int expandedSocket);

    /**
     * @brief pedalRemover удалятор педалей из сокетов
     * @param socketToSkip - айди сокета, который не участвует в удалении ( в него мы педаль добавили)
     * @param pedalToRemove - айди педали, которую удаляем
     */
    void pedalRemover(int socketToSkip, int pedalToRemove);

    /**
     * @brief populateRoles - метамагическая запонялка имён ролей по именам енума ролей
     */
    void populateRoles();
    std::vector<SocketStrings> getDatabaseText();


//поля
private:
    std::map<int, SockPtr>* m_itemsMapPtr = nullptr;
    std::vector<std::map<int, SockPtr >> m_itemsMapVect;

    std::map<int, InstrPtr>* m_instrMapPtr = nullptr;
    std::vector<std::map<int, InstrPtr >> m_instrMapVect;

    int m_subProgIdx = 0;
    int activeSocket = -1;

    QStringList m_socketNames;
    QHash<int, QByteArray> m_roles;
};

#endif // SOCKETMODEL_H
