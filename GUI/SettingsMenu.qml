import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Item {
    id: settingsMenuRoot

    signal returnButtonPressed()
    signal additionalSettingsButtonPressed()
    signal infoButtonPressed()

    property int volumeLevel: 3
    property bool clickSoundEnabled: true

    property color fotekBlue: "#264093"
    property color fotekOrange: "#faa731"
    property color pageBg: "#F3F5F9"
    property color panelBg: "#FFFFFF"
    property color trackIdle: "#D7DCE3"
    property color mutedBorder: "#C5CAD3"
    readonly property string iconsBasePath: AppPaths.iconsBaseUrl
    readonly property int screenMargin: 34
    readonly property int mainSpacing: 20
    readonly property int panelRadius: 20
    readonly property int panelPadding: 20
    readonly property int headerHeight: 70
    readonly property int menuActionLabelSize: 34
    readonly property int sectionTitleSize: 24
    readonly property int controlLabelSize: 22
    readonly property int menuActionIconSize: 76
    readonly property string currentLanguage: (typeof container !== "undefined" && container)
            ? container.normalizedLanguage(container.language)
            : "ru"

    function readVolumeLevel() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return 3
        }
        var raw = savedJson.readInt("volume", 3)
        if (raw > 3) {
            raw = Math.round((raw - 1) * 3 / 6)
        }
        return Math.max(0, Math.min(3, raw))
    }

    function applyVolumeLevel(level) {
        var clamped = Math.max(0, Math.min(3, Math.round(level)))
        volumeLevel = clamped
        if (typeof appControl !== "undefined" && appControl) {
            appControl.setVolumeLevel(clamped)
        }
        return clamped
    }

    function saveVolumeLevel(level) {
        var clamped = applyVolumeLevel(level)
        if (typeof savedJson !== "undefined" && savedJson) {
            savedJson.saveInt("volume", clamped)
        }
    }

    function readClickSoundEnabled() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return true
        }
        return savedJson.readInt("clickSound", 1) !== 0
    }

    function saveClickSoundEnabled(enabled) {
        clickSoundEnabled = enabled
        if (typeof uiClickSound !== "undefined" && uiClickSound) {
            uiClickSound.enabled = enabled
        }
        if (typeof savedJson !== "undefined" && savedJson) {
            savedJson.saveInt("clickSound", enabled ? 1 : 0)
        }
    }

    function setLanguage(langCode) {
        if (typeof container === "undefined" || !container) {
            return
        }
        container.language = langCode
    }

    Component.onCompleted: {
        volumeLevel = readVolumeLevel()
        clickSoundEnabled = readClickSoundEnabled()
        if (typeof uiClickSound !== "undefined" && uiClickSound) {
            uiClickSound.enabled = clickSoundEnabled
        }
    }

    Rectangle {
        anchors.fill: parent
        color: settingsMenuRoot.pageBg
    }

    Item {
        id: headerArea
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: settingsMenuRoot.screenMargin - 8
        }
        width: parent.width - settingsMenuRoot.screenMargin * 2
        height: settingsMenuRoot.headerHeight

        Text {
            id: screenTitle
            text: qsTr("НАСТРОЙКИ")
            anchors.centerIn: parent
            color: settingsMenuRoot.fotekBlue
            font.pixelSize: 48
            font.bold: true
        }
    }

    Item {
        id: footerArea
        height: 72
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: settingsMenuRoot.screenMargin
            rightMargin: settingsMenuRoot.screenMargin
            bottomMargin: settingsMenuRoot.screenMargin
        }

        DialogActionButton {
            id: retButton
            width: 180
            height: parent.height
            text: qsTr("НАЗАД")
            secondaryColor: settingsMenuRoot.fotekBlue
            secondaryBorderWidth: 1
            secondaryBorderColor: "#1E3274"
            cornerRadius: settingsMenuRoot.panelRadius
            labelPixelSize: 30
            onPressed: settingsMenuRoot.returnButtonPressed()
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
        }

//        Text {
//            id: footerDateTime
//            anchors {
//                right: parent.right
//                verticalCenter: parent.verticalCenter
//            }
//            text: (typeof dateTimeController !== "undefined" && dateTimeController)
//                  ? dateTimeController.currentDateTime
//                  : ""
//            color: settingsMenuRoot.fotekBlue
//            opacity: 0.72
//            font.pixelSize: 24
//            font.bold: false
//        }
    }

//    Timer {
//        interval: 1000
//        repeat: true
//        running: typeof dateTimeController !== "undefined" && dateTimeController
//        onTriggered: dateTimeController.refresh()
//    }

    Item {
        id: actionsArea
        anchors {
            top: headerArea.bottom
            topMargin: 22
            left: parent.left
            right: parent.right
            bottom: footerArea.top
            bottomMargin: settingsMenuRoot.mainSpacing
            leftMargin: settingsMenuRoot.screenMargin
            rightMargin: settingsMenuRoot.screenMargin
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: settingsMenuRoot.mainSpacing

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 248

                RowLayout {
                    anchors.fill: parent
                    spacing: settingsMenuRoot.mainSpacing

                    // Блок громкости + звук касания
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: settingsMenuRoot.panelRadius
                        color: settingsMenuRoot.panelBg
                        border.width: 1
                        border.color: settingsMenuRoot.mutedBorder

                        Item {
                            anchors {
                                fill: parent
                                margins: settingsMenuRoot.panelPadding
                            }

                            Text {
                                id: volumeTitle
                                text: qsTr("ГРОМКОСТЬ")
                                anchors {
                                    top: parent.top
                                    horizontalCenter: parent.horizontalCenter
                                }
                                color: settingsMenuRoot.fotekBlue
                                font.pixelSize: settingsMenuRoot.sectionTitleSize
                                font.bold: true
                                opacity: 0.92
                            }

                            Slider {
                                id: volumeSlider
                                objectName: "settingsVolumeSlider"
                                anchors {
                                    top: volumeTitle.bottom
                                    topMargin: 22
                                    left: parent.left
                                    right: parent.right
                                }
                                height: 44
                                from: 0
                                to: 3
                                stepSize: 1
                                snapMode: Slider.SnapAlways
                                value: settingsMenuRoot.volumeLevel
                                leftPadding: 18
                                rightPadding: 18
                                onPressedChanged: {
                                    if (pressed) {
                                        settingsMenuRoot.applyVolumeLevel(value)
                                        if (typeof uiClickSound !== "undefined" && uiClickSound) {
                                            uiClickSound.play(true)
                                        }
                                    } else {
                                        settingsMenuRoot.saveVolumeLevel(value)
                                    }
                                }
                                onMoved: settingsMenuRoot.applyVolumeLevel(value)

                                background: Item {
                                    x: volumeSlider.leftPadding
                                    y: volumeSlider.topPadding
                                       + volumeSlider.availableHeight / 2 - height / 2
                                    width: volumeSlider.availableWidth
                                    height: 28

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width
                                        height: 12
                                        radius: 6
                                        color: settingsMenuRoot.trackIdle

                                        Rectangle {
                                            width: volumeSlider.visualPosition * parent.width
                                            height: parent.height
                                            radius: parent.radius
                                            color: settingsMenuRoot.fotekOrange
                                        }
                                    }

                                    // Дискретные метки уровней 0..3 — небольшие кружки
                                    // с отступом от скруглений по краям полоски.
                                    Repeater {
                                        model: 4
                                        Rectangle {
                                            readonly property real endInset: 8
                                            readonly property real span: parent.width
                                                                        - 2 * endInset
                                            readonly property bool active:
                                                index === Math.round(volumeSlider.value)
                                            readonly property bool filled:
                                                index <= Math.round(volumeSlider.value)
                                            width: active ? 10 : 8
                                            height: width
                                            radius: width / 2
                                            color: filled ? "white" : settingsMenuRoot.trackIdle
                                            border.width: active ? 2 : 1
                                            border.color: filled
                                                          ? settingsMenuRoot.fotekBlue
                                                          : "#9AA3B2"
                                            x: endInset + index * (span / 3) - width / 2
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }

                                handle: Rectangle {
                                    x: volumeSlider.leftPadding + volumeSlider.visualPosition
                                          * (volumeSlider.availableWidth - width)
                                    y: volumeSlider.topPadding
                                          + volumeSlider.availableHeight / 2 - height / 2
                                    implicitWidth: 36
                                    implicitHeight: 36
                                    radius: width / 2
                                    color: "white"
                                    border.color: settingsMenuRoot.fotekOrange
                                    border.width: 3
                                }
                            }

                            Row {
                                id: clickSoundRow
                                anchors {
                                    left: parent.left
                                    top: volumeSlider.bottom
                                    topMargin: 28
                                }
                                spacing: 18
                                height: 48

                                // Только индикатор кликабелен; подпись снаружи Switch.
                                Switch {
                                    id: clickSoundSwitch
                                    width: 88
                                    height: parent.height
                                    checked: settingsMenuRoot.clickSoundEnabled
                                    text: ""
                                    padding: 0
                                    onToggled: settingsMenuRoot.saveClickSoundEnabled(checked)

                                    indicator: Rectangle {
                                        implicitWidth: 88
                                        implicitHeight: 48
                                        anchors.centerIn: parent
                                        radius: height / 2
                                        color: clickSoundSwitch.checked
                                               ? settingsMenuRoot.fotekOrange
                                               : settingsMenuRoot.trackIdle
                                        border.width: clickSoundSwitch.checked ? 0 : 1
                                        border.color: settingsMenuRoot.mutedBorder

                                        Rectangle {
                                            x: clickSoundSwitch.checked
                                               ? parent.width - width - 4 : 4
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 40
                                            height: 40
                                            radius: width / 2
                                            color: "white"
                                            border.width: 1
                                            border.color: clickSoundSwitch.checked
                                                          ? settingsMenuRoot.fotekOrange
                                                          : settingsMenuRoot.mutedBorder
                                        }
                                    }

                                    contentItem: Item {}
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("ЗВУК КАСАНИЯ")
                                    color: settingsMenuRoot.fotekBlue
                                    font.pixelSize: settingsMenuRoot.controlLabelSize
                                    font.bold: true
                                    opacity: 0.88
                                }
                            }
                        }
                    }

                    // Блок языка
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: settingsMenuRoot.panelRadius
                        color: settingsMenuRoot.panelBg
                        border.width: 1
                        border.color: settingsMenuRoot.mutedBorder

                        Item {
                            anchors {
                                fill: parent
                                margins: settingsMenuRoot.panelPadding
                            }

                            Text {
                                id: languageTitle
                                text: qsTr("ЯЗЫК")
                                anchors {
                                    top: parent.top
                                    horizontalCenter: parent.horizontalCenter
                                }
                                color: settingsMenuRoot.fotekBlue
                                font.pixelSize: settingsMenuRoot.sectionTitleSize
                                font.bold: true
                                opacity: 0.92
                            }

                            Row {
                                id: languageFlagsRow
                                anchors {
                                    top: languageTitle.bottom
                                    topMargin: 18
                                    horizontalCenter: parent.horizontalCenter
                                }
                                spacing: 16

                                LanguageFlagButton {
                                    langCode: "ru"
                                    langLabel: "RU"
                                    selected: settingsMenuRoot.currentLanguage === "ru"
                                    onChosen: settingsMenuRoot.setLanguage(langCode)
                                }

                                LanguageFlagButton {
                                    langCode: "en"
                                    langLabel: "EN"
                                    selected: settingsMenuRoot.currentLanguage === "en"
                                    onChosen: settingsMenuRoot.setLanguage(langCode)
                                }

                                LanguageFlagButton {
                                    langCode: "es"
                                    langLabel: "ES"
                                    selected: settingsMenuRoot.currentLanguage === "es"
                                    onChosen: settingsMenuRoot.setLanguage(langCode)
                                }
                            }
                        }
                    }
                }
            }

            MenuActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("СВЕДЕНИЯ ОБ АППАРАТЕ")
                labelCentered: true
                iconSource: settingsMenuRoot.iconsBasePath + "iconInfo.png"
                iconSize: settingsMenuRoot.menuActionIconSize
                accentColor: settingsMenuRoot.fotekOrange
                textColor: settingsMenuRoot.fotekBlue
                labelPixelSize: settingsMenuRoot.menuActionLabelSize
                cornerRadius: settingsMenuRoot.panelRadius
                onPressed: settingsMenuRoot.infoButtonPressed()
            }

            MenuActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("ДОПОЛНИТЕЛЬНЫЕ НАСТРОЙКИ")
                labelCentered: true
                iconSource: settingsMenuRoot.iconsBasePath + "iconSetting.png"
                iconSize: settingsMenuRoot.menuActionIconSize
                accentColor: settingsMenuRoot.fotekOrange
                textColor: settingsMenuRoot.fotekBlue
                labelPixelSize: settingsMenuRoot.menuActionLabelSize
                maxLabelLines: 2
                cornerRadius: settingsMenuRoot.panelRadius
                onPressed: settingsMenuRoot.additionalSettingsButtonPressed()
            }
        }
    }

    component LanguageFlagButton: Button {
        id: flagButton
        property string langCode: "ru"
        property string langLabel: "RU"
        property bool selected: false
        signal chosen()

        implicitWidth: 124
        implicitHeight: 100
        padding: 0
        opacity: flagButton.selected ? 1.0 : 0.78

        background: Rectangle {
            radius: settingsMenuRoot.panelRadius
            color: flagButton.selected ? "#FFFBF3" : "#FAFBFC"
            border.width: flagButton.selected ? 3 : 1
            border.color: flagButton.selected
                          ? settingsMenuRoot.fotekOrange
                          : settingsMenuRoot.mutedBorder
        }

        contentItem: ColumnLayout {
            spacing: 8

            Item {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 72
                Layout.preferredHeight: 44

                Loader {
                    anchors.fill: parent
                    opacity: flagButton.selected ? 1.0 : 0.9
                    sourceComponent: {
                        if (flagButton.langCode === "ru")
                            return russianFlagComponent
                        if (flagButton.langCode === "en")
                            return englishFlagComponent
                        return spanishFlagComponent
                    }
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: flagButton.langLabel
                color: settingsMenuRoot.fotekBlue
                font.pixelSize: 20
                font.bold: flagButton.selected
                opacity: flagButton.selected ? 1.0 : 0.75
            }
        }

        onClicked: flagButton.chosen()
    }

    Component {
        id: russianFlagComponent
        Item {
            Rectangle {
                anchors.fill: parent
                radius: 4
                color: "white"
                border.width: 1
                border.color: "#5C6575"
                clip: true

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }
                    height: parent.height / 3
                    color: "white"
                }

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    height: parent.height / 3
                    color: "#0039A6"
                }

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                    }
                    height: parent.height / 3
                    color: "#D52B1E"
                }
            }
        }
    }

    Component {
        id: englishFlagComponent
        Item {
            Rectangle {
                anchors.fill: parent
                radius: 4
                color: "#B22234"
                border.width: 1
                border.color: "#5C6575"
                clip: true

                Repeater {
                    model: 3
                    Rectangle {
                        width: parent.width
                        height: parent.height / 7
                        y: (index * 2 + 1) * parent.height / 7
                        color: "white"
                    }
                }

                Rectangle {
                    width: parent.width * 0.42
                    height: parent.height * 0.54
                    color: "#3C3B6E"
                }
            }
        }
    }

    Component {
        id: spanishFlagComponent
        Item {
            Rectangle {
                anchors.fill: parent
                radius: 4
                color: "#AA151B"
                border.width: 1
                border.color: "#5C6575"
                clip: true

                Rectangle {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                    }
                    height: parent.height / 2
                    color: "#F1BF00"
                }
            }
        }
    }
}
