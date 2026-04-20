#ifndef SURGICALMODE_H
#define SURGICALMODE_H

#include <QString>
#include <QVariantMap>
#include <QSharedPointer>
#include <QList>
#include <QObject>

#include "Structures.h"

#include <map>
#include <optional>

namespace ESHF {
enum eshfModes	{ /*NO_MODE = 0, */BI_BLEND=1,
                 BI_TUR=2, BI_ARTRO=3, BI_GISTERO=4,
                 BI_COAG=5, BI_COAG_DISS=6, TERMOSHOV=7,
                 CUT=8, BLEND=9, BLEND1=10, TUR=11, VAP=12,
                 E_KNIFE1=13, E_KNIFE2=14, E_KNIFE3=15,
                 E_LOOP1=16, E_LOOP2=17, E_LOOP3=18,
                 FORCE=19, FULGUR=20, SOFT=21, SPRAY=22,
                 FULGUR_A=23, SPRAY_A=24,
                 FULGUR_P=25, SPRAY_P=26, NO_MODE=1000
                 };
const QStringList modesNames = { /*QObject::tr("РЕЖИМ РЕЗ НЕ ВЫБРАН"), QObject::tr("РЕЖИМ КОАГ НЕ ВЫБРАН"),*/

                                 QObject::tr("БИ-СМЕСЬ"),
                                 QObject::tr("БИ-ТУР"), QObject::tr("БИ-АРТРО"),
                                 QObject::tr("БИ-ГИСТЕРО"),
                                 QObject::tr("БИ-КОАГ"), QObject::tr("БИ-КОАГ-ДИССЕКТ"),
                                 QObject::tr("ТЕРМОШОВ"),
                                 QObject::tr("РЕЗАНИЕ"), QObject::tr("СМЕСЬ"),
                                 QObject::tr("СМЕСЬ-1"), QObject::tr("ТУР"), QObject::tr("ВАП"),
                                 QObject::tr("ЭНДОНОЖ-1"), QObject::tr("ЭНДОНОЖ-2"),
                                 QObject::tr("ЭНДОНОЖ-3"),
                                 QObject::tr("ЭНДОПЕТЛЯ-"), QObject::tr("ЭНДОПЕТЛЯ-2"), QObject::tr("ЭНДОПЕТЛЯ-3"),
                                 QObject::tr("ФОРС"), QObject::tr("ФУЛЬГУР"),
                                 QObject::tr("МЯГКАЯ"), QObject::tr("СПРЕЙ"),
                                 QObject::tr("ФУЛЬГУР АРГОН"), QObject::tr("СПРЕЙ АРГОН"),
                                 QObject::tr("ФУЛЬГУР ПУЛЬС АРГОН"), QObject::tr("СПРЕЙ ПУЛЬС АРГОН"),
                                 QObject::tr("НЕ ВЫБРАН")
                                };

const QList<int> modesMaxPowers	{ /*1,*/ /*1,*/ 75,
                                8, 8, 8,
                                150, 150, 5,
                                400, 400, 150, 400, 400,
                                27, 27, 27,
                                27, 27, 27,
                                150, 150, 300, 70,
                                150, 70,
                                70, 70,
                                1 //всегда должно быть последним - мощность заглшуки NoMode
                                };
}

// struct InstrInfo {
//     int id;
//     int miniPower;
//     int midiPower;
//     int maxiPower;
//     // int legacyNumber;
//     InstrInfo() = default;
//     InstrInfo(int _id, int min, int mid, int max/*, int _legacyNumber*/)
//         : id(_id), miniPower(min), midiPower(mid), maxiPower(max)/*, legacyNumber(_legacyNumber)*/
//         {;}
// };

class SurgicalMode {
public:

    SurgicalMode(const QString& name,
                 bool isCoag,
                 int maximum = 1,
                 int minimum = 1,
                 int id = 0,
                 const std::map<int, Onyx::InstrInfo>& _instrs = {},
                 int num = 0,
                 const QString& brief = "",
                 const QString& descript = "",
                 bool isEndo = false);

    explicit SurgicalMode()  :
        SurgicalMode("NoMode", false, 1, 1, 0, {}, 0, "", "", false) {}

    //конструктор копирования тут дефолтный т.к. все поля тривиально коп

    int maximumPower() const;
    int currentPower() const;
    const QString &modeName() const;

    int minimumPower() const;

    bool setCurrentPower(int newCurrentpower);

    bool setParams(const QVariantMap& params);

    QVariantMap params() const;

    bool isCoag() const;

    void setInstrConstraints(const std::map<int, Onyx::InstrInfo> &newInstrConstraints);

    std::map<int, Onyx::InstrInfo> InstrConstraints() const;

    std::optional<Onyx::InstrInfo> getConstraints(int index) const;

    // QString curInstrName() const;

    int selectedInstrId() const;
    bool setSelectedInstrId(int newSelectedInstrId);

    int selectedInstrIndex() const;
    bool setSelectedInstrIndex(int newSelectedInstrIndex);

    // void setId(int newId);

    int id() const;
    int num() const;
    QString brief() const;
    QString descript() const;
    bool isEndo() const;

private:
    void setModeName(const QString &newModeName);
    void setMaximumPower(int newMaximumPower);
    void setMinimumPower(int newMaximumPower);

    int m_maximumPower;
    int m_minimumPower;
    int m_currentPower;
    int m_selectedInstrId = -1;
    int m_selectedInstrIndex = -1;
    // QString m_curInstrN?ame;
    QString m_modeName;
    bool m_isCoag;
    int m_id;
    int m_num;  // Num для формирования имени изображения
    QString m_brief;  // Краткое описание режима
    QString m_descript;  // Полное описание режима
    bool m_isEndo;  // Флаг эндоскопического режима
    std::map<int, Onyx::InstrInfo> m_InstrConstraints;
};

using SurgModePtr=QSharedPointer<SurgicalMode>;
using CSurgModePtr=QSharedPointer<const SurgicalMode>;
#endif // SURGICALMODE_H
