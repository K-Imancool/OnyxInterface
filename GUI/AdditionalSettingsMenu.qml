import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import CuteKeyboard 1.0

Item {
    id: additionalSettingsRoot

    signal returnButtonPressed()
    signal serviceMenuButtonPressed(string accessLevel)
    signal logFileButtonPressed()
    signal dateTimeSettingsButtonPressed()

    property color fotekBlue: "#264093"
    property color fotekOrange: "#faa731"
    readonly property string iconsBasePath: AppPaths.iconsBaseUrl
    readonly property int screenMargin: 34
    readonly property int mainSpacing: 20
    readonly property int headerHeight: 70
    readonly property int menuActionLabelSize: 34
    readonly property int menuActionIconSize: 76
    readonly property int panelRadius: 20
    readonly property int menuActionSmallLabelSize: 28

    function isServiceMenuNoPasswordEnabled() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return false
        }
        return savedJson.readString("serviceMenuNoPassword", "0") === "1"
    }

    function normalizeServicePassword(value) {
        var normalized = String(value).trim().toLowerCase()
        normalized = normalized.replace(/\u0430/g, "a")
        return normalized
    }

    function resolveServiceMenuAccessLevel(value) {
        var normalized = normalizeServicePassword(value)
        if (normalized === "145a") {
            return "full"
        }
        if (normalized === "123") {
            return "limited"
        }
        return ""
    }

    function isServiceMenuPasswordValid(value) {
        return resolveServiceMenuAccessLevel(value) !== ""
    }

    function serialNumberValue() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return -1
        }
        var serialText = String(savedJson.readString("serialNumber", "")).trim()
        if (!/^\d+$/.test(serialText)) {
            return -1
        }
        return parseInt(serialText, 10)
    }

    function deviceTypeValue() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return "ONYX-AM"
        }
        return String(savedJson.readString("deviceType", "ONYX-AM")).trim().toUpperCase()
    }

    function unlockKeyDigits(value) {
        return String(value === undefined || value === null ? "" : value).replace(/\D/g, "").substring(0, 12)
    }

    function formattedUnlockKey(value) {
        var digits = unlockKeyDigits(value)
        var groups = []
        for (var i = 0; i < digits.length; i += 3) {
            groups.push(digits.substring(i, i + 3))
        }
        return groups.join("-")
    }

    function requestServiceMenuAccess() {
        if (additionalSettingsRoot.isServiceMenuNoPasswordEnabled()) {
            additionalSettingsRoot.serviceMenuButtonPressed("full")
        } else {
            servicePasswordDialog.passwordError = ""
            servicePasswordDialog.open()
        }
    }

    function requestFeatureUnlock() {
        featureUnlockDialog.passwordError = ""
        featureUnlockDialog.open()
    }

    Rectangle {
        anchors.fill: parent
        color: "#8A929E"
    }

    Item {
        id: headerArea
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: additionalSettingsRoot.screenMargin - 8
        }
        width: parent.width - additionalSettingsRoot.screenMargin * 2
        height: additionalSettingsRoot.headerHeight

        Text {
            text: qsTr("ДОПОЛНИТЕЛЬНЫЕ НАСТРОЙКИ")
            anchors.centerIn: parent
            color: additionalSettingsRoot.fotekBlue
            font.pixelSize: 36
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width
            lineHeight: 1.05
        }
    }

    Item {
        id: footerArea
        height: 72
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: additionalSettingsRoot.screenMargin
            rightMargin: additionalSettingsRoot.screenMargin
            bottomMargin: additionalSettingsRoot.screenMargin
        }

        DialogActionButton {
            width: 180
            height: parent.height
            text: qsTr("НАЗАД")
            secondaryColor: additionalSettingsRoot.fotekBlue
            secondaryBorderWidth: 1
            secondaryBorderColor: "#1E3274"
            cornerRadius: additionalSettingsRoot.panelRadius
            labelPixelSize: 30
            onPressed: additionalSettingsRoot.returnButtonPressed()
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
        }

        Text {
            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            text: (typeof dateTimeController !== "undefined" && dateTimeController)
                  ? dateTimeController.currentDateTime
                  : ""
            color: additionalSettingsRoot.fotekBlue
            opacity: 0.72
            font.pixelSize: 24
            font.bold: false
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: typeof dateTimeController !== "undefined" && dateTimeController
        onTriggered: dateTimeController.refresh()
    }

    ColumnLayout {
        anchors {
            top: headerArea.bottom
            topMargin: 22
            left: parent.left
            right: parent.right
            bottom: footerArea.top
            bottomMargin: additionalSettingsRoot.mainSpacing
            leftMargin: additionalSettingsRoot.screenMargin
            rightMargin: additionalSettingsRoot.screenMargin
        }
        spacing: additionalSettingsRoot.mainSpacing

        MenuActionButton {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTr("ЖУРНАЛ СОБЫТИЙ")
            labelCentered: true
            iconSource: additionalSettingsRoot.iconsBasePath + "iconLog.png"
            iconSize: additionalSettingsRoot.menuActionIconSize
            accentColor: additionalSettingsRoot.fotekOrange
            textColor: additionalSettingsRoot.fotekBlue
            labelPixelSize: additionalSettingsRoot.menuActionLabelSize
            cornerRadius: additionalSettingsRoot.panelRadius
            onPressed: additionalSettingsRoot.logFileButtonPressed()
        }

        MenuActionButton {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTr("НАСТРОЙКА ДАТЫ И ВРЕМЕНИ")
            labelCentered: true
            iconSource: additionalSettingsRoot.iconsBasePath + "iconDate.png"
            iconSize: additionalSettingsRoot.menuActionIconSize
            accentColor: additionalSettingsRoot.fotekOrange
            textColor: additionalSettingsRoot.fotekBlue
            labelPixelSize: additionalSettingsRoot.menuActionLabelSize
            cornerRadius: additionalSettingsRoot.panelRadius
            onPressed: additionalSettingsRoot.dateTimeSettingsButtonPressed()
        }

        MenuActionButton {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTr("АКТИВАЦИЯ ДОПОЛНИТЕЛЬНЫХ ОПЦИЙ")
            labelCentered: true
            iconSource: additionalSettingsRoot.iconsBasePath + "iconOption.png"
            iconSize: additionalSettingsRoot.menuActionIconSize
            accentColor: additionalSettingsRoot.fotekOrange
            textColor: additionalSettingsRoot.fotekBlue
            labelPixelSize: additionalSettingsRoot.menuActionLabelSize
            maxLabelLines: 2
            cornerRadius: additionalSettingsRoot.panelRadius
            onPressed: additionalSettingsRoot.requestFeatureUnlock()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            color: "#00000028"
        }

        MenuActionButton {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: qsTr("СЕРВИСНОЕ МЕНЮ")
            labelCentered: true
            iconSource: additionalSettingsRoot.iconsBasePath + "iconPass.png"
            iconSize: additionalSettingsRoot.menuActionIconSize
            accentColor: additionalSettingsRoot.fotekOrange
            textColor: additionalSettingsRoot.fotekBlue
            labelPixelSize: additionalSettingsRoot.menuActionLabelSize
            cornerRadius: additionalSettingsRoot.panelRadius
            onPressed: additionalSettingsRoot.requestServiceMenuAccess()
        }
    }

    Dialog {
        id: servicePasswordDialog
        property string passwordError: ""
        parent: Overlay.overlay
        modal: true
        width: Math.min(Overlay.overlay ? Overlay.overlay.width * 0.92 : 760, 760)
        height: 360
        x: parent ? (parent.width - width) / 2 : 0
        // У верхнего края — кнопки остаются над виртуальной клавиатурой
        y: 48
        title: ""
        Overlay.modal: Rectangle {
            color: "#E8ECF2"
        }

        contentItem: Rectangle {
            color: "white"
            radius: additionalSettingsRoot.panelRadius

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 16

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Для доступа к сервисным функциям введите пароль")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: additionalSettingsRoot.fotekBlue
                    font.pixelSize: 28
                    font.bold: true
                }

                TextField {
                    id: servicePasswordInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    placeholderText: qsTr("Пароль")
                    echoMode: TextInput.Password
                    selectByMouse: true
                    activeFocusOnPress: true
                    font.pixelSize: 24
                    color: "black"
                    background: Rectangle {
                        color: "#F3F5F9"
                        border.color: servicePasswordInput.activeFocus
                                      ? additionalSettingsRoot.fotekOrange
                                      : "#C5CAD3"
                        border.width: 2
                        radius: 12
                    }
                    onAccepted: servicePasswordDialog.trySubmit()
                    onActiveFocusChanged: {
                        if (activeFocus)
                            Qt.inputMethod.show()
                    }

                    MouseArea {
                        anchors.fill: parent
                        propagateComposedEvents: true
                        onPressed: {
                            servicePasswordDialog.ensureKeyboard()
                            mouse.accepted = false
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: servicePasswordDialog.passwordError.length > 0
                    text: servicePasswordDialog.passwordError
                    color: "#dc2626"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 22
                }

                Item { Layout.fillHeight: true }
            }
        }

        function ensureKeyboard() {
            if (!servicePasswordInput.activeFocus)
                servicePasswordInput.forceActiveFocus()
            Qt.inputMethod.show()
        }

        function trySubmit() {
            var accessLevel = additionalSettingsRoot.resolveServiceMenuAccessLevel(servicePasswordInput.text)
            if (accessLevel !== "") {
                servicePasswordInput.text = ""
                servicePasswordDialog.passwordError = ""
                servicePasswordDialog.close()
                additionalSettingsRoot.serviceMenuButtonPressed(accessLevel)
                return
            }
            servicePasswordDialog.passwordError = qsTr("Неверный пароль")
        }

        onOpened: {
            servicePasswordInput.text = ""
            servicePasswordDialog.passwordError = ""
            Qt.callLater(function() {
                servicePasswordInput.forceActiveFocus()
                Qt.inputMethod.show()
            })
        }

        Connections {
            target: Qt.inputMethod
            enabled: servicePasswordDialog.visible
            function onVisibleChanged() {
                // После Hide снимаем фокус, чтобы следующий тап снова открыл клавиатуру.
                if (!Qt.inputMethod.visible && servicePasswordInput.activeFocus)
                    servicePasswordInput.focus = false
            }
        }

        onClosed: {
            servicePasswordInput.focus = false
            Qt.inputMethod.hide()
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 108

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                DialogActionButton {
                    Layout.preferredWidth: 200
                    Layout.fillHeight: true
                    text: qsTr("ОТМЕНА")
                    onPressed: servicePasswordDialog.close()
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 200
                    Layout.fillHeight: true
                    text: qsTr("ВОЙТИ")
                    primary: true
                    onPressed: servicePasswordDialog.trySubmit()
                }
            }
        }
    }

    Dialog {
        id: featureUnlockDialog
        property string passwordError: ""
        property string successMessage: ""
        property bool activationSucceeded: false
        readonly property bool showInputMode: !activationSucceeded && passwordError.length === 0
        readonly property bool showCloseOnly: activationSucceeded || passwordError.length > 0
        parent: Overlay.overlay
        modal: true
        width: Math.min(Overlay.overlay ? Overlay.overlay.width * 0.92 : 820, 820)
        height: 380
        x: parent ? (parent.width - width) / 2 : 0
        // У верхнего края — кнопки остаются над виртуальной клавиатурой
        y: 48
        title: ""
        Overlay.modal: Rectangle {
            color: "#E8ECF2"
        }

        contentItem: Rectangle {
            color: "white"
            radius: additionalSettingsRoot.panelRadius

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 16

                Label {
                    Layout.fillWidth: true
                    visible: featureUnlockDialog.showInputMode
                    text: qsTr("Для активации дополнительных опций введите ключ")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: additionalSettingsRoot.fotekBlue
                    font.pixelSize: 28
                    font.bold: true
                }

                TextField {
                    id: featureUnlockInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    visible: featureUnlockDialog.showInputMode
                    placeholderText: qsTr("123456789012")
                    echoMode: TextInput.Normal
                    maximumLength: 12
                    inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText
                    selectByMouse: true
                    activeFocusOnPress: true
                    font.pixelSize: 24
                    color: "black"
                    background: Rectangle {
                        color: "#F3F5F9"
                        border.color: featureUnlockInput.activeFocus
                                      ? additionalSettingsRoot.fotekOrange
                                      : "#C5CAD3"
                        border.width: 2
                        radius: 12
                    }
                    onTextChanged: {
                        var digits = additionalSettingsRoot.unlockKeyDigits(text)
                        if (text !== digits) {
                            text = digits
                            cursorPosition = text.length
                        }
                    }
                    onAccepted: featureUnlockDialog.trySubmit()
                    onActiveFocusChanged: {
                        if (activeFocus)
                            Qt.inputMethod.show()
                    }

                    MouseArea {
                        anchors.fill: parent
                        propagateComposedEvents: true
                        onPressed: {
                            featureUnlockDialog.ensureKeyboard()
                            mouse.accepted = false
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: featureUnlockDialog.showInputMode
                            && featureUnlockInput.text.length > 0
                    text: additionalSettingsRoot.formattedUnlockKey(featureUnlockInput.text)
                    color: "#5A6478"
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 22
                }

                Label {
                    Layout.fillWidth: true
                    visible: featureUnlockDialog.activationSucceeded
                    text: featureUnlockDialog.successMessage
                    color: "#2E7D32"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 30
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    visible: !featureUnlockDialog.activationSucceeded
                            && featureUnlockDialog.passwordError.length > 0
                    text: featureUnlockDialog.passwordError
                    color: "#dc2626"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 28
                    font.bold: true
                }

                Item { Layout.fillHeight: true }
            }
        }

        function resetActivationState() {
            featureUnlockDialog.activationSucceeded = false
            featureUnlockDialog.passwordError = ""
            featureUnlockDialog.successMessage = ""
        }

        function ensureKeyboard() {
            if (!featureUnlockInput.activeFocus)
                featureUnlockInput.forceActiveFocus()
            // Явно цифровая раскладка: после Hide/Loader буквы могли остаться.
            Qt.callLater(function() {
                InputEngine.symbolMode = false
                InputEngine.inputMode = InputEngine.Letters
                InputEngine.inputMode = InputEngine.DigitsOnly
                Qt.inputMethod.show()
            })
        }

        function trySubmit() {
            if (featureUnlockDialog.activationSucceeded) {
                featureUnlockDialog.close()
                return
            }

            featureUnlockDialog.successMessage = ""
            if (additionalSettingsRoot.serialNumberValue() < 260000) {
                featureUnlockDialog.passwordError = qsTr("Сначала сохраните серийный номер аппарата")
                Qt.inputMethod.hide()
                return
            }
            if (typeof featureUnlock === "undefined" || !featureUnlock) {
                featureUnlockDialog.passwordError = qsTr("Контроллер активации недоступен")
                Qt.inputMethod.hide()
                return
            }

            var activatedKey = featureUnlock.tryActivateWithUnlockKey(
                        additionalSettingsRoot.serialNumberValue(),
                        additionalSettingsRoot.deviceTypeValue(),
                        featureUnlockInput.text)
            if (activatedKey > 0) {
                featureUnlockInput.text = ""
                featureUnlockDialog.passwordError = ""
                featureUnlockDialog.successMessage = qsTr("Опция №%1 успешно активирована").arg(activatedKey)
                featureUnlockDialog.activationSucceeded = true
                Qt.inputMethod.hide()
                return
            }
            featureUnlockDialog.successMessage = ""
            featureUnlockDialog.passwordError = qsTr("Извините, ключ не верный! Попробуйте ещё раз или обратитесь к производителю")
            Qt.inputMethod.hide()
        }

        onOpened: {
            featureUnlockInput.text = ""
            featureUnlockDialog.resetActivationState()
            Qt.callLater(function() {
                featureUnlockDialog.ensureKeyboard()
            })
        }

        Connections {
            target: Qt.inputMethod
            enabled: featureUnlockDialog.visible
            function onVisibleChanged() {
                // После Hide снимаем фокус, чтобы следующий тап снова открыл клавиатуру.
                if (!Qt.inputMethod.visible && featureUnlockInput.activeFocus)
                    featureUnlockInput.focus = false
            }
        }

        onClosed: {
            featureUnlockInput.focus = false
            featureUnlockDialog.resetActivationState()
            Qt.inputMethod.hide()
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 108

            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 16

                DialogActionButton {
                    id: featureUnlockCancelButton
                    Layout.preferredWidth: 200
                    Layout.fillHeight: true
                    visible: featureUnlockDialog.showInputMode
                    text: qsTr("ОТМЕНА")
                    onPressed: featureUnlockDialog.close()
                }

                Item {
                    Layout.fillWidth: true
                    visible: featureUnlockDialog.showInputMode
                }

                DialogActionButton {
                    id: featureUnlockActivateButton
                    Layout.preferredWidth: 260
                    Layout.fillHeight: true
                    visible: featureUnlockDialog.showInputMode
                    text: qsTr("АКТИВИРОВАТЬ")
                    primary: true
                    onPressed: featureUnlockDialog.trySubmit()
                }

                DialogActionButton {
                    id: featureUnlockCloseButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: featureUnlockDialog.showCloseOnly
                    text: qsTr("ЗАКРЫТЬ")
                    primary: true
                    onPressed: featureUnlockDialog.close()
                }
            }
        }
    }
}
