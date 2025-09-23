#ifndef PEDAL_H
#define PEDAL_H
#include <QString>

class Pedal
{
public:
    enum PedalType : int {  NO_PED,
                    DOUBLE_PED,
                    SINGLE_PED,
                    INSTR_BUTTON_BI,
                    INSTR_BUTTON_MONO,
                    PED_COUNT
                    };
    Pedal(PedalType pedType = NO_PED);

    int pedalType() const;
    void setPedType(int newPedType);

private:
    PedalType m_pedType;
    //может быть будем использовать сишные конструкции для image провайдера
    QString m_name;

};

#endif // PEDAL_H
