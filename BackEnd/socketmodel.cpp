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

QVariant SocketModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= m_socketNames.size())
        return QVariant();

    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr)
        return QVariant();

    const auto socketIter = m_itemsMapPtr->find(index.row());
    if (socketIter == m_itemsMapPtr->end())
        return QVariant();
    if (socketIter->second.isNull())
        return QVariant();

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
        auto iter = m_instrMapPtr->find(socketItem.curCoagMode()->selectedInstrId());
        if (iter != m_instrMapPtr->end())
            return iter->second->name();
        else
            return tr("Не выбран");
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
    case CoagModeInstrNum:
    {
        if (socketItem.curCoagMode().isNull()
            || m_instrMapPtr == nullptr)
            return -1;
        
        int instrId = socketItem.curCoagMode()->selectedInstrId();
        // Если выбран пункт "НЕ ВЫБРАН" (ID = 1000), возвращаем 1000
        if (instrId == 1000)
            return 1000;
        
        auto iter = m_instrMapPtr->find(instrId);
        if (iter != m_instrMapPtr->end())
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
        if (!m_instrMapPtr || socketItem.curCutMode().isNull()) {
            return QString();
        }
        auto iter = m_instrMapPtr->find(socketItem.curCutMode()->selectedInstrId());
        if (iter != m_instrMapPtr->end()) {
            return iter->second->name();
        } else {
            return QString();
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
    case CutModeInstrNum:
    {
        if (socketItem.curCutMode().isNull()
                || m_instrMapPtr == nullptr) {
            return -1;
        }
        
        int instrId = socketItem.curCutMode()->selectedInstrId();
        // Если выбран пункт "НЕ ВЫБРАН" (ID = 1000), возвращаем 1000
        if (instrId == 1000)
            return 1000;
        
        auto iter = m_instrMapPtr->find(instrId);
        if (iter != m_instrMapPtr->end()) {
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
        return QVariant();
    }

    return QVariant();
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

    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return false;
    }

    if (m_itemsMapPtr->find((index.row())) == m_itemsMapPtr->end()) {
        return false;
    }
    auto socketIter = (*m_itemsMapPtr)[(index.row())];

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
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return QVariantMap{};
    }

    auto iter = m_itemsMapPtr->find(socketId);
    if (iter == m_itemsMapPtr->end() || iter->second.isNull()) {
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

void SocketModel::recalcCollapsed()
{
    qmlSetData(0, Onyx::S_EXPANDED, "socketdisplaymode");
    qmlSetData(0, Onyx::S_COLLAPSED, "socketdisplaymode");
}

QStringList SocketModel::modeNames(int socketID, bool isCoag) const
{
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return QStringList{};
    }

    auto iter = m_itemsMapPtr->find(socketID);
    if (iter == m_itemsMapPtr->end() || iter->second.isNull()) {
        return QStringList{};
    }
    return isCoag ? iter->second->coagModeNames() : iter->second->cutModeNames();
}

QStringList SocketModel::instrumNames(int socketId, int modeIndex, bool isCoag) const
{
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return QStringList{};
    }

    auto iter = m_itemsMapPtr->find(socketId);

    if (iter == m_itemsMapPtr->end() || iter->second.isNull()) {
        return {};
    }

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, Onyx::InstrInfo>& compatible = mode->InstrConstraints();

    QStringList names;
    for (const auto&[key, item] : compatible) {
        const auto instIter = m_instrMapPtr->find(item.id);
        if (instIter != m_instrMapPtr->end()) {
            names.append(instIter->second->name());
        }
    }
    // Добавляем пункт "не выбран" в конец списка
    names.append(QObject::tr("НЕ ВЫБРАН"));
    return names;
}

QStringList SocketModel::modeNamesIds(int socketID, bool isCoag) const
{
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return QStringList{};
    }

    auto iter = m_itemsMapPtr->find(socketID);
    if (iter == m_itemsMapPtr->end() || iter->second.isNull()) {
        return QStringList{};
    }
    return isCoag ? iter->second->coagModeNamesIds() : iter->second->cutModeNamesIds();
}

QStringList SocketModel::instrumNamesIds(int socketId, int modeIndex, bool isCoag) const
{
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return QStringList{};
    }

    auto iter = m_itemsMapPtr->find(socketId);
    if (iter == m_itemsMapPtr->end() || iter->second.isNull()) {
        return {};
    }

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, Onyx::InstrInfo>& compatible = mode->InstrConstraints();

    QStringList names;
    for (const auto&[key, item] : compatible) {
        const auto instIter = m_instrMapPtr->find(item.id);
        if (instIter != m_instrMapPtr->end()) {
            names.append(QString("%1").arg(instIter->second->id()));
        }
    }
    // Добавляем ID для пункта "НЕ ВЫБРАН"
    names.append("1000");
    return names;
}

QStringList SocketModel::instrumNamesNums(int socketId, int modeIndex, bool isCoag) const
{
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return QStringList{};
    }

    auto iter = m_itemsMapPtr->find(socketId);
    if (iter == m_itemsMapPtr->end() || iter->second.isNull()) {
        return {};
    }

    SockPtr sock = iter->second;
    CSurgModePtr mode = sock->getMode(modeIndex, isCoag);
    const std::map<int, Onyx::InstrInfo>& compatible = mode->InstrConstraints();

    QStringList nums;
    for (const auto&[key, item] : compatible) {
        const auto instIter = m_instrMapPtr->find(item.id);
        if (instIter != m_instrMapPtr->end()) {
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

    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return -1;
    }

    const auto socketIter = m_itemsMapPtr->find(socketId);
    if (socketIter == m_itemsMapPtr->end()) {
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

    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return -1;
    }

    const auto socketIter = m_itemsMapPtr->find(socketId);
    if (socketIter == m_itemsMapPtr->end()) {
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

void SocketModel::copyCurrentList()
{
    std::map<int, SockPtr> itemsMap = m_itemsMapVect.at(m_subProgIdx);
    std::map<int, InstrPtr> instrMap = m_instrMapVect.at(m_subProgIdx);
    loadProgs({itemsMap}, {instrMap}, true);
}


void SocketModel::slotRemoveSubProg()
{
    if (m_itemsMapVect.size() > 1) {
        removeSubProg(m_subProgIdx);
    }
}


void SocketModel::startActivation(int socketId, bool isCut)
{
    QTimer::singleShot(0, this, [this, socketId, isCut]() {
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
                    QTimer::singleShot(10, this, [this, socketId, newStatus]() {
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
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return ;
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
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr) {
        return false;
    }

    auto iter = m_itemsMapPtr->find(socketId);
    if (iter == m_itemsMapPtr->end() || iter->second.isNull()) {
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
    if (m_itemsMapPtr == nullptr || m_instrMapPtr == nullptr){
        return nullptr;
    }

    auto iter = m_itemsMapPtr->find(id);
    if (iter == m_itemsMapPtr->end()) {
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
    if (itemsMapVect.size() != instrMapVect.size()) { //размеры не совпали, разбираться не хочу - бежим
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

    for (size_t i = 0; i < size; ++i) {
        addList(itemsMapVect.at(i), instrMapVect.at(i));
        m_subProgIdx = m_itemsMapVect.size() - 1;
        if (m_itemsMapVect.size() == 4) {
            break;
        }
    }

    // Для полной загрузки программы показываем первую подпрограмму,
    // иначе пользователь может попадать на последнюю (часто пустую) страницу.
    if (!add && !m_itemsMapVect.empty()) {
        m_subProgIdx = 0;
    }

    m_itemsMapPtr = &(m_itemsMapVect.at(m_subProgIdx));
    m_instrMapPtr = &(m_instrMapVect.at(m_subProgIdx));
    m_socketNames.clear();
    for (int i = Onyx::BIPOLAR_1; i <= Onyx::MONOPOLAR_2; ++i) {
        const auto iter = m_itemsMapPtr->find(i - 1);
        if (iter == m_itemsMapPtr->cend()) {
            continue;
        }
        m_socketNames.append(iter->second->socketName());
    }

    endResetModel();
    emit subProgCountChanged();
    emit subProgIdxChanged();
}

void SocketModel::removeSubProg(int index)
{
    if (m_itemsMapVect.size() == 1
        || index < 0
        || static_cast<size_t>(index) >= m_itemsMapVect.size()) {
        return;
    }
    //сначала вычисляем новый индекс подстраницы

    int tmpIndex = 0;
    bool needIdxUpd = false;

    if (index == m_itemsMapVect.size() - 1) {// удаляем последнюю
        tmpIndex = index - 1;
    } else { //непоследнюю (для юзер индекс останется как был)
        tmpIndex = index + 1;
        needIdxUpd = true;
    }

    //переходим на какую-то страницу безопасную
    setSubProgIdx(tmpIndex);

    //безопасно удаляем утраницу
    auto itemIter = m_itemsMapVect.begin();
    itemIter += index;
    m_itemsMapVect.erase(itemIter);

    auto instrIter = m_instrMapVect.begin();
    instrIter += index;
    m_instrMapVect.erase(instrIter);

    //обновляем число страниц
    emit subProgCountChanged();
    if (needIdxUpd) {
        m_subProgIdx--;
        emit subProgIdxChanged();
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
    m_socketNames.clear();

    for (int i = Onyx::BIPOLAR_1; i <= Onyx::MONOPOLAR_2; ++i) {
        const auto iter = m_itemsMapPtr->find(i - 1);
        if (iter == m_itemsMapPtr->cend()) {
            continue;
        }
        m_socketNames.append(iter->second->socketName());
    }
    endResetModel();

    emit subProgIdxChanged();
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
