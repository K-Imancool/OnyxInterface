pragma Singleton
import QtQuick 2.15

// Палитра дисплея под корпус PANTONE 7546C.
// Все оттенки интерфейса меняются здесь.
QtObject {
    // Корпус
    readonly property color caseBase: "#253746"     // 7546C — шторка, тёмные боковые панели
    readonly property color caseSteel: "#768692"    // 7544C — стальной акцент, пустой режим
    readonly property color caseMist: "#D9E1E2"     // 7541C — светлый туманный

    // Светлая тема
    readonly property color background: "#F0F4F6"   // главный фон
    readonly property color surface: "#E1E8EC"      // карточки и неактивные блоки
    readonly property color text: "#1C2A36"         // основной текст и иконки
    readonly property color textSecondary: "#627380" // подписи
    readonly property color border: "#CDD7DF"       // границы и разделители

    // Интерактив и статусы
    readonly property color accent: "#00A8CC"       // кнопки действия и выделение
    readonly property color accentAlt: "#3B82F6"
    readonly property color accentDisabled: "#6600A8CC"
    readonly property color success: "#10B981"
    readonly property color danger: "#EF4444"
    readonly property color neSelected: "#FAA731"   // выбранная кнопка НЭ
    readonly property color neGreen: "#00DC4A"      // подключённый нейтральный электрод

    // Резание
    readonly property color cut: "#FFDD00"
    readonly property color cutSoft: "#FFE98A"
    readonly property color cutText: "#D9A000"      // жёлтый текст на светлом фоне

    // Коагуляция
    readonly property color coag: "#0F62FE"
    readonly property color coagSoft: "#74B0FF"     // текст коагуляции на тёмном фоне
    readonly property color coagDeep: "#003375"

    // Текст на плашках режимов. Имена без префикса on: в QML onИмя — это обработчик сигнала.
    readonly property color cutInk: "#1C2A36"
    readonly property color coagInk: "#F0F4F6"
    readonly property color darkInk: "#F0F4F6"
    readonly property color accentInk: "#F0F4F6"

    // Биполярный инструмент. В гамме отдельного цвета нет — акцентный циан,
    // чтобы не сливался ни с резанием, ни с коагуляцией.
    readonly property color bipolar: "#00A8CC"

    /*
    Прежние цвета. Чтобы вернуть, подставьте эти значения в свойства выше.

    readonly property color caseBase: "#383e42"       // панели педалей; периферия была #2c2c2c, сокеты #555d61, шторка darkslategray
    readonly property color caseSteel: "#999da0"      // пустой режим и корпус педали
    readonly property color caseMist: "#ffffff"       // подписи на тёмных панелях
    readonly property color background: "#F3F5F9"     // фон окна и редакторов; статус-бар был #ffffff
    readonly property color surface: "#E8ECF2"        // неактивные кнопки и фон клавиатуры
    readonly property color text: "#000000"           // иконка меню была #111111
    readonly property color textSecondary: "#5A6478"
    readonly property color border: "#C7CEDA"         // рамка статус-бара была #000000
    readonly property color accent: "#264093"         // fotekBlue; второй фирменный был #faa731, рамка педали orange
    readonly property color accentAlt: "#0f58fa"      // синий плашки активации; фокус полей был #4a9eff
    readonly property color accentDisabled: "#26409370"
    readonly property color success: "#70F870"        // кнопка «принять» была #2E7D32, баллон #30f020
    readonly property color danger: "#ff0000"
    readonly property color cut: "#F4D13D"            // сокеты были yellow, активация #ffd900
    readonly property color cutSoft: "#ffd900"
    readonly property color cutText: "#D4B82E"
    readonly property color coag: "#0B4FB3"           // сокеты были blue, активация #0f58fa
    readonly property color coagSoft: "#808080"
    readonly property color coagDeep: "#083D8C"
    readonly property color cutInk: "#000000"
    readonly property color coagInk: "#ffffff"
    readonly property color darkInk: "#ffffff"
    readonly property color accentInk: "#ffffff"
    readonly property color bipolar: "#800080"        // purple
    */
}
