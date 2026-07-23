import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Item {
    id: settingsMenuRoot

    signal returnButtonPressed()
    signal additionalSettingsButtonPressed()
    signal infoButtonPressed()

    property int volumeLevel: 7

    property color fotekBlue: "#264093"
    property color fotekOrange: "#faa731"
    readonly property string iconsBasePath: AppPaths.iconsBaseUrl
    readonly property int screenMargin: 34
    readonly property int mainSpacing: 24
    readonly property int headerHeight: 70
    readonly property int menuActionLabelSize: 34
    readonly property int menuActionSmallLabelSize: 28
    readonly property string currentLanguage: (typeof container !== "undefined" && container)
            ? container.normalizedLanguage(container.language)
            : "ru"

    function readVolumeLevel() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return 7
        }
        return savedJson.readInt("volume", 7)
    }

    function saveVolumeLevel(level) {
        var clamped = Math.max(1, Math.min(7, Math.round(level)))
        volumeLevel = clamped
        if (typeof savedJson !== "undefined" && savedJson) {
            savedJson.saveInt("volume", clamped)
        }
        if (typeof appControl !== "undefined" && appControl) {
            appControl.setVolumeLevel(clamped)
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
    }

    Rectangle {
        anchors.fill: parent
        color: "#B8BEC8"
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
            cornerRadius: 20
            labelPixelSize: 30
            onPressed: settingsMenuRoot.returnButtonPressed()
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
        }

        Text {
            id: footerDateTime
            anchors {
                right: parent.right
                verticalCenter: parent.verticalCenter
            }
            text: (typeof dateTimeController !== "undefined" && dateTimeController)
                  ? dateTimeController.currentDateTime
                  : ""
            color: settingsMenuRoot.fotekBlue
            font.pixelSize: 26
            font.bold: true
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: typeof dateTimeController !== "undefined" && dateTimeController
        onTriggered: dateTimeController.refresh()
    }

    Item {
        id: actionsArea
        anchors {
            top: headerArea.bottom
            topMargin: 26
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
                Layout.preferredHeight: 148

                RowLayout {
                    anchors.fill: parent
                    spacing: settingsMenuRoot.mainSpacing

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Text {
                            text: qsTr("ГРОМКОСТЬ")
                            anchors {
                                top: parent.top
                                horizontalCenter: parent.horizontalCenter
                            }
                            color: settingsMenuRoot.fotekBlue
                            font.pixelSize: 28
                            font.bold: true
                        }

                        Slider {
                            id: volumeSlider
                            anchors {
                                top: parent.top
                                topMargin: 52
                                left: parent.left
                                right: parent.right
                                leftMargin: 12
                                rightMargin: 12
                            }
                            from: 1
                            to: 7
                            stepSize: 1
                            snapMode: Slider.SnapAlways
                            value: settingsMenuRoot.volumeLevel
                            onPressedChanged: {
                                if (!pressed) {
                                    settingsMenuRoot.saveVolumeLevel(value)
                                }
                            }
                            onMoved: settingsMenuRoot.volumeLevel = Math.round(value)

                            background: Rectangle {
                                x: volumeSlider.leftPadding
                                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 8
                                width: volumeSlider.availableWidth
                                height: implicitHeight
                                radius: 4
                                color: "#D7DCE3"

                                Rectangle {
                                    width: volumeSlider.visualPosition * parent.width
                                    height: parent.height
                                    color: settingsMenuRoot.fotekOrange
                                    radius: 4
                                }
                            }

                            handle: Rectangle {
                                x: volumeSlider.leftPadding + volumeSlider.visualPosition
                                      * (volumeSlider.availableWidth - width)
                                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                                implicitWidth: 28
                                implicitHeight: 28
                                radius: 14
                                color: "white"
                                border.color: settingsMenuRoot.fotekBlue
                                border.width: 2
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Text {
                            text: qsTr("ЯЗЫК")
                            anchors {
                                top: parent.top
                                horizontalCenter: parent.horizontalCenter
                            }
                            color: settingsMenuRoot.fotekBlue
                            font.pixelSize: 28
                            font.bold: true
                        }

                        RowLayout {
                            anchors {
                                top: parent.top
                                topMargin: 44
                                right: parent.right
                            }
                            spacing: 20

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

            MenuActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 112
                text: qsTr("СВЕДЕНИЯ ОБ АППАРАТЕ")
                labelCentered: true
                iconSource: ""
                iconSize: 0
                accentColor: settingsMenuRoot.fotekOrange
                textColor: settingsMenuRoot.fotekBlue
                labelPixelSize: settingsMenuRoot.menuActionLabelSize
                cornerRadius: 20
                onPressed: settingsMenuRoot.infoButtonPressed()
            }

            Item { Layout.fillHeight: true }

            MenuActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 112
                text: qsTr("ДОПОЛНИТЕЛЬНЫЕ НАСТРОЙКИ")
                labelCentered: true
                iconSource: ""
                iconSize: 0
                accentColor: settingsMenuRoot.fotekOrange
                textColor: settingsMenuRoot.fotekBlue
                labelPixelSize: settingsMenuRoot.menuActionSmallLabelSize
                maxLabelLines: 2
                cornerRadius: 20
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

        implicitWidth: 132
        implicitHeight: 96
        padding: 0

        background: Rectangle {
            radius: 16
            color: "white"
            border.width: flagButton.selected ? 3 : 1
            border.color: flagButton.selected ? settingsMenuRoot.fotekOrange : "#8A93A3"
        }

        contentItem: ColumnLayout {
            spacing: 8

            Item {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 72
                Layout.preferredHeight: 44

                Loader {
                    anchors.fill: parent
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
                font.pixelSize: 22
                font.bold: flagButton.selected
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
