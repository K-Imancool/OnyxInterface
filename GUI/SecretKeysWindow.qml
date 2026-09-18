import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtQuick.CuteKeyboard 1.0 as CuteKeyboardUi
import CuteKeyboard 1.0

import StratifyLabs.UI 2.0

ServiceFrame {
    id: secretKeysRoot
    title: qsTr("СЕКРЕТНЫЕ КЛЮЧИ")

    readonly property var deviceTypeOptions: ["ONYX-M", "ONYX-AM"]
    readonly property var featureKeyOptions: ["1", "2", "3", "4", "5", "6", "7", "8", "9"]
    readonly property int sectionFontSize: 32
    readonly property int labelFontSize: 28
    readonly property int fieldFontSize: 30
    readonly property int fieldHeight: 52
    readonly property int infoFontSize: 22
    readonly property int cardWidth: 610

    function featureCodeForKeyOption(keyOption) {
        return String(keyOption)
    }

    function uiLanguage() {
        if (typeof container !== "undefined" && container.language !== undefined)
            return container.normalizedLanguage(container.language)
        return "ru"
    }

    function keyboardPrimaryLayout() {
        var lang = uiLanguage()
        if (lang === "en")
            return "En"
        if (lang === "es")
            return "Es"
        return "Ru"
    }

    function availableKeyboardLayouts() {
        var lang = uiLanguage()
        if (lang === "en")
            return ["En"]
        if (lang === "es")
            return ["Es", "En"]
        return ["Ru", "En"]
    }

    Item {
        id: contentArea
        anchors {
            top: screenTitle.bottom
            topMargin: 8
            left: parent.left
            right: parent.right
            bottom: returnButton.top
            margins: 16
        }

        Column {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
            }
            spacing: 10
            width: secretKeysRoot.cardWidth * 2 + 16

            Row {
                spacing: 16
                width: parent.width

                Rectangle {
                    width: secretKeysRoot.cardWidth
                    height: generationColumn.height + 24
                    color: "#2a4a5a"
                    radius: 10
                    border.color: "#3d5a6a"
                    border.width: 2

                    Column {
                        id: generationColumn
                        x: 12
                        y: 12
                        width: parent.width - 24
                        spacing: 10

                        Text {
                            width: parent.width
                            color: "white"
                            font.pixelSize: secretKeysRoot.sectionFontSize
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Генерация ключа")
                        }

                        Row {
                            spacing: 12
                            Text {
                                text: qsTr("Серийный номер:")
                                color: "white"
                                font.pixelSize: secretKeysRoot.labelFontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            TextField {
                                id: serialNumberInput
                                width: 200
                                height: secretKeysRoot.fieldHeight
                                font.pixelSize: secretKeysRoot.fieldFontSize
                                placeholderText: "260000"
                                validator: IntValidator { bottom: 260000; top: 1000000 }
                                inputMethodHints: Qt.ImhDigitsOnly
                                color: "white"
                                background: Rectangle {
                                    color: "#1a2a3a"
                                    border.color: serialNumberInput.activeFocus ? "#4a9eff" : "#3a4a5a"
                                    border.width: 2
                                    radius: 5
                                }
                            }
                        }

                        Row {
                            spacing: 12
                            Text {
                                text: qsTr("Тип аппарата:")
                                color: "white"
                                font.pixelSize: secretKeysRoot.labelFontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            ComboBox {
                                id: generationDeviceType
                                width: 200
                                height: secretKeysRoot.fieldHeight
                                font.pixelSize: secretKeysRoot.fieldFontSize
                                model: secretKeysRoot.deviceTypeOptions
                                currentIndex: 1
                            }
                        }

                        Row {
                            spacing: 12
                            Text {
                                text: qsTr("Номер ключа:")
                                color: "white"
                                font.pixelSize: secretKeysRoot.labelFontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            ComboBox {
                                id: generationFeatureKey
                                width: 120
                                height: secretKeysRoot.fieldHeight
                                font.pixelSize: secretKeysRoot.fieldFontSize
                                model: secretKeysRoot.featureKeyOptions
                                currentIndex: 0
                            }
                        }

                        SButton {
                            style: "btn-primary"
                            text: qsTr("Сгенерировать ключ")
                            anchors.horizontalCenter: parent.horizontalCenter
                            onClicked: {
                                if (serialNumberInput.text !== "") {
                                    var serial = parseInt(serialNumberInput.text)
                                    var key = keyGenerator.generateUnlockKey(serial,
                                                                             generationDeviceType.currentText,
                                                                             secretKeysRoot.featureCodeForKeyOption(generationFeatureKey.currentText))
                                    generatedKeyDisplay.text = key
                                    generationResult.text = qsTr("✓ Ключ сгенерирован успешно!")
                                    generationResult.color = "#4ade80"
                                } else {
                                    generationResult.text = qsTr("✗ Введите серийный номер")
                                    generationResult.color = "#f87171"
                                }
                            }
                        }

                        Text {
                            text: qsTr("Сгенерированный ключ:")
                            color: "white"
                            font.pixelSize: secretKeysRoot.labelFontSize
                        }
                        TextField {
                            id: generatedKeyDisplay
                            width: 300
                            height: secretKeysRoot.fieldHeight
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: secretKeysRoot.fieldFontSize
                            font.bold: true
                            readOnly: true
                            color: "#4ade80"
                            background: Rectangle {
                                color: "#0a1a2a"
                                border.color: "#4a9eff"
                                border.width: 2
                                radius: 5
                            }
                        }

                        SLabel {
                            id: generationResult
                            style: "label-secondary sm"
                            text: ""
                            font.pixelSize: secretKeysRoot.labelFontSize
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: text != ""
                        }
                    }
                }

                Rectangle {
                    width: secretKeysRoot.cardWidth
                    height: validationColumn.height + 24
                    color: "#2a4a5a"
                    radius: 10
                    border.color: "#3d5a6a"
                    border.width: 2

                    Column {
                        id: validationColumn
                        x: 12
                        y: 12
                        width: parent.width - 24
                        spacing: 10

                        Text {
                            width: parent.width
                            color: "white"
                            font.pixelSize: secretKeysRoot.sectionFontSize
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Проверка ключа")
                        }

                        Row {
                            spacing: 12
                            Text {
                                text: qsTr("Серийный номер:")
                                color: "white"
                                font.pixelSize: secretKeysRoot.labelFontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            TextField {
                                id: checkSerialInput
                                width: 200
                                height: secretKeysRoot.fieldHeight
                                font.pixelSize: secretKeysRoot.fieldFontSize
                                placeholderText: "260000"
                                validator: IntValidator { bottom: 260000; top: 1000000 }
                                inputMethodHints: Qt.ImhDigitsOnly
                                color: "white"
                                background: Rectangle {
                                    color: "#1a2a3a"
                                    border.color: checkSerialInput.activeFocus ? "#4a9eff" : "#3a4a5a"
                                    border.width: 2
                                    radius: 5
                                }
                            }
                        }

                        Row {
                            spacing: 12
                            Text {
                                text: qsTr("Тип аппарата:")
                                color: "white"
                                font.pixelSize: secretKeysRoot.labelFontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            ComboBox {
                                id: validationDeviceType
                                width: 200
                                height: secretKeysRoot.fieldHeight
                                font.pixelSize: secretKeysRoot.fieldFontSize
                                model: secretKeysRoot.deviceTypeOptions
                                currentIndex: 1
                            }
                        }

                        Row {
                            spacing: 12
                            Text {
                                text: qsTr("Номер ключа:")
                                color: "white"
                                font.pixelSize: secretKeysRoot.labelFontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            ComboBox {
                                id: validationFeatureKey
                                width: 120
                                height: secretKeysRoot.fieldHeight
                                font.pixelSize: secretKeysRoot.fieldFontSize
                                model: secretKeysRoot.featureKeyOptions
                                currentIndex: 0
                            }
                        }

                        Text {
                            text: qsTr("Ключ для проверки:")
                            color: "white"
                            font.pixelSize: secretKeysRoot.labelFontSize
                        }
                        TextField {
                            id: keyToValidate
                            width: 300
                            height: secretKeysRoot.fieldHeight
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: secretKeysRoot.fieldFontSize
                            placeholderText: "123456789012"
                            maximumLength: 12
                            inputMethodHints: Qt.ImhDigitsOnly
                            color: "white"
                            background: Rectangle {
                                color: "#1a2a3a"
                                border.color: keyToValidate.activeFocus ? "#4a9eff" : "#3a4a5a"
                                border.width: 2
                                radius: 5
                            }
                        }

                        SButton {
                            style: "btn-info"
                            text: qsTr("Проверить ключ")
                            anchors.horizontalCenter: parent.horizontalCenter
                            onClicked: {
                                if (checkSerialInput.text !== "" && keyToValidate.text !== "") {
                                    var serial = parseInt(checkSerialInput.text)
                                    var isValid = keyGenerator.validateUnlockKey(serial,
                                                                                validationDeviceType.currentText,
                                                                                secretKeysRoot.featureCodeForKeyOption(validationFeatureKey.currentText),
                                                                                keyToValidate.text)
                                    if (isValid) {
                                        validationResult.text = qsTr("✓ КЛЮЧ ВЕРНЫЙ!")
                                        validationResult.color = "#4ade80"
                                    } else {
                                        validationResult.text = qsTr("✗ КЛЮЧ НЕВЕРНЫЙ!")
                                        validationResult.color = "#f87171"
                                    }
                                } else {
                                    validationResult.text = qsTr("✗ Заполните все поля")
                                    validationResult.color = "#fbbf24"
                                }
                            }
                        }

                        SLabel {
                            id: validationResult
                            style: "label-secondary md"
                            text: ""
                            font.bold: true
                            font.pixelSize: secretKeysRoot.labelFontSize
                            anchors.horizontalCenter: parent.horizontalCenter
                            visible: text != ""
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: infoColumn.height + 16
                color: "#1a2a3a"
                radius: 10
                border.color: "#2a3a4a"
                border.width: 1

                Column {
                    id: infoColumn
                    x: 8
                    y: 8
                    width: parent.width - 16
                    spacing: 4

                    Text {
                        width: parent.width
                        color: "#90caf9"
                        font.pixelSize: secretKeysRoot.infoFontSize
                        font.bold: true
                        text: qsTr("ℹ️ Информация:")
                    }
                    Text {
                        width: parent.width
                        color: "#cfd8dc"
                        font.pixelSize: secretKeysRoot.infoFontSize
                        wrapMode: Text.WordWrap
                        text: qsTr("• Ключи генерируются криптографическим алгоритмом SHA-256")
                    }
                    Text {
                        width: parent.width
                        color: "#cfd8dc"
                        font.pixelSize: secretKeysRoot.infoFontSize
                        wrapMode: Text.WordWrap
                        text: qsTr("• Для каждого типа аппарата и номера ключа формируется отдельный 12-значный код")
                    }
                    Text {
                        width: parent.width
                        color: "#cfd8dc"
                        font.pixelSize: secretKeysRoot.infoFontSize
                        wrapMode: Text.WordWrap
                        text: qsTr("• Соседние номера гарантированно имеют различные ключи")
                    }
                }
            }
        }
    }

    CuteKeyboardUi.InputPanel {
        id: inputPanel

        function tuneKeyboardTree(node) {
            if (!node)
                return
            if (node.autoRepeat !== undefined) {
                node.autoRepeat = node.btnKey !== undefined && node.btnKey === Qt.Key_Backspace
            }
            if (node.inputPanelRef !== undefined && !node.inputPanelRef)
                node.inputPanelRef = inputPanel
            if (node.item)
                tuneKeyboardTree(node.item)
            if (!node.children)
                return
            for (var i = 0; i < node.children.length; ++i) {
                tuneKeyboardTree(node.children[i])
            }
        }

        function keyboardFontFamily() {
            var fontName = STheme.font_family_base.name
            return fontName ? fontName : "DejaVu Sans"
        }

        function applyKeyboardFont() {
            var fontName = keyboardFontFamily()
            btnTextFontFamily = fontName
            InputPanel.btnTextFontFamily = fontName
        }

        function applyKeyboardUppercase() {
            InputEngine.uppercase = true
            Qt.callLater(function() { InputEngine.uppercase = true })
        }

        function applyTouchTuning() {
            applyKeyboardFont()
            applyKeyboardUppercase()
            tuneKeyboardTree(inputPanel)
        }

        function syncKeyboardLocales() {
            var layouts = secretKeysRoot.availableKeyboardLayouts()
            var primary = secretKeysRoot.keyboardPrimaryLayout()
            availableLanguageLayouts = layouts
            InputPanel.availableLanguageLayouts = layouts
            languageLayout = primary
            InputPanel.languageLayout = primary
            applyKeyboardFont()
            applyKeyboardUppercase()
        }

        z: 999
        y: secretKeysRoot.height
        languageLayout: secretKeysRoot.keyboardPrimaryLayout()
        availableLanguageLayouts: secretKeysRoot.availableKeyboardLayouts()
        btnTextFontFamily: STheme.font_family_base.name || "DejaVu Sans"
        anchors.left: parent.left
        anchors.right: parent.right

        onActiveChanged: {
            if (active) {
                syncKeyboardLocales()
                keyboardTuningTimer.restart()
                keyboardUppercaseTimer.restart()
            }
        }

        onLanguageLayoutChanged: keyboardTuningTimer.restart()

        Timer {
            id: keyboardTuningTimer
            interval: 40
            repeat: false
            onTriggered: {
                inputPanel.applyTouchTuning()
                Qt.callLater(inputPanel.applyTouchTuning)
            }
        }

        Timer {
            id: keyboardUppercaseTimer
            interval: 120
            repeat: false
            onTriggered: inputPanel.applyKeyboardUppercase()
        }

        states: State {
            name: "visible"
            when: Qt.inputMethod.visible
            PropertyChanges {
                target: inputPanel
                y: secretKeysRoot.height - inputPanel.height
            }
        }
        transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            ParallelAnimation {
                NumberAnimation {
                    properties: "y"
                    duration: 150
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
}
