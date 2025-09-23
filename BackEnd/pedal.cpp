#include "pedal.h"

Pedal::Pedal(Pedal::PedalType pedId)
    : m_pedType(pedId)
{}

int Pedal::pedalType() const
{
    return static_cast<int>(m_pedType);
}

void Pedal::setPedType(int newPedId)
{
    if (newPedId > 0 && newPedId < PED_COUNT)
        m_pedType = static_cast<PedalType>(newPedId);
}
