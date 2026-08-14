#ifndef SURGICALMODE_H
#define SURGICALMODE_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QSharedPointer>

#include "Structures.h"

#include <map>
#include <optional>

class ESHF : public QObject
{
    Q_OBJECT
public:
    enum eshfModes {
    BI_BLEND = 1,
    BI_TUR = 2,
    BI_GISTERO = 3,
    BI_ARTRO = 4,
    BI_COAG = 5,
    BI_COAG_DISS = 6,
    BI_COAG_MICRO = 7,
    BI_COAG_FORCE = 8,
    TERMOSHOV = 9,
    TERMOSHOV_A = 10,
    BI_ABLATION = 11,

    CUT = 16,
    BLEND = 17,
    BLEND1 = 18,
    BLEND2 = 19,
    TUR = 20,
    VAP = 21,
    ENDO_I_0 = 22,
    ENDO_I_1 = 23,
    ENDO_I_2 = 24,
    ENDO_I_3 = 25,
    ENDO_I_FORCE_0 = 26,
    ENDO_I_FORCE_1 = 27,
    ENDO_I_FORCE_2 = 28,
    ENDO_I_FORCE_3 = 29,
    ENDO_P_0 = 30,
    ENDO_P_1 = 31,
    ENDO_P_2 = 32,
    ENDO_P_3 = 33,
    ENDO_P_FORCE_0 = 34,
    ENDO_P_FORCE_1 = 35,
    ENDO_P_FORCE_2 = 36,
    ENDO_P_FORCE_3 = 37,
    BLEND_ARGON = 38,
    BLEND1_ARGON = 39,
    BLEND2_ARGON = 40,

    SOFT = 46,
    SOFT_A = 47,
    FORCE = 48,
    FULGUR = 49,
    SPRAY = 50,
    SPRAY1 = 51,
    MONO_ABLATION = 52,
    FORCE_ARGON = 53,
    FULGUR_ARGON = 54,
    SPRAY_ARGON = 55,
    SPRAY1_ARGON = 56,
    FULGUR_PULSE_ARGON = 57,
    SPRAY_PULSE_ARGON = 58,

    NO_MODE = 1000
    };
    Q_ENUM(eshfModes)
};

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
                 bool isEndo = false,
                 bool isArgon = false);

    explicit SurgicalMode()  :
        SurgicalMode("NoMode", false, 1, 1, 0, {}, 0, "", "", false, false) {}

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
    bool isArgon() const;

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
    bool m_isArgon; // Режим с аргоном (Modes.Argon = 1)
    std::map<int, Onyx::InstrInfo> m_InstrConstraints;
};

using SurgModePtr=QSharedPointer<SurgicalMode>;
using CSurgModePtr=QSharedPointer<const SurgicalMode>;
#endif // SURGICALMODE_H
