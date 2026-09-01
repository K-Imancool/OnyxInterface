#include "socketmodel.h"
#include <QQmlEngine>
#include <QTimer>
#include <QDebug>
// #include <algorithm>

namespace {
template<typename Key, typename Value>
void uniteMaps(
    std::map<Key, Value>& destination,
    const std::map<Key, Value>& source) {
    for (const auto& [key, value] : source) {
        auto it = destination.find(key);
        if (it != destination.end()) {
            continue;
        } else {
            destination[key] = value;
        }
    }
}
}

SocketModel::SocketModel(QObject *parent)
    : QAbstractListModel{parent}
{
    populateRoles();
    connect(this, &SocketModel::dataChanged, this, [this] (const QModelIndex &topLeft,
                                                    const QModelIndex &,
                                                    const QVector<int> &roles) {
        if (roles.contains(SocketDisplayMode)) {
            if (topLeft.data(SocketDisplayMode).toString() == "expanded")
                socketCollapser(topLeft.row());
        }
        if (roles.contains(SocketPedal)) {
            int pedalToRemove = topLeft.data(SocketPedal).toInt();
            if (pedalToRemove != 0) {
                pedalRemover(topLeft.row(), pedalToRemove);
            }
        }
    });
}

int SocketModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (m_itemsMapPtr == nullptr)
        return 0;
    return m_itemsMapPtr->size();
}

SocketModel::SocketResolveResultConst SocketModel::resolveSocket(int displayRow) const
{
    SocketResolveResultConst res;
    if (displayRow < 0) {
        return res;
    }

    if (!m_endoProgramView) {
        if (!m_itemsMapPtr || !m_instrMapPtr) {
            return res;
        }
        if (m_itemsMapPtr->find(displayRow) == m_itemsMapPtr->end()) {
            return res;
        }
        res.itemsMap = m_itemsMapPtr;
        res.instrMap = m_instrMapPtr;
        res.socketId = displayRow;
        return res;
    }

    if (m_itemsMapVect.size() < 2 || m_instrMapVect.size() < 2) {
        return res;
    }

    int pageIndex = 0;
    int socketId = 2;
    switch (displayRow) {
    case 0:
        pageIndex = 1;
        socketId = 2;
        break;
    case 1:
        pageIndex = 1;
        socketId = 3;
        break;
    case 2:
        pageIndex = 0;
        socketId = 2;
        break;
    case 3:
        pageIndex = 0;
        socketId = 3;
        break;
    default:
        return res;
    }

    const auto& itemsMap = m_itemsMapVect.at(static_cast<size_t>(pageIndex));
    const auto& instrMap = m_instrMapVect.at(static_cast<size_t>(pageIndex));
    if (itemsMap.find(socketId) == itemsMap.end()) {
        return res;
    }

    res.itemsMap = &itemsMap;
    res.instrMap = &instrMap;
    res.socketId = socketId;
    return res;
}

SocketModel::SocketResolveResult SocketModel::resolveSocket(int displayRow)
{
    const auto constRes = static_cast<const SocketModel*>(this)->resolveSocket(displayRow);
    SocketResolveResult res;
    res.itemsMap = const_cast<std::map<int, SockPtr>*>(constRes.itemsMap);
    res.instrMap = const_cast<std::map<int, InstrPtr>*>(constRes.instrMap);
    res.socketId = constRes.socketId;
    return res;
}

void SocketModel::rebuildSocketNames()
{
    m_socketNames.clear();

    if (m_endoProgramView) {
        for (int row = 0; row < 4; ++row) {
            const auto resolved = resolveSocket(row);
            if (!resolved.itemsMap) {
                continue;
            }
            const auto iter = resolved.itemsMap->find(resolved.socketId);
            if (iter == resolved.itemsMap->cend() || iter->second.isNull()) {
                continue;
            }
            m_socketNames.append(iter->second->socketName());
        }
        return;
    }

    if (!m_itemsMapPtr) {
        return;
    }
    for (int i = Onyx::BIPOLAR_1; i <= Onyx::MONOPOLAR_2; ++i) {
        const auto iter = m_itemsMapPtr->find(i - 1);
        if (iter == m_itemsMapPtr->cend()) {
            continue;
        }
        m_socketNames.append(iter->second->socketName());
    }
}

QVariant SocketModel::data(const QModelIndex &index, int role) const
{
    auto defaultValueByRole = [role]() -> QVariant {
        switch (role) {
        case SocketStatus:
            return static_cast<int>(Onyx::S_OFF);
        case SocketEnabled:
        case SocketAllowed:
        case CoagModeIsEndo:
        case CutModeIsEndo:
            return false;
        case SocketDisplayMode:
            return QStringLiteral("collapsed");
        case SocketName:
        case CoagModeName:
        case CoagModeBrief:
        case CoagModeDescript:
        case CoagModeInstrName:
        case CutModeName:
        case CutModeBrief:
        case CutModeDescript:
        case CutModeInstrName:
            return QString();
        case SocketPolarity:
        case SocketPedal:
        case CoagModeIndex:
        case CoagModeId:
        case CoagModeNum:
        case CoagModePower:
        case CoagModeMinPower:
        case CoagModeMaxPower:
        case CoagModeInstrIndex:
        case CoagModeInstrID:
        case CoagModeInstrNum:
        case CutModeIndex:
        case CutModeId:
        case CutModeNum:
        case CutModePower:
        case CutModeMinPower:
        case CutModeMaxPower:
        case CutModeInstrIndex:
        case CutModeInstrID:
        case CutModeInstrNum:
            return 0;
        case SocketAllowedPedal:
            return QVariantList{};
        case CoagModeIdList:
        case CoagModeInstrIdList:
        case CoagModesNames:
        case CutModeIdList:
        case CutModeInstrIdList:
        case CutModesNames:
            return QStringList{};
        default:
            return QVariant();
        }
    };

    if (!index.isValid())
        return defaultValueByRole();

    if (index.row() >= m_socketNames.size())
        return defaultValueByRole();

    const auto resolved = resolveSocket(index.row());
    if (!resolved.itemsMap || !resolved.instrMap)
        return defaultValueByRole();

    const auto socketIter = resolved.itemsMap->find(resolved.socketId);
    if (socketIter == resolved.itemsMap->end())
        return defaultValueByRole();
    if (socketIter->second.isNull())
        return defaultValueByRole();

    const SOCKET& socketItem = *(socketIter->second);

    switch (role) {
    case Qt::DisplayRole:
        return QVariant();
    case SocketEnabled:
        return socketItem.socketStatus() >= Onyx::S_ENABLED;
    case SocketAllowed:
        return socketItem.socketStatus() != Onyx::S_OFF;
    case SocketStatus:
        return static_cast<int>(socketItem.socketStatus());
    case SocketDisplayMode:
    {
        switch (socketItem.displayMode()) {
        case Onyx::S_COLLAPSED:
            return "collapsed";
        case Onyx::S_EXPANDED:
            return "expanded";
        default:
            break;
        }
        break;
    }
    case SocketAllowedPedal:
    {
        QList<int> tmp = socketItem.allowedPedals();
        QVariantList varList;

        for (const auto& item : tmp)
            varList.append(item);

        return varList;
    }
    case SocketName:
        return socketItem.socketName();
    case SocketPolarity:
        return socketItem.socketType();
    case SocketPedal:
        return socketItem.pedal();
    case SocketUartInfo:
    {
        QVariant res;
        res.setValue<Onyx::SocketState>(socketItem.getInfo());
        return res;
    }
    case CoagModeIndex:
        return socketItem.coagModeIndex();
    case CoagModeId:
        return socketItem.coagModeId();
    case CoagModeIdList:
        return socketItem.coagModeNamesIds();
    case CoagModeNum:
        if (socketItem.curCoagMode().isNull())
            return 0;
        return socketItem.curCoagMode()->num();
    case CoagModeBrief:
        if (socketItem.curCoagMode().isNull())
            return QString();
        return socketItem.curCoagMode()->brief();
    case CoagModeDescript:
        if (socketItem.curCoagMode().isNull())
            return QString();
        return socketItem.curCoagMode()->descript();
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
        if (socketItem.curCoagMode().isNull())
            return "";
        const int instrId = socketItem.curCoagMode()->selectedInstrId();
        if (instrId <= 0 || instrId == 1000)
            return tr("Другой инструмент");
        auto iter = resolved.instrMap->find(instrId);
        if (iter != resolved.instrMap->end())
            return iter->second->name();
        else
            return tr("Другой инструмент");
    }
    case CoagModeInstrID:
        if (socketItem.curCoagMode().isNull())
            return -1;
        return socketItem.curCoagMode()->selectedInstrId();
    case CoagModeInstrIdList:
    {
        QStringList res;
        const auto& instrs = socketItem.curCoagMode()->InstrConstraints();
        for (const auto& [key, item] : instrs) {
            res.append(QString::number(item.id));
        }
        return res;
    }
//    case CoagInstrIdList:
//    {
//        QStringList res;
//        std::map<int, Onyx::InstrInfo> all;
//        int lim = socketItem.coagModeCount();
//        for (int mode = 0; mode < lim; ++mode) {
//            const auto& modePtr = socketItem.getMode(mode, true);
//            uniteMaps(all, modePtr->InstrConstraints() );
//        }
//        for (const auto& [key, item] : all) {
//            res.append(QString::number(item.id));
//        }
//        return res;
//    }
    case CoagModeInstrNum:
    {
        if (socketItem.curCoagMode().isNull()
            || resolved.instrMap == nullptr)
            return -1;
        
        int instrId = socketItem.curCoagMode()->selectedInstrId();
        // Если выбран пункт "НЕ ВЫБРАН" (ID = 1000), возвращаем 1000
        if (instrId == 1000) {
            return 1000;
        }
        
        auto iter = resolved.instrMap->find(instrId);
        if (iter != resolved.instrMap->end())
            return iter->second->legacyNumber();
        else
            return -1;
    }
    case CoagModeInstrImage:
        return QVariant();
        //тут можно возращеть строку с именем или даже целиком с нужным ImageProvider
    case CoagModeInstrIndex:
        if (socketItem.curCoagMode().isNull()) {
            return -1;
        }
        return socketItem.curCoagMode()->selectedInstrIndex();

    case CutModeIndex:
        return socketItem.cutModeIndex();
    case CutModeId:
        return socketItem.cutModeId();
    case CutModeIdList:
        return socketItem.cutModeNamesIds();
    case CutModeNum:
        if (socketItem.curCutMode().isNull()) {
            return 0;
        }
        return socketItem.curCutMode()->num();
    case CutModeBrief:
        if (socketItem.curCutMode().isNull()) {
            return QString();
        }
        return socketItem.curCutMode()->brief();
    case CutModeDescript:
        if (socketItem.curCutMode().isNull()) {
            return QString();
        }
        return socketItem.curCutMode()->descript();
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
        if (!resolved.instrMap || socketItem.curCutMode().isNull()) {
            return QString();
        }
        const int instrId = socketItem.curCutMode()->selectedInstrId();
        if (instrId <= 0 || instrId == 1000)
            return tr("Другой инструмент");
        auto iter = resolved.instrMap->find(instrId);
        if (iter != resolved.instrMap->end()) {
            return iter->second->name();
        } else {
            return tr("Другой инструмент");
        }
    }
    case CutModeInstrID:
        if (socketItem.curCutMode().isNull()) {
            return -1;
        }
        return socketItem.curCutMode()->selectedInstrId();
    case CutModeInstrIdList:
    {
        QStringList res;
        const auto& instrs = socketItem.curCutMode()->InstrConstraints();
        for (const auto& [key, item] : instrs) {
            res.append(QString::number(item.id));
        }
        return res;
    }
//    case CutInstrIdList:
//    {
//        QStringList res;
//        std::map<int, Onyx::InstrInfo> all;
//        int lim = socketItem.cutModeCount();
//        for (int mode = 0; mode < lim; ++mode) {
//            const auto& modePtr = socketItem.getMode(mode, false);
//            uniteMaps(all,modePtr->InstrConstraints() );
//        }
//        for (const auto& [key, item] : all) {
//            res.append(QString::number(item.id));
//        }
//        return res;
//    }
    case CutModeInstrNum:
    {
        if (socketItem.curCutMode().isNull()
                || resolved.instrMap == nullptr) {
            return -1;
        }
        
        int instrId = socketItem.curCutMode()->selectedInstrId();
        // Если выбран пункт "НЕ ВЫБРАН" (ID = 1000), возвращаем 1000
        if (instrId == 1000) {
            return 1000;
        }
        
        auto iter = resolved.instrMap->find(instrId);
        if (iter != resolved.instrMap->end()) {
            return iter->second->legacyNumber();
        } else {
            return -1;
        }
    }
    case CutModeInstrImage:
        return QVariant();
    case CutModeInstrIndex:
        if (socketItem.curCutMode().isNull()) {
            return -1;
        }
        return socketItem.curCutMode()->selectedInstrIndex();
    case CoagModeIsEndo:
        if (socketItem.curCoagMode().isNull()) {
            return false;
        }
        return socketItem.curCoagMode()->isEndo();
    case CutModeIsEndo:
        if (socketItem.curCutMode().isNull()) {
            return false;
        }
        return socketItem.curCutMode()->isEndo();
    default:
        return defaultValueByRole();
    }

    return defaultValueByRole();
}
//

bool SocketModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    if (index.row() >= m_socketNames.size()) {
        return false;
    }

    auto resolved = resolveSocket(index.row());
    if (!resolved.itemsMap || !resolved.instrMap) {
        return false;
    }
    if (resolved.itemsMap->find(resolved.socketId) == resolved.itemsMap->end()) {
        return false;
    }
    auto socketIter = (*resolved.itemsMap)[resolved.socketId];

    SOCKET& socketItem = *(socketIter);

    switch (role) {
    case SocketStatus:
    {
        socketItem.setSocketStatus(static_cast<Onyx::SocStatus>(value.toUInt()) );
        return true;
        break;
    }
    case SocketEnabled:
    {
        if (socketItem.socketStatus() == Onyx::S_OFF) {
            return false;
        }
        bool isOk = value.toBool();
        socketItem.setSocketStatus(isOk ? Onyx::S_ENABLED : Onyx::S_DISABLED);
        return true;
    }
    case SocketDisplayMode:
    {
        bool isOk = false;
        int mode = value.toInt(&isOk);
        if (!isOk) {
            return isOk;
        }
        return socketItem.setDisplayMode(static_cast<Onyx::SocDisplayMode>(mode));
    }
    case SocketPedal:
    {
        bool isOk = false;
        int ped = value.toInt(&isOk);
        if (!isOk) {
            return isOk;
        }
        return socketItem.setPedal(ped);
    }
    case CoagModeIndex:
        if (socketItem.setCoagModeIndex(value.toInt())) {
            syncBi2HandlePedal(index.row());
            return true;
        }
        return false;
    case CutModeIndex:
        if (socketItem.setCutModeIndex(value.toInt())) {
            return true;
        }
        return false;
    case CutModePower:
        if (socketItem.setCutModePower(value.toInt())) {
            return true;
        }
        return false;
    case CoagModePower:
        if (socketItem.setCoagModePower(value.toInt())) {
            return true;
        }
        return false;
    default:
        break;
    }
    return false;
}

QVariantMap SocketModel::modeParam(int socketId, int modeIndeex, bool isCoag) const
{
    const auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return QVariantMap{};
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return QVariantMap{};
    }

    return iter->second->getMode(modeIndeex, isCoag)->params();
}

void SocketModel::qmlSetData(int row, const QVariant &value, const QString &roleName)
{
    int role = roleIntByName(roleName);
    if (role == -1) {
        return;
    }
    QModelIndex idx = createIndex(row,0);

    if (setData(idx,value,role)) {
        emit dataChanged(idx,idx,{role});
    }
}

bool SocketModel::isArgonMode(int modeId) const
{
    if (modeId <= 0 || modeId == ESHF::NO_MODE || !m_itemsMapPtr) {
        return false;
    }

    for (const auto& [sockId, sock] : *m_itemsMapPtr) {
        Q_UNUSED(sockId)
        if (sock.isNull()) {
            continue;
        }
        for (bool isCoag : {false, true}) {
            const QStringList names = isCoag ? sock->coagModeNames() : sock->cutModeNames();
            for (int i = 0; i < names.size(); ++i) {
                const CSurgModePtr mode = sock->getMode(i, isCoag);
                if (!mode.isNull() && mode->id() == modeId && mode->isArgon()) {
                    return true;
                }
            }
        }
    }
    return false;
}

QVariantMap SocketModel::otherMonoArgonConflict(int currentSocketId) const
{
    QVariantMap result;
    result.insert(QStringLiteral("conflict"), false);
    result.insert(QStringLiteral("socketId"), -1);
    result.insert(QStringLiteral("socketName"), QString());

    if (currentSocketId != 2 && currentSocketId != 3) {
        return result;
    }

    const int otherSocketId = (currentSocketId == 2) ? 3 : 2;
    const auto resolved = resolveSocket(otherSocketId);
    if (!resolved.itemsMap) {
        return result;
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return result;
    }

    const CSurgModePtr cutMode = iter->second->curCutMode();
    const CSurgModePtr coagMode = iter->second->curCoagMode();
    const bool otherHasArgon = (!cutMode.isNull() && cutMode->isArgon())
            || (!coagMode.isNull() && coagMode->isArgon());
    if (!otherHasArgon) {
        return result;
    }

    result.insert(QStringLiteral("conflict"), true);
    result.insert(QStringLiteral("socketId"), otherSocketId);
    result.insert(QStringLiteral("socketName"), iter->second->socketName());
    return result;
}

bool SocketModel::clearArgonModes(int socketId)
{
    auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap) {
        return false;
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return false;
    }

    SockPtr sock = iter->second;
    QModelIndex idx = createIndex(socketId, 0);
    QVector<int> roles;
    bool changed = false;

    auto clearHalf = [&](bool isCoag) {
        const CSurgModePtr mode = isCoag ? sock->curCoagMode() : sock->curCutMode();
        if (mode.isNull() || !mode->isArgon()) {
            return;
        }
        if (!sock->setModeId(ESHF::NO_MODE, isCoag)) {
            return;
        }
        changed = true;
        if (isCoag) {
            roles.append(CoagModeIndex);
            roles.append(CoagModeId);
            roles.append(CoagModeName);
            roles.append(CoagModeNum);
            roles.append(CoagModeInstrID);
            roles.append(CoagModeInstrIndex);
            roles.append(CoagModeInstrName);
            roles.append(CoagModeInstrNum);
            roles.append(CoagModePower);
            roles.append(CoagModeMaxPower);
            roles.append(CoagModeIsEndo);
        } else {
            roles.append(CutModeIndex);
            roles.append(CutModeId);
            roles.append(CutModeName);
            roles.append(CutModeNum);
            roles.append(CutModeInstrID);
            roles.append(CutModeInstrIndex);
            roles.append(CutModeInstrName);
            roles.append(CutModeInstrNum);
            roles.append(CutModePower);
            roles.append(CutModeMaxPower);
            roles.append(CutModeIsEndo);
        }
    };

    clearHalf(false);
    clearHalf(true);

    if (changed) {
        emit dataChanged(idx, idx, roles);
    }
    return changed;
}

void SocketModel::stripArgonModesFromAll()
{
    beginResetModel();
    for (auto& itemsMap : m_itemsMapVect) {
        for (auto& entry : itemsMap) {
            if (!entry.second.isNull()) {
                entry.second->stripArgonModes();
            }
        }
    }
    endResetModel();
}

void SocketModel::recalcCollapsed()
{
    qmlSetData(0, Onyx::S_EXPANDED, "socketdisplaymode");
    qmlSetData(0, Onyx::S_COLLAPSED, "socketdisplaymode");
}

QStringList SocketModel::modeNames(int socketID, bool isCoag) const
{
    const auto resolved = resolveSocket(socketID);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return QStringList{};
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return QStringList{};
    }
    return isCoag ? iter->second->coagModeNames() : iter->second->cutModeNames();
}

QStringList SocketModel::instrumNames(int socketId, int modeIndex, bool isCoag) const
{
    const auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return QStringList{};
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);

    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return {};
    }

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, Onyx::InstrInfo>& compatible = mode->InstrConstraints();

    QStringList names;
    for (const auto&[key, item] : compatible) {
        const auto instIter = resolved.instrMap->find(item.id);
        if (instIter != resolved.instrMap->end()) {
            names.append(instIter->second->name());
        }
    }
    // Добавляем пункт выбора произвольного/не из списка инструмента
    names.append(tr("Другой инструмент"));
    return names;
}

QStringList SocketModel::modeNamesIds(int socketID, bool isCoag) const
{
    const auto resolved = resolveSocket(socketID);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return QStringList{};
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return QStringList{};
    }
    return isCoag ? iter->second->coagModeNamesIds() : iter->second->cutModeNamesIds();
}

QStringList SocketModel::modeNamesNums(int socketID, bool isCoag) const
{
    const auto resolved = resolveSocket(socketID);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return QStringList{};
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return QStringList{};
    }

    const SockPtr sock = iter->second;
    const QStringList names = isCoag ? sock->coagModeNames() : sock->cutModeNames();
    QStringList nums;
    nums.reserve(names.size());
    for (int i = 0; i < names.size(); ++i) {
        const CSurgModePtr mode = sock->getMode(i, isCoag);
        nums.append(mode.isNull() ? QStringLiteral("0") : QString::number(mode->num()));
    }
    return nums;
}

QStringList SocketModel::instrumNamesIds(int socketId, int modeIndex, bool isCoag) const
{
    const auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return QStringList{};
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return {};
    }

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, Onyx::InstrInfo>& compatible = mode->InstrConstraints();

    QStringList names;
    for (const auto&[key, item] : compatible) {
        const auto instIter = resolved.instrMap->find(item.id);
        if (instIter != resolved.instrMap->end()) {
            names.append(QString("%1").arg(instIter->second->id()));
        }
    }
    // Добавляем ID для пункта "НЕ ВЫБРАН"
    names.append("1000");
    return names;
}

QStringList SocketModel::instrumNamesNums(int socketId, int modeIndex, bool isCoag) const
{
    const auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return QStringList{};
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return {};
    }

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, Onyx::InstrInfo>& compatible = mode->InstrConstraints();

    QStringList nums;
    for (const auto&[key, item] : compatible) {
        const auto instIter = resolved.instrMap->find(item.id);
        if (instIter != resolved.instrMap->end()) {
            nums.append(QString("%1").arg(instIter->second->legacyNumber()));
        }
    }
    // Добавляем Num для пункта "НЕ ВЫБРАН"
    nums.append("1000");
    return nums;
}

int SocketModel::selectedInstrumIndexByMode(int socketId, int modeIndex, bool isCoag)
{
    if (socketId >= m_socketNames.size()) {
        return -1;
    }

    const auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return -1;
    }
    const auto socketIter = resolved.itemsMap->find(resolved.socketId);
    if (socketIter == resolved.itemsMap->end()) {
        return -1;
    }
    if (socketIter->second.isNull()) {
        return -1;
    }

    const SOCKET& socketItem = *(socketIter->second);
    CSurgModePtr ptr =  socketItem.getMode(modeIndex, isCoag);
    if (ptr.isNull()) {
        return -1;
    }
    return ptr->selectedInstrIndex();
}


int SocketModel::selectedInstrumIdByMode(int socketId, int modeIndex, bool isCoag)
{
    if (socketId >= m_socketNames.size()) {
        return -1;
    }

    const auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return -1;
    }
    const auto socketIter = resolved.itemsMap->find(resolved.socketId);
    if (socketIter == resolved.itemsMap->end()) {
        return -1;
    }
    if (socketIter->second.isNull()) {
        return -1;
    }

    const SOCKET& socketItem = *(socketIter->second);
    CSurgModePtr ptr =  socketItem.getMode(modeIndex, isCoag);
    if (ptr.isNull()) {
        return -1;
    }
    return ptr->selectedInstrId();
}

InstrPtr SocketModel::getInstrumentById(int id) const
{
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return nullptr;
    }
    auto iter = m_instrMapPtr->find(id);
    if (iter != m_instrMapPtr->cend()) {
        return iter->second;
    }
    return nullptr;
}

QString SocketModel::modeNameById(int id) const
{
    const QString idText = QString::number(id);
    for (const auto &itemsMap : m_itemsMapVect) {
        for (const auto &item : itemsMap) {
            if (item.second.isNull()) {
                continue;
            }

            for (const bool isCoag : {false, true}) {
                const QStringList ids = isCoag
                        ? item.second->coagModeNamesIds()
                        : item.second->cutModeNamesIds();
                const int index = ids.indexOf(idText);
                if (index < 0) {
                    continue;
                }

                const QStringList names = isCoag
                        ? item.second->coagModeNames()
                        : item.second->cutModeNames();
                if (index < names.size()) {
                    return names.at(index);
                }
            }
        }
    }
    return {};
}

QString SocketModel::instrumentNameById(int id) const
{
    if (id <= 0 || id == 1000) {
        return tr("Другой инструмент");
    }

    for (const auto &instruments : m_instrMapVect) {
        const auto instrument = instruments.find(id);
        if (instrument != instruments.cend() && !instrument->second.isNull()) {
            return instrument->second->name();
        }
    }
    return {};
}

void SocketModel::copyCurrentList()
{
    if (m_subProgIdx < 0
        || m_subProgIdx >= static_cast<int>(m_itemsMapVect.size())
        || m_subProgIdx >= static_cast<int>(m_instrMapVect.size())) {
        qWarning() << "copyCurrentList: invalid sub program index" << m_subProgIdx;
        return;
    }

    std::map<int, SockPtr> itemsMapCopy;
    const auto &sourceItemsMap = m_itemsMapVect.at(m_subProgIdx);
    for (const auto &item : sourceItemsMap) {
        if (item.second.isNull()) {
            continue;
        }
        // Важно: создаем новый объект SOCKET, а не копируем QSharedPointer.
        itemsMapCopy.insert({item.first, SockPtr::create(*item.second)});
    }

    std::map<int, InstrPtr> instrMapCopy;
    const auto &sourceInstrMap = m_instrMapVect.at(m_subProgIdx);
    for (const auto &instr : sourceInstrMap) {
        if (instr.second.isNull()) {
            continue;
        }
        instrMapCopy.insert({instr.first, InstrPtr::create(*instr.second)});
    }

    loadProgs({itemsMapCopy}, {instrMapCopy}, true);
}


void SocketModel::slotRemoveSubProg()
{
    if (m_endoProgramView) {
        return;
    }
    if (m_itemsMapVect.size() > 1) {
        removeSubProg(m_subProgIdx);
    }
}


void SocketModel::startActivation(int socketId, bool isCut)
{
    const quint64 activationSequence = ++m_activationSequence;
    QTimer::singleShot(0, this, [this, socketId, isCut, activationSequence]() {
        if (activationSequence != m_activationSequence) {
            return;
        }

        expandSocket(socketId);
        // Сначала сбрасываем статус, чтобы QML увидел изменение при повторной активации
        Onyx::SocStatus newStatus = isCut ? Onyx::S_ACTIVE_CUT : Onyx::S_ACTIVE_COAG;
        if (m_itemsMapPtr && socketId < static_cast<int>(m_itemsMapPtr->size())) {
            auto iter = m_itemsMapPtr->find(socketId);
            if (iter != m_itemsMapPtr->end() && !iter->second.isNull()) {
                // Если статус уже установлен, сначала сбрасываем его
                if (iter->second->socketStatus() == newStatus) {
                    qmlSetData(socketId, Onyx::S_ENABLED, "socketstatus");
                    // Даём время QML обработать изменение
                    QTimer::singleShot(10, this, [this, socketId, newStatus, activationSequence]() {
                        if (activationSequence != m_activationSequence) {
                            return;
                        }
                        qmlSetData(socketId, newStatus, "socketstatus");
                    });
                } else {
                    qmlSetData(socketId, newStatus, "socketstatus");
                }
            }
        }
    });
}

void SocketModel::stopActivation()
{
    ++m_activationSequence;

    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return ;
    }
    if (m_endoProgramView) {
        for (int row = 0; row < 4; ++row) {
            const auto resolved = resolveSocket(row);
            if (!resolved.itemsMap) {
                continue;
            }
            const auto iter = resolved.itemsMap->find(resolved.socketId);
            if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
                continue;
            }
            if (iter->second->socketStatus() == Onyx::S_ACTIVE_CUT
                || iter->second->socketStatus() == Onyx::S_ACTIVE_COAG) {
                qmlSetData(row, Onyx::S_ENABLED, "socketstatus");
            }
        }
        return;
    }
    for (auto& item : *m_itemsMapPtr) {
        if (item.second->socketStatus() == Onyx::S_ACTIVE_CUT
            || item.second->socketStatus() == Onyx::S_ACTIVE_COAG) {
            qmlSetData(item.first, Onyx::S_ENABLED, "socketstatus");
        }
    }
}

bool SocketModel::commitModeChange(int socketId, int modeIndex, const QVariantMap &param)
{
    auto resolved = resolveSocket(socketId);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return false;
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end() || iter->second.isNull()) {
        return false;
    }

    QModelIndex idx = createIndex(socketId, 0);

    bool isModeCoag = param.value("iscoag").toBool();
//    qDebug() << "[commitModeChange]"
//             << "socketId=" << socketId
//             << "modeIndex=" << modeIndex
//             << "isModeCoag=" << isModeCoag
//             << "modeName=" << param.value("name").toString()
//             << "instrIndex=" << param.value("instrindex").toInt()
//             << "currentPower=" << param.value("currentpower").toInt();
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
            roles.append(CoagModeIsEndo);
            res = true;
        }

        int coagPower = param.value("currentpower").toInt();

        // Проверка для эндоскопических режимов: округление и валидация значений
        auto coagMode = iter->second->curCoagMode();

        if (!coagMode.isNull() && coagMode->isEndo()) {
            int endoCut = coagPower / 10;
            int endoCoag = coagPower % 10;
            if (endoCut < 1) {
                endoCut = 1;
            } else if (endoCut > ENDO_MAX) {
                endoCut = ENDO_MAX;
            }
            if (endoCoag < 1) {
                endoCoag = 1;
            } else if (endoCoag > ENDO_MAX) {
                endoCoag = ENDO_MAX;
            }
            coagPower = endoCut * 10 + endoCoag;
        }

        if (iter->second->setCoagModePower(coagPower)) {
            roles.append(CoagModePower);
            res = true;
        }
        if (iter->second->setInstrumIndex(param.value("instrindex").toInt(), true)) {
            roles.append(CoagModeInstrName);
            roles.append(CoagModeInstrIndex);
            roles.append(CoagModeInstrID);
            roles.append(CoagModeInstrNum);  // Добавляем Num для обновления изображения
            res = true;
        }
        if (res) {
            syncBi2HandlePedal(socketId);
            emit dataChanged(idx, idx, roles);
        }
        return res;
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
            roles.append(CutModeIsEndo);
            res = true;
        }

        int cutPower = param.value("currentpower").toInt();

        // Проверка для эндоскопических режимов: округление и валидация значений
        auto cutMode = iter->second->curCutMode();
        if (!cutMode.isNull() && cutMode->isEndo()) {
            int endoCut = cutPower / 10;
            int endoCoag = cutPower % 10;
            if (endoCut < 1) {
                endoCut = 1;
            } else if (endoCut > ENDO_MAX) {
                endoCut = ENDO_MAX;
            }
            if (endoCoag < 1) {
                endoCoag = 1;
            } else if (endoCoag > ENDO_MAX) {
                endoCoag = ENDO_MAX;
            }
            cutPower = endoCut * 10 + endoCoag;
        }

        if (iter->second->setCutModePower(cutPower)) {
            roles.append(CutModePower);
            res = true;
        }
        if (iter->second->setInstrumIndex(param.value("instrindex").toInt(), false)) {
            roles.append(CutModeInstrName);
            roles.append(CutModeInstrIndex);
            roles.append(CutModeInstrID);
            roles.append(CutModeInstrNum);  // Добавляем Num для обновления изображения
            res = true;
        }
        if (res) {
            emit dataChanged(idx, idx, roles);
        }
        return res;
    }
    return false;
}

SockPtr SocketModel::socketByName(const QString &socket) const
{
    SockPtr itemPtr = nullptr;

    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return itemPtr;
    }

    for (auto& item : (*m_itemsMapPtr)) {
        if (item.second->socketName() == socket) {
            itemPtr = item.second;
            break;
        }
    }
    return itemPtr;
}

SockPtr SocketModel::socketById(int id) const
{
    const auto resolved = resolveSocket(id);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return nullptr;
    }
    auto iter = resolved.itemsMap->find(resolved.socketId);
    if (iter == resolved.itemsMap->end()) {
        return nullptr;
    }
    return iter->second;
}

void SocketModel::expandSocket(int socketId)
{
    if ( !m_itemsMapPtr
         || m_itemsMapPtr->find(socketId) == m_itemsMapPtr->end() ) {
        return;
    }
    //нам тут достаточно отправить команду на разворот -
    // при сигнале о развороте сокета свернутся остальные
    qmlSetData(socketId, Onyx::S_EXPANDED, "socketdisplaymode");
}

void SocketModel::loadProgs(const std::vector<std::map<int, SockPtr> > &itemsMapVect,
                const std::vector<std::map<int, InstrPtr> > &instrMapVect, bool add)
{
//    qDebug() << "[ProgFlow] SocketModel::loadProgs pages:" << itemsMapVect.size()
//             << "add(добавить к существующим):" << add;
    if (itemsMapVect.size() != instrMapVect.size()) { //размеры не совпали, разбираться не хочу - бежим
//        qWarning() << "[ProgFlow] SocketModel::loadProgs: размеры itemsMapVect и instrMapVect не совпали";
        return;
    }

    beginResetModel();

    if (!add) {
        m_subProgIdx = 0;
        m_instrMapVect.clear();
        m_itemsMapVect.clear();
        m_instrMapPtr = nullptr;
        m_itemsMapPtr = nullptr;
    }

    size_t size = itemsMapVect.size();
    const int pageCountBeforeAdd = add ? static_cast<int>(m_itemsMapVect.size()) : 0;
    const int subProgIdxBeforeAdd = m_subProgIdx;

    for (size_t i = 0; i < size; ++i) {
        addList(itemsMapVect.at(i), instrMapVect.at(i));
        if (m_itemsMapVect.size() == 4) {
            break;
        }
    }

    if (add) {
        const int pageCountAfterAdd = static_cast<int>(m_itemsMapVect.size());
        if (pageCountAfterAdd > pageCountBeforeAdd) {
            m_subProgIdx = pageCountAfterAdd - 1;
        }
    }

    m_itemsMapPtr = &(m_itemsMapVect.at(m_subProgIdx));
    m_instrMapPtr = &(m_instrMapVect.at(m_subProgIdx));
    rebuildSocketNames();

    endResetModel();
    emit subProgCountChanged();
    if (add && m_subProgIdx != subProgIdxBeforeAdd) {
        emit subProgIdxChanged();
    }

    for (int row = 0; row < m_socketNames.size(); ++row) {
        syncBi2HandlePedal(row);
    }
//    qDebug() << "[ProgFlow] SocketModel::loadProgs готово subProgIdx:" << m_subProgIdx
//             << "subProgCount:" << m_itemsMapVect.size();
}

void SocketModel::removeSubProg(int index)
{
    if (m_itemsMapVect.size() <= 1
        || index < 0
        || static_cast<size_t>(index) >= m_itemsMapVect.size()) {
        return;
    }

    const int oldIdx = m_subProgIdx;
    // Удаляем последнюю — остаёмся на предыдущей.
    // Иначе следующий лист сдвигается на место удалённого.
    const int newIdx = (index == static_cast<int>(m_itemsMapVect.size()) - 1)
            ? index - 1
            : index;

    beginResetModel();

    m_itemsMapVect.erase(m_itemsMapVect.begin() + index);
    m_instrMapVect.erase(m_instrMapVect.begin() + index);

    m_subProgIdx = newIdx;
    m_itemsMapPtr = &m_itemsMapVect.at(m_subProgIdx);
    m_instrMapPtr = &m_instrMapVect.at(m_subProgIdx);
    rebuildSocketNames();

    endResetModel();

    emit subProgCountChanged();
    if (m_subProgIdx != oldIdx) {
        emit subProgIdxChanged();
    }

    for (int row = 0; row < m_socketNames.size(); ++row) {
        syncBi2HandlePedal(row);
    }
}


void SocketModel::addList(const std::map<int, SockPtr> &itemsMap,
                          const std::map<int, InstrPtr> &instrMap)
{
    m_itemsMapVect.push_back(itemsMap);
    m_instrMapVect.push_back(instrMap);
}

QHash<int, QByteArray> SocketModel::roleNames() const
{
    return m_roles;
}

int SocketModel::subProgIdx() const
{
    return m_subProgIdx;
}

int SocketModel::subProgCount() const
{
    return m_itemsMapVect.size();
}

void SocketModel::setSubProgIdx(int newIndex)
{
    if (newIndex < 0
        || static_cast<size_t>(newIndex) >= m_itemsMapVect.size()) {

            newIndex = m_itemsMapVect.size() - 1;
    }

    beginResetModel();
    m_subProgIdx = newIndex;
    m_itemsMapPtr = &(m_itemsMapVect.at(m_subProgIdx));
    m_instrMapPtr = &(m_instrMapVect.at(m_subProgIdx));
    rebuildSocketNames();
    endResetModel();

    emit subProgIdxChanged();
}

bool SocketModel::endoProgramView() const
{
    return m_endoProgramView;
}

void SocketModel::setEndoProgramView(bool enabled)
{
    if (m_endoProgramView == enabled) {
        return;
    }

    beginResetModel();
    m_endoProgramView = enabled;
    rebuildSocketNames();
    endResetModel();

    emit endoProgramViewChanged();
}

int SocketModel::roleIntByName(const QString &name)
{
    const QHash<int, QByteArray>& hash = m_roles;
    QByteArray nameArr = name.toLower().toUtf8();
    for (auto iter = hash.begin(); iter != hash.end(); ++iter) {
        if (iter.value() == nameArr) {
            return iter.key();
        }
    }
    return -1;
}

void SocketModel::socketCollapser(int expandedSocket)
{
    for (size_t i = 0; i < m_itemsMapPtr->size(); ++i) {
        if (i == static_cast<size_t>(expandedSocket)) {
            continue;
        }
        qmlSetData(i, 0, "socketdisplaymode");
    }
}

void SocketModel::pedalRemover(int socketToSkip, int pedalToRemove)
{
    for (size_t i = 0; i < m_itemsMapPtr->size(); ++i) {
        if (i == static_cast<size_t>(socketToSkip)) {
            continue;
        }
        //тут не надо изменять все сокеты - переназначенная педаль могла быть в одном сокетет только
        if (m_itemsMapPtr->at(i)->pedal() == pedalToRemove
            && pedalToRemove != Onyx::INSTR_BUTTON_MONO) {
            qmlSetData(i, 0, "socketpedal");
            // break;
        }
    }
}

int SocketModel::coagInstrumentButtonValue(int socketRow) const
{
    const auto resolved = resolveSocket(socketRow);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return 0;
    }

    const auto socketIter = resolved.itemsMap->find(resolved.socketId);
    if (socketIter == resolved.itemsMap->end() || socketIter->second.isNull()) {
        return 0;
    }

    if (socketIter->second->socketType() != Onyx::BIPOLAR_2) {
        return 0;
    }

    const CSurgModePtr coagMode = socketIter->second->curCoagMode();
    if (coagMode.isNull() || coagMode->id() == ESHF::NO_MODE) {
        return 0;
    }

    const int instrId = coagMode->selectedInstrId();
    if (instrId <= 0 || instrId == 1000) {
        return 0;
    }

    const auto instrIter = resolved.instrMap->find(instrId);
    if (instrIter == resolved.instrMap->end() || instrIter->second.isNull()) {
        return 0;
    }

    return instrIter->second->hadleType();
}

void SocketModel::syncBi2HandlePedal(int socketRow)
{
    const auto resolved = resolveSocket(socketRow);
    if (!resolved.itemsMap || !resolved.instrMap) {
        return;
    }

    const auto socketIter = resolved.itemsMap->find(resolved.socketId);
    if (socketIter == resolved.itemsMap->end() || socketIter->second.isNull()) {
        return;
    }

    SOCKET& socketItem = *(socketIter->second);
    if (socketItem.socketType() != Onyx::BIPOLAR_2) {
        return;
    }

    const bool hasHandleButton = coagInstrumentButtonValue(socketRow) == 1;
    socketItem.updateBi2AllowedPedals(hasHandleButton);

    QVector<int> roles = {SocketAllowedPedal};
    if (!hasHandleButton && socketItem.pedal() == Onyx::INSTR_BUTTON_BI) {
        socketItem.setPedal(Onyx::NO_PED);
        roles.append(SocketPedal);
    }

    const QModelIndex idx = createIndex(socketRow, 0);
    emit dataChanged(idx, idx, roles);
}

void SocketModel::populateRoles()
{
    //грёбаная мета-магия, но это приятное
    QMetaEnum metaEnum = QMetaEnum::fromType<SocketRoles>();
    m_roles.clear();
    for (int k = 0; k < metaEnum.keyCount(); k++)
    {
        int roleKey = metaEnum.value(k);
        QString roleName = metaEnum.valueToKey(roleKey);
        roleName = roleName.toLower();
        m_roles.insert(roleKey, roleName.toUtf8());
    }
}

std::vector<std::map<int, SockPtr> > SocketModel::getSocketsCopy()
{
    std::vector<std::map<int, SockPtr>> sockMap;
    //получаем именно копию мар и её данных без привязок к данным в модели
    for (const auto& socketPtrMap : m_itemsMapVect) {
        sockMap.emplace_back();
        auto& curMap = sockMap.back();
        for (const auto& sock : socketPtrMap) {
            curMap.insert({sock.first, SockPtr::create(*sock.second)});
        }
    }
    return sockMap;
}

std::vector<std::map<int, InstrPtr> > SocketModel::getInstrCopy()
{
    std::vector<std::map<int, InstrPtr>> instrMap;
    for (const auto& instrMapPtr : m_instrMapVect) {
        instrMap.emplace_back();
        auto& curInstr = instrMap.back();
        for (const auto& instr : instrMapPtr) {
            curInstr.insert({instr.first, InstrPtr::create(*instr.second)});
        }
    }
    return instrMap;
}
