#include "socketmodeeditor.h"
// #include "instrument.h"

#include <optional>
#include <QDebug>

SocketModeEditor::SocketModeEditor(QSharedPointer<SocketModel> model, QObject *parent)
    : QObject{parent},
    m_model{model}
{}

void SocketModeEditor::initialize(int socket, int mode, bool isCoag)
{
    if (socket == -1|| mode == -1)
        return;
    m_isCoag = isCoag;
    m_socketID = socket;
    m_modeNames = m_model->modeNames(socket, isCoag);
    m_modeNameIds = m_model->modeNamesIds(socket, isCoag);
    m_modeNameNums = m_model->modeNamesNums(socket, isCoag);
    preloadModeCache();

    const int modelModeIndex = m_model->index(socket, 0).data(
                                   m_isCoag ? SocketModel::CoagModeIndex
                                            : SocketModel::CutModeIndex).toInt();
    if (modelModeIndex >= 0 && modelModeIndex < m_modeNames.size()) {
        mode = modelModeIndex;
    } else if (mode >= m_modeNames.size()) {
        mode = 0;
    }

    m_originalParameters =  m_model->modeParam(socket, mode, isCoag);
    m_instrList =           m_model->instrumNames(socket, mode, isCoag);
    m_instrListIds =        m_model->instrumNamesIds(socket, mode, isCoag);
    m_instrListNums =       m_model->instrumNamesNums(socket, mode, isCoag);

    m_socketName = m_model->index(socket,0).data(SocketModel::SocketName).toString();
    m_originalModeIndex = mode;

    m_currentModeIndex = m_originalModeIndex;
    m_currentParameters = m_originalParameters;
    m_preferredInstrId = m_model->selectedInstrumIdByMode(m_socketID, m_currentModeIndex, m_isCoag);
    if (m_preferredInstrId <= 0 || m_preferredInstrId == 1000) {
        m_preferredInstrId = -1;
    }

    setCurrentInstrIndex(m_model->index(socket,0).data(m_isCoag ? SocketModel::CoagModeInstrIndex
                                                               : SocketModel::CutModeInstrIndex).toInt());

    m_hasChanges = false;
    m_autoModeDirty = false;
    emit currentModeIndexChanged();
    emit parametersLoaded();
    emit currentParamsChanged();
}

void SocketModeEditor::loadModeParameters(int modeIndex)
{
    if (modeIndex >= m_modeNames.size())
        return;
    if (hasCachedModeData(modeIndex)) {
        const ModeCacheEntry& cached = m_modeCache.at(modeIndex);
        m_currentParameters = cached.parameters;
        m_instrList = cached.instrList;
        m_instrListIds = cached.instrListIds;
        m_instrListNums = cached.instrListNums;
    } else {
        m_currentParameters = m_model->modeParam(m_socketID, modeIndex, m_isCoag);
        m_instrList = m_model->instrumNames(m_socketID, modeIndex, m_isCoag);
        m_instrListIds = m_model->instrumNamesIds(m_socketID, modeIndex, m_isCoag);
        m_instrListNums = m_model->instrumNamesNums(m_socketID, modeIndex, m_isCoag);
    }

    emit parametersLoaded();

    const int selectedInstrIndex = hasCachedModeData(modeIndex)
            ? m_modeCache.at(modeIndex).selectedInstrIndex
            : m_model->selectedInstrumIndexByMode(m_socketID, modeIndex, m_isCoag);
    setCurrentInstrIndex(selectedInstrIndex);
}

void SocketModeEditor::preloadModeCache()
{
    m_modeCache.clear();
    m_modeCache.resize(m_modeNames.size());

    for (int i = 0; i < m_modeNames.size(); ++i) {
        ModeCacheEntry& entry = m_modeCache[i];
        entry.parameters = m_model->modeParam(m_socketID, i, m_isCoag);
        entry.instrList = m_model->instrumNames(m_socketID, i, m_isCoag);
        entry.instrListIds = m_model->instrumNamesIds(m_socketID, i, m_isCoag);
        entry.instrListNums = m_model->instrumNamesNums(m_socketID, i, m_isCoag);
        entry.selectedInstrIndex = m_model->selectedInstrumIndexByMode(m_socketID, i, m_isCoag);
    }
}

bool SocketModeEditor::hasCachedModeData(int modeIndex) const
{
    return modeIndex >= 0
            && modeIndex < m_modeCache.size()
            && !m_modeCache.at(modeIndex).parameters.isEmpty();
}

void SocketModeEditor::updateCurrentParameters(const QVariantMap ms)
{
    if (isParamsEqual(m_currentParameters, ms))
        return;
    m_currentParameters = ms;
            checkChanges();
    emit currentParamsChanged();
}

void SocketModeEditor::updateParameter(const QString &paramName, const QVariant &value)
{
    // if (paramName)
    //Пока что просто присваиваем, но по-хорошему нужна проверка что добавляем параметры с нормальными
    //ключами и проверять типа данных на соответствие хранимым, а то из QMLнапихать сможем многовато лишнего
    
    m_currentParameters[paramName] = value;
            checkChanges();
    emit currentParamsChanged();
}

void SocketModeEditor::commitChanges()
{
    const bool modelOk = m_model->commitModeChange(m_socketID,
                                                   m_currentModeIndex,
                                                   m_currentParameters);
    const bool success = modelOk || m_autoModeDirty;
    if (success) {
        m_hasChanges = false;
        m_autoModeDirty = false;
        emit hasChangesChanged();
    }
    emit editingFinished(success);
}

void SocketModeEditor::rollBack()
{
    m_hasChanges = false;
    m_autoModeDirty = false;
    emit hasChangesChanged();
    emit editingFinished(false);
}

QStringList SocketModeEditor::modeNames() const
{
    return m_modeNames;
}

QVariantMap SocketModeEditor::currentMode() const
{
    return m_currentParameters;
}

QString SocketModeEditor::socketName() const
{
    return m_socketName;
}

int SocketModeEditor::currentModeIndex() const
{
    return m_currentModeIndex;
}

void SocketModeEditor::setCurrentModeIndex(int index)
{
    if (index >= 0 && index < m_modeNames.size()) {
        int prevInstrId = -1;
        if (m_currentInstrIndex >= 0 && m_currentInstrIndex < m_instrListIds.size()) {
            bool ok = false;
            const int parsedId = m_instrListIds.at(m_currentInstrIndex).toInt(&ok);
            if (ok) {
                prevInstrId = parsedId;
            }
        }
        if (prevInstrId <= 0 || prevInstrId == 1000) {
            prevInstrId = m_preferredInstrId;
        }
        m_currentModeIndex = index;
        loadModeParameters(index);
        if (prevInstrId > 0 && prevInstrId != 1000) {
            const int sameInstrIndex = m_instrListIds.indexOf(QString::number(prevInstrId));
            if (sameInstrIndex >= 0) {
                setCurrentInstrIndex(sameInstrIndex);
            }
        }
        checkChanges();
        emit currentModeIndexChanged();
        emit currentParamsChanged();
    }
}

bool SocketModeEditor::checkChanges()
{
    bool tmp = ( m_currentModeIndex != m_originalModeIndex
            || m_currentParameters.value("currentpower").toInt()
                != m_originalParameters.value("currentpower").toInt()
            || m_currentParameters.value("instrindex").toInt()
                != m_originalParameters.value("instrindex").toInt());

    if (tmp && m_hasChanges != tmp) {
        m_hasChanges = tmp;
        emit hasChangesChanged();
    }
    return tmp;
}

QStringList SocketModeEditor::instrList() const
{
    return m_instrList;
}

QStringList SocketModeEditor::modeNamesIds() const
{
    return m_modeNameIds;
}

QStringList SocketModeEditor::modeNamesNums() const
{
    return m_modeNameNums;
}


QStringList SocketModeEditor::instrListIds() const
{
    return m_instrListIds;
}

QStringList SocketModeEditor::instrListNums() const
{
    return m_instrListNums;
}

QVariantMap SocketModeEditor::fetchModeParameters(int modeIndex)
{
    if (modeIndex >= m_modeNames.size())
        return QVariantMap();

    return m_model->modeParam(m_socketID, /*m_modeNames.at*/(modeIndex), m_isCoag);
}

bool SocketModeEditor::isParamsEqual(const QVariantMap &a, const QVariantMap &b) const
{
    if (a.size() != b.size())
        return false;

    QStringList tempA = a.keys();
    QStringList tempB = a.keys();

    if (tempA.size() != tempB.size())
        return false;

    std::sort(tempA.begin(), tempA.end());
    std::sort(tempB.begin(), tempB.end());

    if (tempA != tempB)
        return false;
    bool isEqual = true;
    for (int idx = 0; idx < tempA.size(); ++idx) {
        const QVariant& aValueRef = a.value(tempA.at(idx));
        const QVariant& bValueRef = b.value(tempB.at(idx));
        if (aValueRef.typeName() !=
            bValueRef.typeName() ) {
            isEqual = false;
            break;
        }
        // тут нужна конверсия названия полей интересующих нас в типы для явного сравнения
        // или type_traits для сравнения только одинаковых типов имеющих оператор ==
        // if (aValueRef.() != bValueRef.value())
    }
    return isEqual;

}

bool SocketModeEditor::isCoag() const
{
    //в пределах одного едитора параметр isCoag не меняется вовсе
    return m_isCoag;
}

bool SocketModeEditor::hasChanges() const
{
    return m_hasChanges || m_autoModeDirty;
}

void SocketModeEditor::setAutoModeDirty(bool dirty)
{
    if (m_autoModeDirty == dirty) {
        return;
    }
    m_autoModeDirty = dirty;
    emit hasChangesChanged();
}

int SocketModeEditor::currentInstrIndex() const
{
    return m_currentInstrIndex;
}

void SocketModeEditor::setCurrentInstrIndex(int newCurrentInstrIndex)
{
    m_currentInstrIndex = newCurrentInstrIndex;
    if (m_currentInstrIndex >= 0 && m_currentInstrIndex < m_instrListIds.size()) {
        bool ok = false;
        const int selectedInstrId = m_instrListIds.at(m_currentInstrIndex).toInt(&ok);
        if (ok && selectedInstrId > 0 && selectedInstrId != 1000) {
            m_preferredInstrId = selectedInstrId;
        }
    }
    updateParameter("instrindex", m_currentInstrIndex);
    CSurgModePtr mode = m_model->socketById(m_socketID)->getMode(m_currentModeIndex, m_isCoag);
    if (mode.isNull())
        return;
    std::optional<Onyx::InstrInfo> info = mode->getConstraints(m_currentInstrIndex);
    if (info != std::nullopt) {
        m_instrID = newCurrentInstrIndex+1;
        m_lowPowerBound = info->miniPower;
        m_midPowerBound = info->midiPower;
        m_highPowerBound = info->maxiPower;
    } else {
        m_instrID = m_lowPowerBound = m_midPowerBound = m_highPowerBound = 0;
    }

    emit currentInstrChanged();
}

int SocketModeEditor::lowPowerBound() const
{
    return m_lowPowerBound;
}

int SocketModeEditor::midPowerBound() const
{
    return m_midPowerBound;
}

int SocketModeEditor::highPowerBound() const
{
    return m_highPowerBound;
}

int SocketModeEditor::instrID() const
{
    return m_instrID;
}

int SocketModeEditor::currentPower() const
{
    return m_currentParameters.value("currentpower").toInt();
}

QString SocketModeEditor::modeDescript() const
{
    return m_currentParameters.value("modedescript").toString();
}

QString SocketModeEditor::modeBrief() const
{
    return m_currentParameters.value("modebrief").toString();
}

QString SocketModeEditor::instrBrief() const
{
    // Получаем ID инструмента из текущего режима
    SockPtr sckPtr = m_model->socketById(m_socketID);
    if (!sckPtr)
        return "";
    CSurgModePtr mode = sckPtr->getMode(m_currentModeIndex, m_isCoag);
    if (mode.isNull())
        return QString();
    
    std::optional<Onyx::InstrInfo> info = mode->getConstraints(m_currentInstrIndex);
    if (info == std::nullopt)
        return QString();
    // mode->
    // Получаем инструмент по ID
    InstrPtr instr = m_model->getInstrumentById(info->id);
    if (instr.isNull())
        return QString();
    
    return instr->description();
}

bool SocketModeEditor::isEndo() const
{
    //в пределах одного едитора параметр эндо меняется только при загрузке всех остальных
    //элементов информации про режим
    return m_currentParameters.value("isendo").toBool();
}
