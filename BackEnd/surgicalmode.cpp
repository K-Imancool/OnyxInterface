#include "surgicalmode.h"


SurgicalMode::SurgicalMode(const QString& name,
                           bool isCoag,
                           int maximum,
                           int minimum,
                           int id,
                           const std::map<int, Onyx::InstrInfo>& _instrs,
                           int num,
                           const QString& brief,
                           const QString& descript,
                           bool isEndo,
                           bool isArgon) :
    m_maximumPower(maximum),
    m_minimumPower(minimum),
    m_currentPower(1),  // Мощность по умолчанию = 1
    m_modeName(name),
    m_isCoag(isCoag),
    m_id(id),
    m_num(num),
    m_brief(brief),
    m_descript(descript),
    m_isEndo(isEndo),
    m_isArgon(isArgon),
    m_InstrConstraints(_instrs)
{
    // Устанавливаем "НЕ ВЫБРАН" (индекс = размер списка, ID = 1000)
    setSelectedInstrIndex(m_InstrConstraints.size());
}

int SurgicalMode::maximumPower() const
{
    return m_maximumPower;
}

int SurgicalMode::currentPower() const
{
    return m_currentPower;
}

void SurgicalMode::setMaximumPower(int newMaximumPower)
{
    if (m_maximumPower == newMaximumPower)
        return;
    m_maximumPower = newMaximumPower;
}

int SurgicalMode::id() const
{
    return m_id;
}

int SurgicalMode::num() const
{
    return m_num;
}

QString SurgicalMode::brief() const
{
    return m_brief;
}

QString SurgicalMode::descript() const
{
    return m_descript;
}

bool SurgicalMode::isEndo() const
{
    return m_isEndo;
}

bool SurgicalMode::isArgon() const
{
    return m_isArgon;
}

int SurgicalMode::selectedInstrIndex() const
{
    return m_selectedInstrIndex;
}

bool SurgicalMode::setSelectedInstrIndex(int newSelectedInstrIndex)
{
    // Проверяем, что индекс не отрицательный
    if (newSelectedInstrIndex < 0)
        return false;
    
    // Если индекс равен размеру списка, это "НЕ ВЫБРАН" (ID = 1000)
    if (static_cast<size_t>(newSelectedInstrIndex) == m_InstrConstraints.size()) {
        m_selectedInstrIndex = newSelectedInstrIndex;
        m_selectedInstrId = 1000;  // Специальный ID для "не выбран"
        return true;
    }
    
    // Проверяем, что индекс в пределах списка
    if (static_cast<size_t>(newSelectedInstrIndex) > m_InstrConstraints.size())
        return false;
    
    m_selectedInstrIndex = newSelectedInstrIndex;
    //элементы map отсортированы по возрастанию ключа
    for (const auto& iter : m_InstrConstraints) {
        if (newSelectedInstrIndex == 0) {
            m_selectedInstrId = iter.second.id;
            return true;
        } else {
            newSelectedInstrIndex--;
        }
    }
    return false;
}

int SurgicalMode::selectedInstrId() const
{
    return m_selectedInstrId;
}

bool SurgicalMode::setSelectedInstrId(int newSelectedInstrId)
{
    auto iter = m_InstrConstraints.find(newSelectedInstrId);

    if (iter != m_InstrConstraints.end()) {
        m_selectedInstrId = newSelectedInstrId;
        int index = 0;
        const Onyx::InstrInfo& check = iter->second;
        for (const auto& [key, item] : m_InstrConstraints)
        {
            if (check.id == item.id) {
                m_selectedInstrIndex = index;
                return true;
            }
            index++;
        }
    }
    return false;
}

std::map<int, Onyx::InstrInfo> SurgicalMode::InstrConstraints() const
{
    return m_InstrConstraints;
}

std::optional<Onyx::InstrInfo> SurgicalMode::getConstraints(int index) const
{
    if (index >= m_InstrConstraints.size())
        return std::nullopt;
    //элементы map отсортированы по возрастанию ключа
    for (const auto& iter : m_InstrConstraints) {
        if (index == 0) {
            return iter.second;
        } else {
            index--;
        }
    }
    return std::nullopt;
}

void SurgicalMode::setInstrConstraints(const std::map<int, Onyx::InstrInfo> &newInstrConstraints)
{
    m_InstrConstraints = newInstrConstraints;
}

bool SurgicalMode::isCoag() const
{
    return m_isCoag;
}

bool SurgicalMode::setCurrentPower(int newCurrentpower)
{
    if (newCurrentpower <= m_maximumPower
        && newCurrentpower >= m_minimumPower
        && newCurrentpower != m_currentPower) {
        m_currentPower = newCurrentpower;
        return true;
    } else {
        return false;
    }
}

bool SurgicalMode::setParams(const QVariantMap &params)
{
    QString tmpName = params.value("name").toString();
    int tmpMin = params.value("minpower").toInt();
    int tmpMax = params.value("maxpower").toInt();
    bool tmpIsCoag = params.value("iscoag").toBool();
    int instrIndex = params.value("instrindex").toInt();

    //проверяяем что прислали изменения к нашему режиму и не пытаются поменять константы
    if ((tmpName != m_modeName)
        || (tmpIsCoag != m_isCoag)
        || (tmpMin != m_minimumPower)
        || (tmpMax != m_maximumPower))
        return false;
    int tmpCur = params.value("currentpower").toInt();
    setSelectedInstrIndex(instrIndex);
    //проверяем что мощность удовлетворяет ограничениям
    if ( (tmpCur >= m_minimumPower)
        && (tmpCur <= m_maximumPower) ) {
        m_currentPower = tmpCur;
        return true;
    }
    return false;
    // QVariantMap param
}

QVariantMap SurgicalMode::params() const
{
    QVariantMap res;
    res["name"] = m_modeName;
    res["currentpower"] = m_currentPower;
    res["minpower"] = m_minimumPower;
    res["maxpower"] = m_maximumPower;
    res["iscoag"] = m_isCoag;
    res["instrid"] = m_selectedInstrId;
    res["instrindex"] = m_selectedInstrIndex;
    res["modebrief"] = m_brief;
    res["modedescript"] = m_descript;
    res["isendo"] = m_isEndo;
    res["isargon"] = m_isArgon;
    res["id"] = m_id;
    res["num"] = m_num;
    return res;
}

int SurgicalMode::minimumPower() const
{
    return m_minimumPower;
}

const QString &SurgicalMode::modeName() const
{
    return m_modeName;
}

void SurgicalMode::setModeName(const QString &newModeName)
{
    if (m_modeName == newModeName)
        return;
    m_modeName = newModeName;
}

