#include "socketmodel.h"
#include <QQmlEngine>
#include <QTimer>

SocketModel::SocketModel(QObject *parent)
    : QAbstractListModel{parent}
{
    populateRoles();
    connect(this, &SocketModel::dataChanged, this, [this] (const QModelIndex &topLeft,
                                                    const QModelIndex &,
                                                    const QVector<int> &roles) {
        if (!roles.contains(SocketDisplayMode))
            return;
        if (topLeft.data(SocketDisplayMode).toString() == "expanded")
            socketCollapser(topLeft.row());
    });
}

int SocketModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (m_itemsMap == nullptr)
        return 0;
    return m_itemsMap->size();
}

QVariant SocketModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_socketNames.size())
        return QVariant();

    if (m_itemsMap == nullptr)
        return QVariant();

    const auto socketIter = m_itemsMap->find(index.row());
    if (socketIter == m_itemsMap->end())
        return QVariant();
    if (socketIter->second.isNull())
        return QVariant();

    const SOCKET& socketItem = *(socketIter->second);

    switch (role) {
    case Qt::DisplayRole:
        return QVariant();
    case SocketEnabled:
        return socketItem.socketStatus() >= SOCKET::S_ENABLED;
    case SocketAllowed:
        return socketItem.socketStatus() != SOCKET::S_OFF;
    case SocketStatus:
        return static_cast<int>(socketItem.socketStatus());
    case SocketDisplayMode:
    {
        switch (socketItem.displayMode()) {
        case SOCKET::S_COLLAPSED:
            return "collapsed";
        case SOCKET::S_EXPANDED:
            return "expanded";
        default:
            break;
        }
        break;
    }
    case SocketName:
        return socketItem.socketName();
    case SocketPolarity:
        return socketItem.socketType();
    case SocketPedal:
        return socketItem.pedal();
    case CoagModeIndex:
        return socketItem.coagModeIndex();
    case CoagModeId:
        return socketItem.coagModeId();
    case CoagModeName:
        return socketItem.coagModeName();
    case CoagModePower:
        return socketItem.coagModePower();
    case CoagModeMinPower:
        if (socketItem.curCoagMode().isNull())
            return -1;
        return socketItem.curCoagMode()->minimumPower();
    case CoagModeMaxPower:
        if (socketItem.curCoagMode().isNull())
            return -1;
        return socketItem.curCoagMode()->maximumPower();

    case CoagModeInstrName:
    {
        auto iter = m_instrumMap.find(socketItem.curCoagMode()->selectedInstrId());
        if (iter != m_instrumMap.end())
            return iter->second->name();
        else
        return tr("Не выбран");
    }
    case CoagModeInstrID:
        if (socketItem.curCoagMode().isNull())
            return -1;
        return socketItem.curCoagMode()->selectedInstrId();

    case CoagModeInstrImage:
        return QVariant();
        //тут можно возращеть строку с именем или даже целиком с нужным ImageProvider
    case CoagModeInstrIndex:
        if (socketItem.curCoagMode().isNull())
            return -1;
        return socketItem.curCoagMode()->selectedInstrIndex();

    case CutModeIndex:
        return socketItem.cutModeIndex();
    case CutModeId:
        return socketItem.cutModeId();
    case CutModeName:
        return socketItem.cutModeName();
    case CutModePower:
        return socketItem.cutModePower();
    case CutModeMinPower:
        if (socketItem.curCutMode().isNull())
            return -1;
        return socketItem.curCutMode()->minimumPower();
    case CutModeMaxPower:
        if (socketItem.curCutMode().isNull())
            return -1;
        return socketItem.curCutMode()->maximumPower();
    case CoagModesNames:
        return socketItem.coagModeNames();
    case CutModesNames:
        return socketItem.cutModeNames();
    case CutModeInstrName:
    {
        if (socketItem.curCutMode().isNull())
            return QString();
        auto iter = m_instrumMap.find(socketItem.curCutMode()->selectedInstrId());
        if (iter != m_instrumMap.end())
            return iter->second->name();
        else
            return QString();
    }
    case CutModeInstrID:
        if (socketItem.curCutMode().isNull())
            return -1;
        return socketItem.curCutMode()->selectedInstrId();
    case CutModeInstrImage:
        return QVariant();
    case CutModeInstrIndex:
        if (socketItem.curCutMode().isNull())
            return -1;
        return socketItem.curCutMode()->selectedInstrIndex();
    default:
        return QVariant();
    }

    return QVariant();
}

bool SocketModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    if (index.row() >= m_socketNames.size())
        return false;

    if (m_itemsMap == nullptr)
        return false;

    if (m_itemsMap->find((index.row())) == m_itemsMap->end())
        return false;
    auto socketIter = (*m_itemsMap)[(index.row())];

    SOCKET& socketItem = *(socketIter);

    switch (role) {
    case SocketEnabled:
    {
        if (socketItem.socketStatus() == SOCKET::S_OFF)
            return false;
        bool isOk = value.toBool();
        socketItem.setSocketStatus(isOk ? SOCKET::S_ENABLED : SOCKET::S_DISABLED);
        return true;
    }
    case SocketDisplayMode:
    {
        bool isOk = false;
        int mode = value.toInt(&isOk);
        if (!isOk)
            return isOk;
        if (mode < SOCKET::S_COLLAPSED || mode > SOCKET::S_EXPANDED)
            return false;
        // if (socketItem.displayMode() == mode)
        //     return true;
        socketItem.setDisplayMode(static_cast<SOCKET::SocDisplayMode>(mode));
        return true;
    }
    case CoagModeIndex:
        return socketItem.setCoagModeIndex(value.toInt());
    case CutModeIndex:
        return socketItem.setCutModeIndex(value.toInt());
    case CutModePower:
        return socketItem.setCutModePower(value.toInt());
    case CoagModePower:
        return socketItem.setCoagModePower(value.toInt());
    default:
        break;
    }
    return false;
}

QVariantMap SocketModel::modeParam(int socketId, int modeIndeex, bool isCoag) const
{
    if (m_itemsMap == nullptr)
        return QVariantMap{};

    auto iter = m_itemsMap->find(socketId);
    if (iter == m_itemsMap->end() || iter->second.isNull())
        return QVariantMap{};

    return iter->second->getMode(modeIndeex, isCoag)->params();
}

void SocketModel::qmlSetData(int row, const QVariant &value, const QString &roleName)
{
    int role = roleIntByName(roleName);
    if (role == -1)
        return;
    QModelIndex idx = createIndex(row,0);
    if (setData(idx,value,role)) {
        emit dataChanged(idx,idx,{role});
    }
}

void SocketModel::recalcCollapsed()
{
    int role = roleIntByName("socketdisplaymode");
    qmlSetData(0, SOCKET::S_EXPANDED, "socketdisplaymode");
    qmlSetData(0, SOCKET::S_COLLAPSED, "socketdisplaymode");
}

QStringList SocketModel::modeNames(int socketID, bool isCoag) const
{
    if (m_itemsMap == nullptr)
        return QStringList{};

    auto iter = m_itemsMap->find(socketID);
    if (iter == m_itemsMap->end() || iter->second.isNull())
        return QStringList{};
    return isCoag ? iter->second->coagModeNames() : iter->second->cutModeNames();
}

QStringList SocketModel::instrumNames(int socketId, int modeIndex, bool isCoag)
{
    if (m_itemsMap == nullptr)
        return QStringList{};

    auto iter = m_itemsMap->find(socketId);
    if (iter == m_itemsMap->end() || iter->second.isNull())
        return {};

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, InstrInfo>& compatible = mode->InstrConstraints();

    QStringList names;
    for (const auto&[key, item] : compatible) {
        const auto instIter = m_instrumMap.find(item.id);
        if (instIter != m_instrumMap.end())
            names.append(instIter->second->name());
    }
    return names;
}

QStringList SocketModel::modeNamesIds(int socketID, bool isCoag) const
{
    if (m_itemsMap == nullptr)
        return QStringList{};

    auto iter = m_itemsMap->find(socketID);
    if (iter == m_itemsMap->end() || iter->second.isNull())
        return QStringList{};
    return isCoag ? iter->second->coagModeNamesIds() : iter->second->cutModeNamesIds();
}

QStringList SocketModel::instrumNamesIds(int socketId, int modeIndex, bool isCoag)
{
    if (m_itemsMap == nullptr)
        return QStringList{};

    auto iter = m_itemsMap->find(socketId);
    if (iter == m_itemsMap->end() || iter->second.isNull())
        return {};

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, InstrInfo>& compatible = mode->InstrConstraints();

    QStringList names;
    for (const auto&[key, item] : compatible) {
        const auto instIter = m_instrumMap.find(item.id);
        if (instIter != m_instrumMap.end())
            names.append(QString("%1").arg(instIter->second->id()));
    }
    return names;
}

int SocketModel::selectedInstrumIdByMode(int socketId, int modeIndex, bool isCoag)
{
    if (socketId >= m_socketNames.size())
        return -1;

    if (m_itemsMap == nullptr)
        return -1;

    const auto socketIter = m_itemsMap->find(socketId);
    if (socketIter == m_itemsMap->end())
        return -1;
    if (socketIter->second.isNull())
        return -1;

    const SOCKET& socketItem = *(socketIter->second);
    CSurgModePtr ptr =  socketItem.getMode(modeIndex, isCoag);
    if (ptr.isNull())
        return -1;
    return ptr->selectedInstrId();
}

int SocketModel::selectedInstrumIndexByMode(int socketId, int modeIndex, bool isCoag)
{
    if (socketId >= m_socketNames.size())
        return -1;

    if (m_itemsMap == nullptr)
        return -1;

    const auto socketIter = m_itemsMap->find(socketId);
    if (socketIter == m_itemsMap->end())
        return -1;
    if (socketIter->second.isNull())
        return -1;

    const SOCKET& socketItem = *(socketIter->second);
    CSurgModePtr ptr =  socketItem.getMode(modeIndex, isCoag);
    if (ptr.isNull())
        return -1;
    return ptr->selectedInstrIndex();
}

bool SocketModel::commitModeChange(int socketId, int modeIndex, const QVariantMap &param)
{
    if (m_itemsMap == nullptr)
        return false;

    auto iter = m_itemsMap->find(socketId);
    if (iter == m_itemsMap->end() || iter->second.isNull())
        return false;

    QModelIndex idx = createIndex(socketId, 0);

    bool isModeCoag = param.value("iscoag").toBool();
    bool res = false;
    QVector<int> roles;

    if (isModeCoag) {
        if (iter->second->setCoagModeIndex(modeIndex)) {
            roles.append(CoagModeIndex);
            roles.append(CoagModeName);
            roles.append(CoagModeId);
            roles.append(CoagModeInstrID);
            roles.append(CoagModeInstrIndex);
            roles.append(CoagModeInstrName);
            roles.append(CoagModeMaxPower);
            roles.append(CoagModePower);
            // roles.append(Coa);
            res = true;
        }
        if (iter->second->setCoagModePower(param.value("currentpower").toInt())) {
            roles.append(CoagModePower);
            res = true;
        }
        if (iter->second->setInstrumIndex(param.value("instrindex").toInt(), true)) {
            roles.append(CoagModeInstrName);
            roles.append(CoagModeInstrIndex);
            roles.append(CoagModeInstrID);
            res = true;
        }
        if (res)
            emit dataChanged(idx, idx, roles);
        return res;
    //     break;
    // case SOCKET::CUT:
    } else {
        if (iter->second->setCutModeIndex(modeIndex)) {
            roles.append(CutModeIndex);
            roles.append(CutModeName);
            roles.append(CutModeId);
            roles.append(CutModeInstrID);
            roles.append(CutModeInstrIndex);
            roles.append(CutModeInstrName);
            roles.append(CutModeMaxPower);
            roles.append(CutModePower);
            res = true;
        }
        if (iter->second->setCutModePower(param.value("currentpower").toInt())) {
            roles.append(CutModePower);
            res = true;
        }
        if (iter->second->setInstrumIndex(param.value("instrindex").toInt(), false)) {
            roles.append(CutModeInstrName);
            roles.append(CutModeInstrIndex);
            roles.append(CutModeInstrID);
            res = true;
        }
        if (res)
            emit dataChanged(idx, idx, roles);
        return res;
    // default:
    //     break;
    }
    return false;
}

SockPtr SocketModel::socketByName(const QString &socket) const
{
    SockPtr itemPtr = nullptr;

    if (m_itemsMap == nullptr)
        return itemPtr;

    for (auto& item : (*m_itemsMap)) {
        if (item.second->socketName() == socket) {
            itemPtr = item.second;
            break;
        }
    }
    return itemPtr;
}

SockPtr SocketModel::socketById(int id) const
{
    if (m_itemsMap == nullptr)
        return nullptr;

    auto iter = m_itemsMap->find(id);
    if (iter == m_itemsMap->end())
        return nullptr;
    return iter->second;
}

void SocketModel::setInstrumMap(const std::map<int, QSharedPointer<Instrument>> &newInstrumMap)
{
    m_instrumMap.clear();
    m_instrumMap = newInstrumMap;
}

int SocketModel::roleIntByName(const QString &name)
{
    const QHash<int, QByteArray>& hash = m_roles;
    QString nameArr = name.toLower();
    for (auto iter = hash.begin(); iter != hash.end(); ++iter) {
        if (iter.value() == nameArr)
            return iter.key();
    }
    return -1;
}

void SocketModel::socketCollapser(int expandedSocket)
{
    for (int i = 0; i < m_itemsMap->size(); ++i) {
        if(i == expandedSocket)
            continue;
        qmlSetData(i, SOCKET::S_COLLAPSED, "socketdisplaymode");
    }
}

void SocketModel::populateRoles()
{
    QMetaEnum metaEnum = QMetaEnum::fromType<SocketRoles>();
    // static QHash<int, QByteArray> roles;
    m_roles.clear();
    for (int k = 0; k < metaEnum.keyCount(); k++)
    {
        int roleKey = metaEnum.value(k);
        QString roleName = metaEnum.valueToKey(roleKey);
        roleName = roleName.toLower();
        m_roles.insert(roleKey, roleName.toUtf8());
    }
}

void SocketModel::setItemsMap(const std::map<int, SockPtr > &newItemsMap, bool add)
{
    beginResetModel();
    m_itemsMap = nullptr;
    if (!add) m_itemsMapVect.clear();
    m_itemsMapVect.push_back(newItemsMap);
    // setCurrentProgSubIndex(m_itemsMapVect.size() - 1);

    m_subProgIdx = m_itemsMapVect.size() - 1;
    m_itemsMap = &(m_itemsMapVect.at(m_subProgIdx));
    m_socketNames.clear();
    for (int i = SOCKET::BIPOLAR_1; i <= SOCKET::MONOPOLAR_2; ++i) {
        const auto iter = m_itemsMap->find(i - 1);
        if (iter == m_itemsMap->cend())
            continue;
        m_socketNames.append(iter->second->socketName());
    }
    endResetModel();
}

void SocketModel::setItemsMapVector(const std::vector<std::map<int, SockPtr> > &newItemsMapVector)
{
    beginResetModel();
    m_itemsMap = nullptr;
    m_itemsMapVect.clear();
    m_itemsMapVect = newItemsMapVector;

    m_subProgIdx = 0;
    m_itemsMap = &(m_itemsMapVect.at(m_subProgIdx));
    m_socketNames.clear();
    for (int i = SOCKET::BIPOLAR_1; i <= SOCKET::MONOPOLAR_2; ++i) {
        const auto iter = m_itemsMap->find(i - 1);
        if (iter == m_itemsMap->cend())
            continue;
        m_socketNames.append(iter->second->socketName());
    }

    endResetModel();
    m_subProgCount = m_itemsMapVect.size();
    emit subProgCountChanged();
}


QHash<int, QByteArray> SocketModel::roleNames() const
{
    //грёбаная мета-магия, но это приятное
    return m_roles;
}

int SocketModel::subProgIdx() const
{
    return m_subProgIdx;
}

void SocketModel::setSubProgIdx(int newIndex)
{
    if (newIndex >= m_itemsMapVect.size() || newIndex < 0)
        return;
    // if (newIndex == m_curMapIdx)
    //     return true;

    beginResetModel();
    m_subProgIdx = newIndex;
    m_itemsMap = &(m_itemsMapVect.at(m_subProgIdx));
    m_socketNames.clear();
    for (int i = SOCKET::BIPOLAR_1; i <= SOCKET::MONOPOLAR_2; ++i) {
        const auto iter = m_itemsMap->find(i - 1);
        if (iter == m_itemsMap->cend())
            continue;
        m_socketNames.append(iter->second->socketName());
    }
    // m_socketNames.append(m_itemsMap->SOCKET::BIPOLAR_1);
    endResetModel();

    emit subProgIdxChanged();
}

int SocketModel::subProgCount() const
{
    return m_subProgCount;
}
