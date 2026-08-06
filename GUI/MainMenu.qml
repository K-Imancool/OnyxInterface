import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15


Item {
    id: settingsScreen
    signal recommendButtonPressed()
    signal settingsButtonPressed()
    signal userButtonPressed()
    signal exitButtonPressed()
    signal secretKeysButtonPressed()
    signal userProgsButtonPressed()
    signal freeSettingsButtonPressed()
    signal lastSettingsButtonPressed()
    signal saveSettingsButtonPressed()
    signal serviceMenuButtonPressed()

    property bool startupMode: false
    property color fotekBlue: "#264093"
    property color fotekOrange: "#faa731"
    readonly property string iconsBasePath: AppPaths.iconsBaseUrl
    readonly property int screenMargin: 34
    readonly property int mainSpacing: startupMode ? 18 : 24
    readonly property int actionButtonHeight: startupMode ? 96 : 132
    readonly property int headerHeight: startupMode ? 110 : 70
    readonly property int videoButtonWidth: 500
    readonly property int videoButtonHeight: 80
    readonly property int menuActionIconSize: startupMode ? 96 : 96
    readonly property int menuActionLabelSize: 38
    readonly property int menuActionLargeLabelSize: 42
    readonly property int menuActionLargeIconSize: startupMode ? 112 : 112

    property bool videoPlayerVisible: false

    function closeVideoPlayer() {
        videoPlayerVisible = false
        videoClickGuard.stop()
        restoreClickSoundPreference()
    }

    function clickSoundPreferred() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return true
        }
        return savedJson.readInt("clickSound", 1) !== 0
    }

    function restoreClickSoundPreference() {
        if (typeof uiClickSound !== "undefined" && uiClickSound) {
            uiClickSound.enabled = clickSoundPreferred()
        }
    }

    function openVideoPlayer() {
        // Клик уже сыграл; не даём GStreamer перехватить ALSA на том же нажатии.
        if (typeof uiClickSound !== "undefined" && uiClickSound) {
            uiClickSound.enabled = false
        }
        videoOpenDelay.restart()
    }

    Timer {
        id: videoOpenDelay
        interval: 120
        repeat: false
        onTriggered: {
            settingsScreen.videoPlayerVisible = true
            videoClickGuard.restart()
        }
    }

    Timer {
        id: videoClickGuard
        interval: 350
        repeat: false
        onTriggered: settingsScreen.restoreClickSoundPreference()
    }

    Rectangle {
        anchors.fill: parent
        color: "#F3F5F9"
    }

    Item {
        id: headerArea
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: settingsScreen.screenMargin - 8
        }
        width: parent.width - settingsScreen.screenMargin * 2
        height: settingsScreen.headerHeight

        Image {
            id: logoImage
            visible: settingsScreen.startupMode
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            source: AppPaths.logoUrl
        }

        Text {
            id: screenTitle
            visible: !settingsScreen.startupMode
            text: qsTr("МЕНЮ")
            anchors.centerIn: parent
            color: settingsScreen.fotekBlue
            font.pixelSize: 48
            font.bold: true
        }
    }

    DialogActionButton {
        id: exitButton
        visible: !settingsScreen.startupMode
        width: 180
        height: 72
        text: qsTr("НАЗАД")
        secondaryColor: settingsScreen.fotekBlue
        secondaryBorderWidth: 1
        secondaryBorderColor: "#1E3274"
        cornerRadius: 20
        labelPixelSize: 30
        onPressed: {
            settingsScreen.closeVideoPlayer()
            settingsScreen.exitButtonPressed()
        }
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: settingsScreen.screenMargin
        }
    }

    Button {
        id: serviceMenuButton
        width: exitButton.width
        height: exitButton.height
        onPressed: settingsScreen.serviceMenuButtonPressed()
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: settingsScreen.screenMargin
        }

        background: Rectangle {
            radius: exitButton.cornerRadius
            color: "white"
            border.width: 1
            border.color: settingsScreen.fotekBlue
        }

        contentItem: Image {
            source: settingsScreen.iconsBasePath + "iconSetting.png"
            width: 48
            height: 48
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
        }
    }

    MenuActionButton {
        id: videoInstructionsButton
        z: 2
        width: settingsScreen.videoButtonWidth
        height: settingsScreen.videoButtonHeight
        text: qsTr("ВИДЕОИНСТРУКЦИИ")
        iconSource: settingsScreen.iconsBasePath + "iconVideo.png"
        accentColor: settingsScreen.fotekOrange
        textColor: settingsScreen.fotekBlue
        iconSize: 52
        labelPixelSize: 26
        cornerRadius: 20
        enabled: qmlGlAvailable
        onPressed: settingsScreen.openVideoPlayer()
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: settingsScreen.screenMargin
        }
    }

    Item {
        id: actionsArea
        anchors {
            top: headerArea.bottom
            topMargin: settingsScreen.startupMode ? 12 : 26
            left: parent.left
            right: parent.right
            bottom: videoInstructionsButton.top
            bottomMargin: settingsScreen.mainSpacing
            leftMargin: settingsScreen.screenMargin
            rightMargin: settingsScreen.screenMargin
        }

        RowLayout {
            anchors.fill: parent
            spacing: settingsScreen.mainSpacing

            MenuActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("РЕКОМЕНДУЕМЫЕ\nПРОГРАММЫ")
                iconSource: settingsScreen.iconsBasePath + "iconRecom.png"
                accentColor: settingsScreen.fotekOrange
                textColor: settingsScreen.fotekBlue
                iconSize: settingsScreen.menuActionLargeIconSize
                labelPixelSize: settingsScreen.menuActionLargeLabelSize
                onPressed: settingsScreen.recommendButtonPressed()
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: settingsScreen.mainSpacing

                MenuActionButton {
                    visible: settingsScreen.startupMode
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("ПОСЛЕДНИЕ\nУСТАНОВКИ")
                    iconSource: settingsScreen.iconsBasePath + "iconLast.png"
                    accentColor: settingsScreen.fotekOrange
                    textColor: settingsScreen.fotekBlue
                    iconSize: settingsScreen.menuActionIconSize
                    labelPixelSize: settingsScreen.menuActionLabelSize
                    onPressed: settingsScreen.lastSettingsButtonPressed()
                }

                MenuActionButton {
                    visible: !settingsScreen.startupMode
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("СОХРАНИТЬ\nПРОГРАММУ")
                    iconSource: settingsScreen.iconsBasePath + "iconSave.png"
                    accentColor: settingsScreen.fotekOrange
                    textColor: settingsScreen.fotekBlue
                    iconSize: settingsScreen.menuActionIconSize
                    labelPixelSize: settingsScreen.menuActionLabelSize
                    onPressed: settingsScreen.saveSettingsButtonPressed()
                }

                MenuActionButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("ПРОГРАММЫ\nПОЛЬЗОВАТЕЛЯ")
                    iconSource: settingsScreen.iconsBasePath + "iconUser.png"
                    accentColor: settingsScreen.fotekOrange
                    textColor: settingsScreen.fotekBlue
                    iconSize: settingsScreen.menuActionIconSize
                    labelPixelSize: settingsScreen.menuActionLabelSize
                    onPressed: settingsScreen.userButtonPressed()
                }

                MenuActionButton {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("СВОБОДНАЯ\nУСТАНОВКА")
                    iconSource: settingsScreen.iconsBasePath + "iconFree.png"
                    accentColor: settingsScreen.fotekOrange
                    textColor: settingsScreen.fotekBlue
                    iconSize: settingsScreen.menuActionIconSize
                    labelPixelSize: settingsScreen.menuActionLabelSize
                    onPressed: {
                        if (!settingsScreen.startupMode) {
                            recomHandle.loadFreeSettings()
                        }
                        settingsScreen.freeSettingsButtonPressed()
                    }
                }
            }
        }
    }

    Loader {
        id: videoPlayerLoader
        anchors.fill: parent
        z: 1000
        active: settingsScreen.videoPlayerVisible && qmlGlAvailable
        source: active ? "qrc:/VideoPlayer.qml" : ""
    }

    Connections {
        target: videoPlayerLoader.item
        ignoreUnknownSignals: true

        function onCloseRequested() {
            settingsScreen.closeVideoPlayer()
        }
    }
}
