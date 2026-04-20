#include "socket.h"
#include <QDebug>
#include <QtQml/qqml.h>


SOCKET::SOCKET(Onyx::SocType type) :
    m_socketType(type),
    m_socketStatus(Onyx::S_ENABLED),
    m_displayMode(Onyx::S_COLLAPSED)
{
    switch (m_socketType) {
    case Onyx::BIPOLAR_1:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED};
        break;
    case Onyx::BIPOLAR_2:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED, Onyx::INSTR_BUTTON_BI};
        break;
    case Onyx::MONOPOLAR_1:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED};
//        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED, Onyx::INSTR_BUTTON_MONO};
        break;
    case Onyx::MONOPOLAR_2:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED};
//        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED, Onyx::INSTR_BUTTON_MONO};
        break;
    default:
        break;
    }
    m_cutHalf = HalfSockPtr::create(false);
    m_coagHalf = HalfSockPtr::create(true);
}


int SOCKET::coagModeIndex() const
{
    if (m_coagHalf.isNull())
        return -1;
    return m_coagHalf->modeIndex();
}

bool SOCKET::setCoagModeIndex(int newCoagModeIndex)
{
    return setModeIndex(newCoagModeIndex, true);
}

int SOCKET::cutModeIndex() const
{
    if (m_cutHalf.isNull())
        return -1;
    return m_cutHalf->modeIndex();
}

int SOCKET::coagModeId() const
{
    if (m_coagHalf.isNull())
        return 1000;
    return m_coagHalf->modeId();
}

int SOCKET::cutModeId() const
{
    if (m_cutHalf.isNull())
        return 1000;
    return m_cutHalf->modeId();
}

int SOCKET::coagModeNum() const
{
    if (m_coagHalf.isNull())
        return 1000;
    return m_coagHalf->curMode()->num();
}

int SOCKET::cutModeNum() const
{
    if (m_cutHalf.isNull())
        return 1000;
    return m_cutHalf->curMode()->num();
}

bool SOCKET::setCutModeIndex(int newCutModeIndex)
{
    return setModeIndex(newCutModeIndex, false);
}

bool SOCKET::setModeId(int id, bool isCoag)
{
    HalfSockPtr half = isCoag ? m_coagHalf : m_cutHalf;
    if (half.isNull())
        return false;
    // qDebug() << "mode Change on" << m_socketName << Qt::endl;
    return half->setModeId(id);
}

Onyx::SocType SOCKET::socketType() const
{
    return m_socketType;
}

void SOCKET::setSocketType(Onyx::SocType newSocketType)
{
    if (m_socketType == newSocketType)
        return;
    m_socketType = newSocketType;
    m_cutHalf = HalfSockPtr::create(false);
    m_coagHalf = HalfSockPtr::create(true);
    switch (m_socketType) {
    case Onyx::BIPOLAR_1:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED};
        break;
    case Onyx::BIPOLAR_2:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED, Onyx::INSTR_BUTTON_BI};
        break;
    case Onyx::MONOPOLAR_1:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED};
//        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED, Onyx::INSTR_BUTTON_MONO};
        break;
    case Onyx::MONOPOLAR_2:
        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED};
//        m_allowedPedals = {Onyx::NO_PED, Onyx::SINGLE_PED, Onyx::DOUBLE_PED, Onyx::INSTR_BUTTON_MONO};
        break;
    default:
        break;
    }
}

Onyx::SocStatus SOCKET::socketStatus() const
{
    return m_socketStatus;
}

void SOCKET::setSocketStatus(Onyx::SocStatus newSocketStatus)
{
    if (m_socketStatus == newSocketStatus)
        return;
    m_socketStatus = newSocketStatus;
}

const QString &SOCKET::socketName() const
{
    return m_socketName;
}

QString SOCKET::coagModeName() const
{
    if (m_coagHalf.isNull())
        return "";
    return m_coagHalf->modeName();
}

QString SOCKET::cutModeName() const
{
    if (m_cutHalf.isNull())
        return "";
    return m_cutHalf->modeName();
}

QStringList SOCKET::cutModeNames() const
{
    if (m_cutHalf.isNull())
        return {};
    return m_cutHalf->modeNames();
}

QStringList SOCKET::coagModeNames() const
{
    if (m_coagHalf.isNull())
        return {};
    return m_coagHalf->modeNames();
}

QStringList SOCKET::coagModeNamesIds() const
{
    if (m_coagHalf.isNull())
        return {};
    return m_coagHalf->modeNamesIds();
}

QStringList SOCKET::cutModeNamesIds() const
{
    if (m_cutHalf.isNull())
        return {};
    return m_cutHalf->modeNamesIds();
}

void SOCKET::setSocketName(const QString &newSocketName)
{
    if (m_socketName == newSocketName)
        return;
    m_socketName = newSocketName;
}

CSurgModePtr SOCKET::getMode(int modeIndex, bool isCoag) const
{
    HalfSockPtr half = isCoag ? m_coagHalf : m_cutHalf;
    if (half.isNull())
        return nullptr;
    bool hasMode = half->modeNames().size() > modeIndex;

    if (!hasMode)
        return nullptr;
    return half->getMode(modeIndex);
    // return half->modes().value(name);
}

int SOCKET::coagModePower() const
{
    if (m_coagHalf.isNull())
        return 1;
    return m_coagHalf->modePower();
}

bool SOCKET::setCoagModePower(int newCoagModePower)
{
    return setModePower(newCoagModePower, true);
}

int SOCKET::cutModePower() const
{
    // return m_cutModePower;
    if (m_cutHalf.isNull())
        return 1;
    return m_cutHalf->modePower();
}

bool SOCKET::setCutModePower(int newCutModePower)
{
    return setModePower(newCutModePower, false);
}

CSurgModePtr SOCKET::curCutMode() const
{
    return m_cutHalf->curMode();
}

bool SOCKET::setInstrumIndex(int index, bool isCoag)
{
    HalfSockPtr half = isCoag ? m_coagHalf : m_cutHalf;
    if (half.isNull())
        return false;

    const QMap<int, SurgModePtr>& modes = half->modes();

    CSurgModePtr cMode = half->curMode();

    SurgModePtr mode = modes[cMode->id()];

    return mode->setSelectedInstrIndex(index);
}

bool SOCKET::setInstrumId(int id, bool isCoag)
{
    HalfSockPtr half = isCoag ? m_coagHalf : m_cutHalf;
    if (half.isNull())
        return false;

    const QMap<int, SurgModePtr>& modes = half->modes();

    CSurgModePtr cMode = half->curMode();

    if (cMode.isNull() || cMode->id() == 1000)
        return false;

    SurgModePtr mode = modes[cMode->id()];

    return mode->setSelectedInstrId(id);
}

void SOCKET::setAllowed(bool allow)
{
    // if (m_socketStatus >= S_DISABLED && allow)
    //     return;
    // if (m_socketStatus == S_OFF && (!allow))
    //     return;

    if (allow)
        m_socketStatus = Onyx::S_ENABLED;
    else
        m_socketStatus = Onyx::S_OFF;
}

CSurgModePtr SOCKET::curCoagMode() const
{
    return m_coagHalf->curMode();
}

bool SOCKET::setModePower(int newPower, bool isCoag)
{
    HalfSockPtr half = isCoag ? m_coagHalf : m_cutHalf;
    if (half.isNull())
        return false;
    return half->setModePower(newPower);
}

bool SOCKET::setModeIndex(int index, bool isCoag)
{
    HalfSockPtr half = isCoag ? m_coagHalf : m_cutHalf;
    if (half.isNull())
        return false;
    return half->setModeIndex(index);
}

int SOCKET::displayMode() const
{
    return m_displayMode;
}

bool SOCKET::setDisplayMode(Onyx::SocDisplayMode newDisplayMode)
{
    if (newDisplayMode < Onyx::S_COLLAPSED
        || newDisplayMode > Onyx::S_EXPANDED)
        return false;
    m_displayMode = newDisplayMode;
    return true;
}

void SOCKET::setCoagModes(const QMap<int, SurgModePtr > &newCoagModes,
                          const QStringList &order)
{
    if (m_coagHalf.isNull())
        return;
    m_coagHalf->setModes(newCoagModes, order);
}

void SOCKET::setCutModes(const QMap<int, SurgModePtr > &newCutModes,
                         const QStringList &order)
{
    if (m_cutHalf.isNull())
        return;
    m_cutHalf->setModes(newCutModes, order);
}

int SOCKET::pedal() const
{
    return m_pedal.pedalType();
}

bool SOCKET::setPedal(int type)
{           
    return m_pedal.setPedType(type);
}

QList<int> SOCKET::allowedPedals() const
{
    return m_allowedPedals;
}

Onyx::SocketState SOCKET::getInfo() const
{
    Onyx::SocketState res;
    res.coagModeNum = coagModeNum();
    res.cutModeNum = cutModeNum();
    res.cutModePower = cutModePower();
    res.coagModePower = coagModePower();
    res.pedal = pedal();
    return res;
}

int SOCKET::coagModeCount() const
{
    if (m_coagHalf.isNull())
        return 0;
    return m_coagHalf->modeCount();
}

int SOCKET::cutModeCount() const
{
    if (m_cutHalf.isNull())
        return 0;
    return m_cutHalf->modeCount();
}

SOCKET::SOCKET(const SOCKET &other)
{
    if (&other == this) {
        return;
    }

    m_socketType = other.m_socketType;
    m_socketStatus = other.m_socketStatus;
    m_displayMode = other.m_displayMode;
    m_pedal = other.m_pedal;
    m_socketName = other.m_socketName;

    //точно избегаем имплисит шаринг для КуЛиста
    m_allowedPedals.reserve(other.m_allowedPedals.size());
    for (int allowed : qAsConst(other.m_allowedPedals)) {
        m_allowedPedals.push_back(allowed);
    }
    //передаём копираванный объект в конструктор указателя
    m_cutHalf = HalfSockPtr::create(*(other.m_cutHalf.data()));
    m_coagHalf = HalfSockPtr::create(*(other.m_coagHalf.data()));
}

SOCKET &SOCKET::operator =(const SOCKET &other)
{
    if (this == &other) {
        return *this;
    }
    m_socketType = other.m_socketType;
    m_socketStatus = other.m_socketStatus;
    m_displayMode = other.m_displayMode;
    m_pedal = other.m_pedal;
    m_socketName = other.m_socketName;
    m_allowedPedals.reserve(other.m_allowedPedals.size());
    for (int allowed : qAsConst(other.m_allowedPedals)) {
        m_allowedPedals.push_back(allowed);
    }
    //передаём копираванный объект в конструктор указателя
    m_cutHalf = HalfSockPtr::create(*(other.m_cutHalf.data()));
    m_coagHalf = HalfSockPtr::create(*(other.m_coagHalf.data()));
    return *this;
}
