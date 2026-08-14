#ifndef STRUCTURES_H
#define STRUCTURES_H

#include "qglobal.h"
#include <QVariant>
#include <QMetaType>

/*
 * Это неймспес со свсем вспомогательными енумами и структурами нужными более
 * чем 1 классу для общения между ними. Таким образом избегает жёсткой связи разных классов,
 * сохраняя возможность использовать именованные структыры для облегчения общения
 * */
namespace Onyx {


struct SocketState {
	quint16 cutModeNum = 0;
	quint16 cutModePower = 0;
	quint16 coagModeNum = 0;
	quint16 coagModePower = 0;
	quint16 cutModeId = 0;
	quint16 coagModeId = 0;
	quint8 pedal = 0;
	quint8 autoMode = 0;
};

struct InstrInfo {
	int id;
	int miniPower;
	int midiPower;
	int maxiPower;
	// int legacyNumber;
	InstrInfo() = default;
	InstrInfo(int _id, int min, int mid, int max/*, int _legacyNumber*/)
	    : id(_id), miniPower(min), midiPower(mid), maxiPower(max)/*, legacyNumber(_legacyNumber)*/
	{;}
};

/*! Перечисление возможных типов сокета */
enum SocType {  EMPTY, /*!<  Пустой сокет - заглушка на случай ошибок*/
	            BIPOLAR_1, /*!< Биполяр 1 */
	            BIPOLAR_2, /*!< Биполяр 2 */
	            MONOPOLAR_1, /*!< Монополяр 1 */
	            MONOPOLAR_2 /*!< Монополяр 2 */
                    };

/*! Перечисление возможных состояний сокета */
enum SocStatus  {S_OFF = 0, /*!< ОТКЛЮЧЕН */
	            S_DISABLED, /*!< Выключен, активация запрещена */
	            S_ENABLED, /*!< Включен, активация разрешена */
	            S_ACTIVE_COAG, /*!< Активирован, коагуляция 3*/
	            S_ACTIVE_CUT, /*!< Активирован, резание 4*/
	            S_ERROR /*!< Ошибка, активация запрещена */
                    };

/*! Перечисление возможных отображений сокета */
enum SocDisplayMode : int {   S_COLLAPSED = 0, /*!< свёрнут  */
	                    S_EXPANDED, /*!< развёрнут */
                    };

enum PedalType : int {  NO_PED = 0,
	                    SINGLE_PED,
	                    DOUBLE_PED,
	                    INSTR_BUTTON_BI,
	                    INSTR_BUTTON_MONO,
	                    PED_COUNT
                     };

enum HS_State {HS_DISABLED, /*!< Выключен, активация запрещена */
	            HS_ENABLED, /*!< Включен, активация разрешена */
                    };

// Нажатие кнопок держателей и педалей
enum PedalKnobPressed : quint8 {
	PRESS_MONO1_Y = 0x80,
	PRESS_MONO1_B = 0x40,
	PRESS_MONO1_YB = 0xC0,

	PRESS_MONO2_Y = 0x20,
	PRESS_MONO2_B = 0x10,
	PRESS_MONO2_YB = 0x30,

	PRESS_TERMO = 0x08,
	PRESS_PED1 = 0x04,

	PRESS_PED2_Y = 0x02,
	PRESS_PED2_B = 0x01,
	PRESS_PED2_YB = 0x03,

	PRESS_NONE = 0,
	PRESS_WRONG = 0xFF
};

enum InstrumentConnected : quint8 {
	INSTR_NOT_CONNECTED = 0,
	INSTR_DETECTED = 1,
	INSTR_READ = 2,
	INSTR_IDENTIFIED = 3
};

struct UnitState {
	bool argonCylinder1{false};             // Подключение баллонов
	bool argonCylinder2{false};
	quint8 argonRealRate{0};            // Реальный расход аргона во время активации
	bool neutraElConnected{false};          // Подключение нейтрального электрода НЭ
	bool tissueGrab{false};                 // Обнаружен захват ткани
	PedalKnobPressed pedalKnob{PRESS_NONE};      // Состояние кнопок и педалей
	quint8  pedalCharge{0};             // Заряд беспроводной педали
	InstrumentConnected instrBi2{INSTR_NOT_CONNECTED};    // Подключение инструментов (держателей) с определителем
	InstrumentConnected instrMono2{INSTR_NOT_CONNECTED};
	quint8 activOutput{0};              // Активированный выход
	quint8 activMode{0};                // Активированный режим

	bool operator==(const UnitState& other) const {
		return argonCylinder1 == other.argonCylinder1
		        && argonCylinder2 == other.argonCylinder2
		        && argonRealRate == other.argonRealRate
		        && neutraElConnected == other.neutraElConnected
		        && tissueGrab == other.tissueGrab
		        && pedalKnob == other.pedalKnob
		        && pedalCharge == other.pedalCharge
		        && instrBi2 == other.instrBi2
		        && instrMono2 == other.instrMono2
		        && activOutput == other.activOutput;
	}

	bool operator!=(const UnitState& other) const {
		return !(*this == other);
	}
};

}
Q_DECLARE_METATYPE(Onyx::SocketState);
Q_DECLARE_METATYPE(Onyx::UnitState);

#endif // STRUCTURES_H
