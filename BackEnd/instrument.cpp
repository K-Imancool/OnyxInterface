#include "instrument.h"


Instrument::Instrument(int id,
                       int legacyNumber,
                       const QString &name,
                       bool mono)
    : m_id(id),
      m_legacyNumber(legacyNumber),
      m_name(name),
      m_isMono(mono)
{

}

void Instrument::setDescription(const QString &newDescription)
{
    m_description = newDescription;
}

QString Instrument::name() const
{
    return m_name;
}

int Instrument::id() const
{
    return m_id;
}

int Instrument::hadleType() const
{
    return m_handleType;
}

void Instrument::setHandleType(int newHandleType)
{
    m_handleType = newHandleType;
}

bool Instrument::isMono() const
{
    return m_isMono;
}
