import QtQuick 2.15
import BackEnd 1.0

Rectangle {
    id: overlayRoot

    property bool dismissedBySecret: false
    property bool latchedCritical: false
    property var latchedCodes: []
    property int lastActivationModeId: -1
    property bool lastActivationIsCoag: false

    readonly property int socketStateActiveCoag: 3
    readonly property int socketStateActiveCut: 4
    readonly property bool lastActivationIsTermoCoag: lastActivationIsCoag
            && (lastActivationModeId === ESHF.TERMOSHOV
                || lastActivationModeId === ESHF.TERMOSHOV_A)

    readonly property bool fullscreenOn: typeof periphHandle !== "undefined"
                                         && periphHandle
                                         && periphHandle.fullscreenErrorsEnabled
    readonly property bool liveVisible: fullscreenOn && periphHandle.activationStopWarningVisible
    readonly property var liveCodes: liveVisible ? overlayRoot.filterWarningCodes(periphHandle.activationStopWarningCodes) : []
    readonly property var warningCodes: liveVisible ? liveCodes : latchedCodes
    readonly property bool hasCriticalError: {
        if (overlayRoot.latchedCritical)
            return true
        var codes = overlayRoot.warningCodes
        for (var i = 0; i < 16; ++i) {
            if (codes === undefined || codes === null)
                break
            var item = codes[i]
            if (item === undefined || item === null)
                break
            if (Number(item) >= 0x90)
                return true
        }
        return false
    }
    readonly property bool overlayActive: fullscreenOn
                                          && !dismissedBySecret
                                          && ((liveVisible && liveCodes.length > 0) || latchedCritical)

    visible: overlayActive
    enabled: overlayActive
    color: "#AAFFFFFF"

    function codesHaveCritical(codes) {
        for (var i = 0; i < codes.length; ++i) {
            if (Number(codes[i]) >= 0x90)
                return true
        }
        return false
    }

    function copyCodes(codes) {
        var copy = []
        for (var i = 0; i < codes.length; ++i)
            copy.push(codes[i])
        return copy
    }

    function captureActiveMode() {
        if (typeof theModel === "undefined" || !theModel)
            return
        var n = theModel.rowCount()
        for (var i = 0; i < n; ++i) {
            var idx = theModel.index(i, 0)
            if (!idx.valid)
                continue
            var status = Number(theModel.data(idx, SocketModel.SocketStatus))
            if (status === overlayRoot.socketStateActiveCoag) {
                overlayRoot.lastActivationModeId = Number(theModel.data(idx, SocketModel.CoagModeId))
                overlayRoot.lastActivationIsCoag = true
                return
            }
            if (status === overlayRoot.socketStateActiveCut) {
                overlayRoot.lastActivationModeId = Number(theModel.data(idx, SocketModel.CutModeId))
                overlayRoot.lastActivationIsCoag = false
                return
            }
        }
    }

    function codesContain(codes, code) {
        if (codes === undefined || codes === null)
            return false
        for (var i = 0; i < 16; ++i) {
            var item = codes[i]
            if (item === undefined || item === null)
                break
            if (Number(item) === code)
                return true
        }
        return false
    }

    function filterWarningCodes(codes) {
        var result = []
        if (codes === undefined || codes === null)
            return result
        for (var i = 0; i < 16; ++i) {
            var item = codes[i]
            if (item === undefined || item === null)
                break
            if (Number(item) === 0x41 && !overlayRoot.lastActivationIsTermoCoag)
                continue
            result.push(item)
        }
        return result
    }

    Connections {
        target: (typeof theModel !== "undefined") ? theModel : null
        function onDataChanged(topLeft, bottomRight, roles) {
            overlayRoot.captureActiveMode()
        }
    }

    Connections {
        target: (typeof periphHandle !== "undefined") ? periphHandle : null
        function onActivationChanged(active) {
            if (active)
                overlayRoot.captureActiveMode()
        }
        function onActivationStopWarningChanged() {
            if (!periphHandle)
                return
            if (periphHandle.activationStopWarningVisible
                    && overlayRoot.codesContain(periphHandle.activationStopWarningCodes, 0x41)
                    && !overlayRoot.lastActivationIsTermoCoag) {
                periphHandle.dismissWarningCode(0x41)
                return
            }
            if (!periphHandle.activationStopWarningVisible)
                return
            var codes = overlayRoot.filterWarningCodes(periphHandle.activationStopWarningCodes)
            if (overlayRoot.codesHaveCritical(codes)) {
                overlayRoot.latchedCritical = true
                overlayRoot.latchedCodes = overlayRoot.copyCodes(codes)
            }
            overlayRoot.dismissedBySecret = false
        }
    }

    function fullscreenErrorTextForCode(code) {
        switch (code) {
//        case 0x01: return qsTr("ОШИБКА Е1. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка связи: передача не выполнена")
//        case 0x02: return qsTr("ОШИБКА Е2. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка связи: нет ответа")
//        case 0x03: return qsTr("ОШИБКА Е3. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка связи: неверный ответ")
//        case 0x04: return qsTr("ОШИБКА Е4. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка связи: неверная длина пакета")
//        case 0x05: return qsTr("ОШИБКА Е5. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка связи: CRC не совпадает")

        case 0x41: return qsTr("КОАГУЛЯЦИЯ ЗАВЕРШЕНА")
//            return qsTr("ДЛИТЕЛЬНАЯ РАБОТА БЕЗ КАСАНИЯ ТКАНИ И/ИЛИ ОБРАЗОВАНИЯ ДУГИ.\nОТПУСКАЙТЕ ПЕДАЛЬ (КНОПКУ ДЕРЖАТЕЛЯ), КОГДА ВОЗДЕЙСТВИЕ НЕ ПРОИЗВОДИТСЯ.\nПРОВЕРЬТЕ ИСПРАВНОСТЬ ПЕДАЛИ И/ИЛИ ДЕРЖАТЕЛЯ С КНОПКАМИ.")
        case 0x42: return qsTr("ЗАМЫКАНИЕ ИНСТРУМЕНТА.\nПЕРЕУСТАНОВИТЕ ИНСТРУМЕНТ НА ТКАНЬ И ПОВТОРИТЕ АКТИВАЦИЮ")
        case 0x43: return qsTr("ПРОВЕРЬТЕ ПОДКЛЮЧЕНИЕ ДЕРЖАТЕЛЯ И НЕЙТРАЛЬНОГО ЭЛЕКТРОДА.\nПРОВЕРЬТЕ НАЛОЖЕНИЕ НЕЙТРАЛЬНОГО ЭЛЕКТРОДА НА ПАЦИЕНТА.")
        case 0x44: return qsTr("ПРОВЕРЬТЕ ДАВЛЕНИЕ В БАЛЛОНАХ. ПРОВЕРЬТЕ ПОДКЛЮЧЕНИЕ БАЛЛОНОВ.\nПОЛНОСТЬЮ ОТКРОЙТЕ ВЕНТИЛИ БАЛЛОНОВ.")
        case 0x45: return qsTr("НЕПРОХОДИМОСТЬ ГАЗОВОГО ТРАКТА ИНСТРУМЕНТА. ПРОВЕРЬТЕ ИНСТРУМЕНТ И ДЕРЖАТЕЛЬ.")
        case 0x46: return qsTr("ПРОВЕРЬТЕ ИСПОЛЬЗУЕМЫЙ РАСТВОР (0,9% NaCl)\nВ СЛУЧАЕ ПОВТОРЕНИЯ ОШИБКИ, ЗАМЕНИТЕ ИНСТРУМЕНТ И (ИЛИ) РЕЗЕКТОСКОП.\nПРОВЕРЬТЕ ПОДКЛЮЧЕНИЕ ДЕРЖАТЕЛЯ И РЕЗЕКТОСКОПА.")
        case 0x47: return qsTr("ПРОВЕРЬТЕ, ЧТО РАСТВОР НЕ ПОПАЛ В РЕЗЕКТОСКОП ИЛИ РАЗЪЁМ ПОДКЛЮЧЕНИЯ ДЕРЖАТЕЛЯ. ПРОСУШИТЕ РЕЗЕКТОСКОП И ДЕРЖАТЕЛЬ.\nВ СЛУЧАЕ ПОВТОРЕНИЯ ОШИБКИ, ЗАМЕНИТЕ РЕЗЕКТОСКОП.")
        case 0x48: return qsTr("ЗАХВАЧЕН СЛИШКОМ БОЛЬШОЙ ОБЪЁМ ТКАНИ И/ИЛИ ВЫБРАН РЕЖИМ НЕДОСТАТОЧНОЙ МОЩНОСТИ.")
//        case 0x4F: return qsTr("Активация остановлена: ошибка генератора")

//        case 0x80: return qsTr("ОШИБКА Е80. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: модуль связи не принимает сигналы от МИФ")
//        case 0x81: return qsTr("ОШИБКА Е81. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: генератор не отвечает")
//        case 0x82: return qsTr("ОШИБКА Е82. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: газовый модуль не отвечает")
//        case 0x83: return qsTr("ОШИБКА Е83. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: не отвечает радиомодуль")
//        case 0x84: return qsTr("ОШИБКА Е84. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: кнопки или педали зажаты до старта")
//        case 0x85: return qsTr("ОШИБКА Е85. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: МК НЭ не отвечает")
//        case 0x86: return qsTr("ОШИБКА Е86. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: МК раскачки не отвечает")
//        case 0x87: return qsTr("ОШИБКА Е87. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: питание НЭ 5В не соответствует норме")
//        case 0x88: return qsTr("ОШИБКА Е88. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: питание НЭ 3,3В не соответствует норме")
//        case 0x89: return qsTr("ОШИБКА Е89. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: перегрев контроллера НЭ")
//        case 0x8D: return qsTr("ОШИБКА Е8D. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: прошивка МК повреждена или обновление не выполнено")
//        case 0x8E: return qsTr("ОШИБКА Е8E. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Ошибка: нет рабочей прошивки МК")

//        case 0x90: return qsTr("ОШИБКА Е90. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: ИСН при включении")
//        case 0x91: return qsTr("ОШИБКА Е91. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: АЦП1 (напряжение контура)")
//        case 0x92: return qsTr("ОШИБКА Е92. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: АЦП2 (ток контура)")
//        case 0x93: return qsTr("ОШИБКА Е93. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: АЦП3 (ток генератора)")
//        case 0x94: return qsTr("ОШИБКА Е94. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: АЦП4 (напряжение ИСН)")
//        case 0x95: return qsTr("ОШИБКА Е95. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: реле")
//        case 0x96: return qsTr("ОШИБКА Е96. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: ИСН при нормальной работе")
//        case 0x97: return qsTr("ОШИБКА Е97. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: не найден резонанс при калибровке НЭ")
//        case 0x98: return qsTr("ОШИБКА Е98. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ")  //"Критичная ошибка: АЦП схемы НЭ")

        default:
            return qsTr("ОШИБКА Е%1. ОБРАТИТЕСЬ В СЕРВИСНУЮ СЛУЖБУ").arg(code.toString(16).toUpperCase())
        }
    }
    function isRedStripeCode(code) {
        var c = Number(code)
        if (c === 0x41)
            return !overlayRoot.lastActivationIsTermoCoag
        return c === 0x42 || c === 0x44 || c === 0x45
    }

    function fullscreenErrorColor(code) {
        if (Number(code) === 0x41 && overlayRoot.lastActivationIsTermoCoag)
            return "#70F870"
        return overlayRoot.isRedStripeCode(code) ? "#b8a400" : "#FF8A80"
    }

    readonly property color stripeColor: {
        var codes = overlayRoot.warningCodes
        var yellow = "#b8a400"
        var red = "#FF8A80"
        var firstColor = yellow
        var found = false
        for (var i = 0; i < 16; ++i) {
            if (codes === undefined || codes === null)
                break
            var item = codes[i]
            if (item === undefined || item === null)
                break
            var code = Number(item)
            if (isNaN(code))
                continue
            if (!found) {
                firstColor = overlayRoot.fullscreenErrorColor(code)
                found = true
            }
            if (overlayRoot.isRedStripeCode(code))
                return red
        }
        return found ? firstColor : yellow
    }

    MouseArea {
        anchors.fill: parent
        z: 10
        enabled: overlayRoot.visible
        hoverEnabled: true
        preventStealing: true
        onPressed: function(mouse) { mouse.accepted = true }
        onReleased: function(mouse) { mouse.accepted = true }
        onClicked: function(mouse) { mouse.accepted = true }
    }

    Rectangle {
        id: errorStripe
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        height: Math.max(250, errorColumn.implicitHeight + 40)
        color: overlayRoot.stripeColor
        z: 11

        Column {
            id: errorColumn
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 1180)
            spacing: 12

            Repeater {
                model: overlayRoot.warningCodes

                delegate: Text {
                    required property var modelData

                    width: errorColumn.width
                    text: overlayRoot.fullscreenErrorTextForCode(Number(modelData))
                    color: "black"
                    font.pixelSize: overlayRoot.hasCriticalError ? 34 : 40
                    font.bold: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Text {
                visible: overlayRoot.hasCriticalError
                width: errorColumn.width
                text: qsTr("Выключите питание аппарата и включите его повторно через 15 секунд.\nЕсли ошибка повторится, обратитесь в сервисную службу.")
                color: "black"
                font.pixelSize: 30
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    MouseArea {
        id: secretCloseButton
        width: 30
        height: 30
        z: 20
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
        }
        onClicked: {
            overlayRoot.dismissedBySecret = true
            overlayRoot.latchedCritical = false
        }
    }
}
