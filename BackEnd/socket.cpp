#include "socket.h"
#include <QDebug>
#include <QtQml/qqml.h>


SOCKET::SOCKET(SOCKET::SocType type) :
    m_socketType(type),
    m_socketStatus(S_ENABLED),
    m_displayMode(S_COLLAPSED)
{
    m_cutHalf = HalfSockPtr::create(false);
    m_coagHalf = HalfSockPtr::create(true);
}


int SOCKET::coagModeIndex() const
{
    return m_coagHalf->modeIndex();
}

bool SOCKET::setCoagModeIndex(int newCoagModeIndex)
{
    return setModeIndex(newCoagModeIndex, true);
}

int SOCKET::cutModeIndex() const
{
    return m_cutHalf->modeIndex();
}

int SOCKET::coagModeId() const
{
    return m_coagHalf->modeId();
}

int SOCKET::cutModeId() const
{
    return m_cutHalf->modeId();
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

SOCKET::SocType SOCKET::socketType() const
{
    return m_socketType;
}

void SOCKET::setSocketType(SOCKET::SocType newSocketType)
{
    if (m_socketType == newSocketType)
        return;
    m_socketType = newSocketType;
    m_cutHalf = HalfSockPtr::create(false);
    m_coagHalf = HalfSockPtr::create(true);
}

SOCKET::SocStatus SOCKET::socketStatus() const
{
    return m_socketStatus;
}

void SOCKET::setSocketStatus(SocStatus newSocketStatus)
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

QByteArray SOCKET::toByteArray()
{
    /// Номер сокета (какой-нибудь индивидуальный код) 1байт (ну 256 сокетов это навсегда)
    /// Номер режима резания (индивидуальный код ) 2 байта (возможно второй байт для доп признаков)
    /// мощность режима резания (2 байта)
    /// Номер режима коагуляции (индивидуальный код )
    /// мощность режима коагуляции
    /// Итого 9 байт - 4 16бит значения и 1 8бит
    ///
    QByteArray res;
    // QDataStream stream();
    // stream << m_socketType << m_cutHalf->toByteArray() << m_coagHalf->toByteArray();

    return res;
}

bool SOCKET::setInstrumIndex(int index, bool isCoag)
{
    HalfSockPtr half = isCoag ? m_coagHalf : m_cutHalf;
    if (half.isNull())
        return false;

    const QMap<int, SurgModePtr>& modes = half->modes();

    CSurgModePtr cMode = half->curMode();

    // if (curModeIdx >= modes.size())
    //     return false;

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
    if (m_socketStatus >= S_DISABLED && allow)
        return;
    if (m_socketStatus == S_OFF && (!allow))
        return;

    if (allow)
        m_socketStatus = S_ENABLED;
    else
        m_socketStatus = S_OFF;
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

void SOCKET::setDisplayMode(SocDisplayMode newDisplayMode)
{
    m_displayMode = newDisplayMode;
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

void SOCKET::setPedal(int type)
{
    m_pedal.setPedType(type);
}
