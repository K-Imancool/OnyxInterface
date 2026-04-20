#include "halfsocket.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QDebug>
#include <QVector>

HalfSocket::HalfSocket(bool isCoag, int state)
    : m_isCoag(isCoag),
      m_state(state)
{}

int HalfSocket::modeIndex() const
{
    return m_modeIndex;
}

int HalfSocket::modeId() const
{
    if (m_curMode.isNull())
        return 1000;
    return m_curMode->id();
}

Onyx::HS_State HalfSocket::halfSocketState() const
{
    return static_cast<Onyx::HS_State>(m_state);
}

QString HalfSocket::modeName() const
{
    return m_curMode->modeName();
}

QMap<int, SurgModePtr> HalfSocket::modes() const
{
    return m_modes;
}

QStringList HalfSocket::modeNames() const
{
    return m_modeNames;
}

bool HalfSocket::setModeIndex(int newModeIndex)
{
    if (newModeIndex < 0 || newModeIndex >= m_modeNames.size())
        return false;
    m_modeIndex = newModeIndex;
    QString tmp = m_modeNames.at(newModeIndex);
    for (const auto& item : m_modes) {
        if (tmp == item->modeName())
            m_curMode = item;
    }
    // qDebug() << "mode Change to" << m_curMode->id() << m_curMode->modeName() << Qt::endl;
    return true;
}

bool HalfSocket::setModeId(int id)
{
    return setModeIndex(getModeIndex(id));
}

void HalfSocket::setHalfSocketState(Onyx::HS_State newSocketStatus)
{
    Q_UNUSED(newSocketStatus)
}

int HalfSocket::modePower() const
{
    if (m_curMode.isNull())
        return -1;
    return m_curMode->currentPower();
}

bool HalfSocket::setModePower(int newPower)
{
    if (m_curMode.isNull() || m_curMode->id() == 1000)
        return false;
    SurgModePtr ptr = m_modes[m_curMode->id()];
    return ptr->setCurrentPower(newPower);
}

void HalfSocket::setModes(const QMap<int, SurgModePtr> &newModes, const QStringList &order)
{
    Q_UNUSED(order)
    m_modes = newModes;
    
    // Создаём вектор пар (Num, SurgModePtr) для сортировки
    std::vector<std::pair<int, SurgModePtr>> modesVector;
    for (auto it = m_modes.begin(); it != m_modes.end(); ++it) {
        modesVector.push_back(std::make_pair(it.value()->num(), it.value()));
    }
    
    // Сортируем по Num
    std::sort(modesVector.begin(), modesVector.end(), 
        [](const std::pair<int, SurgModePtr>& a, const std::pair<int, SurgModePtr>& b) {
            return a.first < b.first;
        });
    
    // Заполняем m_modeNames в отсортированном порядке
    m_modeNames.clear();
    for (const auto& pair : modesVector) {
        m_modeNames.append(pair.second->modeName());
    }
    
    setModeIndex(0);
}

CSurgModePtr HalfSocket::curMode() const
{
    return m_curMode;
}

QByteArray HalfSocket::toByteArray()
{
    return QByteArray{};
}

QStringList HalfSocket::modeNamesIds() const
{
    QStringList tmp;
    for (auto item : m_modes.keys())
        tmp.append(QString("%1").arg(item));

    return tmp;
}

bool HalfSocket::setInstrumIndex(int index)
{
    return m_modes[m_curMode->id()]->setSelectedInstrIndex(index);
}

bool HalfSocket::setInstrumId(int id)
{
    return m_modes[m_curMode->id()]->setSelectedInstrId(id);
}

CSurgModePtr HalfSocket::getMode(int index) const
{
    if (index < 0 || index >= m_modeNames.size())
        return nullptr;

    QString tmp = m_modeNames.at(index);
    for (const auto& item : m_modes) {
        if (tmp == item->modeName())
            return item;
    }
    return nullptr;
}

int HalfSocket::getModeIndex(const QString &name) const
{
    return m_modeNames.indexOf(name);
}

int HalfSocket::getModeIndex(int id) const
{
    for (const auto& item : m_modes) {
        if (item->id() != id)
            continue;
        int idx = m_modeNames.indexOf(item->modeName());
        return (idx);
    }
    return -1;
}


int HalfSocket::modeCount()
{
    if (m_modes.size())
        return m_modes.size() - 1;
    return 0;
}

HalfSocket::HalfSocket(const HalfSocket &other)
{
    if (&other == this) {
        return;
    }
    m_modeIndex = other.m_modeIndex;
    m_modeNames = other.m_modeNames;
    m_isCoag = other.m_isCoag;
    m_state = other.m_state;
    for (auto modeIter = other.m_modes.constBegin(); modeIter != other.m_modes.constEnd(); ++modeIter) {
        //создаём копию режима и новый шаредптр уже на копию
        m_modes.insert(modeIter.key(), SurgModePtr::create(*modeIter.value()));
    }
}

HalfSocket &HalfSocket::operator=(const HalfSocket &other)
{
    if (&other == this) {
        return *this;
    }
    m_modeIndex = other.m_modeIndex;
    m_modeNames = other.m_modeNames;
    m_isCoag = other.m_isCoag;
    m_state = other.m_state;
    for (auto modeIter = other.m_modes.constBegin(); modeIter != other.m_modes.constEnd(); ++modeIter) {
        //создаём копию режима и новый шаредптр уже на копию
        m_modes.insert(modeIter.key(), SurgModePtr::create(*modeIter.value()));
    }
    return *this;
}
