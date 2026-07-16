import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.CuteKeyboard 1.0 as CuteKeyboardUi
import CuteKeyboard 1.0
import StratifyLabs.UI 2.0
import BackEnd 1.0

Window {
	id: container
	width: 1280
	height: 800
	visible: true
	title: qsTr("Ты волшебник, Гарри!")
	color: "black"

	// Константы для анимации панелей
	readonly property int panelAnimationDuration: 150
	readonly property int panelAnimationEasing: Easing.InOutQuad

    // Свойства для управления панелями
    property bool leftPanelExpanded: false
    property bool rightPanelExpanded: false
    readonly property int pedalPanelWidth: 100
    readonly property int argNeutralPanelWidth: 150
    readonly property int peripheryDrawerWidth: container.width * 0.5 + 25
    readonly property int pedalDrawerWidth: container.width * 0.5 - 25

    // Свойство для нейтрального электрода
    property bool neutralConnected: false
    
    // Текущее название программы
    property string currentProgName: ""
    property string currentProgramDisplayTitle: ""
    property bool currentProgramIsUser: false
    property bool currentProgramIsRecom: false
    property bool hasUnsavedChanges: false
    property string language: "ru"
    property bool argonAvailable: true
    readonly property color fotekBlue: "#264093"
    readonly property color fotekOrange: "#faa731"
    property string startupScreen: "startMenu"
    property bool startupInfoVisible: false
    property bool powerOffShutdownPending: false
    readonly property bool startupFlowVisible: startupScreen !== "mainScreen"

    function activationEnable() {
        periphHandle.enableActivation = !(pedDrawer.opened
                                          | leftDrawer.opened
                                          | argonDrawer.opened
                                          | neutralDrawer.opened
                                          | socketsDummy.socketEditorOpened
                                          | socketsDummy.fullSocketEditorOpened
                                          | startupFlowVisible
                                          | powerOffShutdownPending
                                          | powerOffConfirmDialog.opened)
    }

    function openMainMenuFromStatus() {
        if (startupFlowVisible) {
            showStartupScreen("startMenu")
        } else {
            menuLoad.navigateTo("qrc:/MainMenu.qml")
            leftDrawer.open()
        }
    }

    function readArgonAvailable() {
        if (typeof savedJson === "undefined" || !savedJson) {
            return true
        }
        var deviceType = String(savedJson.readString("deviceType", "ONYX-AM")).trim().toUpperCase()
        return deviceType === "ONYX-AM"
    }

    function refreshArgonAvailability() {
        argonAvailable = readArgonAvailable()
        if (!argonAvailable && argonDrawer.opened) {
            argonDrawer.close()
        }
    }

    function requestOpenArgonDrawer() {
        refreshArgonAvailability()
        if (!argonAvailable) {
            return
        }
        argonDrawer.open()
    }

    function setCurrentProgram(scopeName, progName, isUserProgram, isRecomProgram) {
        if (isUserProgram === undefined) {
            isUserProgram = false
        }
        if (isRecomProgram === undefined) {
            isRecomProgram = false
        }
        currentProgramIsUser = isUserProgram
        currentProgramIsRecom = isRecomProgram
        currentProgramDisplayTitle = scopeName + ": " + progName
        container.currentProgName = progName
        resetUnsavedChanges()
        refreshStatusTitle()
        persistCurrentProgramInfo()
    }

    function setCurrentProgramTitle(titleText) {
        currentProgramIsUser = false
        currentProgramIsRecom = false
        currentProgramDisplayTitle = titleText
        container.currentProgName = titleText
        resetUnsavedChanges()
        refreshStatusTitle()
        persistCurrentProgramInfo()
    }

    function openProgramListFromStatus() {
        if (currentProgramIsRecom) {
            if (startupFlowVisible) {
                showStartupScreen("recommendedList")
            } else {
                menuLoad.loader.setSource("qrc:/ProgItemList.qml", {"recommended": true})
                leftDrawer.open()
            }
            return
        }
        if (currentProgramIsUser) {
            if (startupFlowVisible) {
                showStartupScreen("userProgramList")
            } else {
                menuLoad.loader.setSource("qrc:/ProgItemList.qml",
                                          {"recommended": false, "editable": true})
                leftDrawer.open()
            }
            return
        }
        openMainMenuFromStatus()
    }

    function displayTitleWithoutUnsavedMark(value) {
        var textValue = String(value === undefined || value === null ? "" : value)
        return textValue.endsWith("*") ? textValue.slice(0, -1).trim() : textValue
    }

    function refreshStatusTitle() {
        var suffix = hasUnsavedChanges ? "*" : ""
        statusDummy.text = currentProgramDisplayTitle + suffix
        statusDummy.saveHighlighted = hasUnsavedChanges
    }

    function markUnsavedChanges() {
        if (!hasUnsavedChanges) {
            hasUnsavedChanges = true
            refreshStatusTitle()
        }
    }

    function resetUnsavedChanges() {
        if (hasUnsavedChanges) {
            hasUnsavedChanges = false
            refreshStatusTitle()
            return
        }
        statusDummy.saveHighlighted = false
    }

    function rolesContainAny(roles, expectedRoles) {
        if (!roles || roles.length === 0) {
            return false
        }
        for (var i = 0; i < expectedRoles.length; ++i) {
            if (roles.indexOf(expectedRoles[i]) >= 0) {
                return true
            }
        }
        return false
    }

    function normalizedLanguage(value) {
        if (typeof translationController !== "undefined" && translationController) {
            return translationController.normalizedLanguage(value)
        }

        var lang = String(value === undefined || value === null ? "" : value).trim().toLowerCase()
        return lang === "en" || lang === "es" ? lang : "ru"
    }

    function keyboardPrimaryLayout() {
        var lang = normalizedLanguage(container.language)
        if (lang === "en")
            return "En"
        if (lang === "es")
            return "Es"
        return "Ru"
    }

    function keyboardLayoutForLanguage() {
        return keyboardPrimaryLayout()
    }

    function availableKeyboardLayouts() {
        var lang = normalizedLanguage(container.language)
        if (lang === "en")
            return ["En"]
        if (lang === "es")
            return ["Es", "En"]
        return ["Ru", "En"]
    }

    function persistLanguage() {
        savedJson.saveString("language", container.language)
    }

    function restoreLanguage() {
        container.language = normalizedLanguage(savedJson.readString("language", "ru"))
        persistLanguage()
    }

    // Пока true — игнорируем closeMe/return из меню (смена языка не должна закрывать drawer)
    property bool suppressMenuNavigationForLanguageChange: false

    function keepLeftDrawerOpen() {
        slideMenuAnimation.stop()
        leftDrawer.opened = true
        leftDrawer.x = 0
    }

    function restoreMenuScreenAfterLanguageChange(preservedStartupScreen, preservedMenuSource, preservedDrawerOpen) {
        if (preservedStartupScreen === "settingsMenu" || preservedStartupScreen === "serviceMenu") {
            if (startupScreen !== preservedStartupScreen) {
                showStartupScreen(preservedStartupScreen)
            }
            // qsTr уже обновлён через retranslate(); разрушающий reload не нужен
            return
        }

        if (!preservedDrawerOpen) {
            return
        }
        if (preservedMenuSource && preservedMenuSource.indexOf("MainMenu.qml") < 0) {
            if (menuLoad.loaderSourceString() !== preservedMenuSource) {
                menuLoad.navigateTo(preservedMenuSource)
            }
        }
        keepLeftDrawerOpen()
    }

    function persistCurrentProgramInfo() {
        savedJson.saveString("lastProgramDisplayName", currentProgramDisplayTitle)
        savedJson.saveString("lastProgramName", container.currentProgName)
    }

    function restoreCurrentProgramInfo() {
        var displayName = String(savedJson.readString("lastProgramDisplayName", "")).trim()
        var progName = String(savedJson.readString("lastProgramName", "")).trim()
        var restored = false

        currentProgramIsUser = false
        currentProgramIsRecom = false

        if (displayName !== "") {
            currentProgramDisplayTitle = displayTitleWithoutUnsavedMark(displayName)
            restored = true
        }

        if (progName !== "") {
            container.currentProgName = progName
            restored = true
        } else if (displayName !== "") {
            container.currentProgName = displayTitleWithoutUnsavedMark(displayName)
        }
        resetUnsavedChanges()
        refreshStatusTitle()

        return restored
    }

    function showStartupScreen(screenName) {
        startupScreen = screenName
        startupInfoVisible = false
        activationEnable()
    }

    function showMainScreen() {
        if (leftDrawer.drawerActive) {
            leftDrawer.opened = false
            slideMenuAnimation.stop()
            leftDrawer.x = -container.width
        }
        startupScreen = "mainScreen"
        startupInfoVisible = false
        Qt.inputMethod.hide()
        activationEnable()
    }

    function warningColorForCode(code) {
        switch (code) {
        case 0x41:
            return "#fff176"
        case 0x44:
        case 0x45:
            return "#ffb74d"
        case 0x4F:
            return "#ff5252"
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x94:
        case 0x95:
        case 0x96:
        case 0x97:
        case 0x98:
            return "#ff5252"
        default:
            return "#ff8a80"
        }
    }

    function warningTextForCode(code) {
        switch (code) {
        case 0x01: return qsTr("Ошибка связи: передача не выполнена")
        case 0x02: return qsTr("Ошибка связи: нет ответа")
        case 0x03: return qsTr("Ошибка связи: неверный ответ")
        case 0x04: return qsTr("Ошибка связи: неверная длина пакета")
        case 0x05: return qsTr("Ошибка связи: CRC не совпадает")

        case 0x41: return qsTr("Активация остановлена: холостой ход (автостоп)")
        case 0x42: return qsTr("Активация остановлена: короткое замыкание бранш")
        case 0x43: return qsTr("Активация остановлена: обрыв нейтрального электрода")
        case 0x44: return qsTr("Активация остановлена: закончился аргон")
        case 0x45: return qsTr("Активация остановлена: непроходимость газового тракта")
        case 0x4F: return qsTr("Активация остановлена: ошибка генератора")

        case 0x80: return qsTr("Ошибка: модуль связи не принимает сигналы от МИФ")
        case 0x81: return qsTr("Ошибка: генератор не отвечает")
        case 0x82: return qsTr("Ошибка: газовый модуль не отвечает")
        case 0x83: return qsTr("Ошибка: не отвечает радиомодуль")
        case 0x84: return qsTr("Ошибка: кнопки или педали зажаты до старта")
        case 0x85: return qsTr("Ошибка: МК НЭ не отвечает")
        case 0x86: return qsTr("Ошибка: МК раскачки не отвечает")
        case 0x87: return qsTr("Ошибка: питание НЭ 5В не соответствует норме")
        case 0x88: return qsTr("Ошибка: питание НЭ 3,3В не соответствует норме")
        case 0x89: return qsTr("Ошибка: перегрев контроллера НЭ")
//        case 0x8C: return qsTr("Ошибка: кнопка питания зажата")
        case 0x8D: return qsTr("Ошибка: обновление не выполнено")
        case 0x8E: return qsTr("Ошибка: нет рабочей прошивки МУС")

        case 0x90: return qsTr("Критичная ошибка: ИСН при включении")
        case 0x91: return qsTr("Критичная ошибка: АЦП1 (напряжение контура)")
        case 0x92: return qsTr("Критичная ошибка: АЦП2 (ток контура)")
        case 0x93: return qsTr("Критичная ошибка: АЦП3 (ток генератора)")
        case 0x94: return qsTr("Критичная ошибка: АЦП4 (напряжение ИСН)")
        case 0x95: return qsTr("Критичная ошибка: реле")
        case 0x96: return qsTr("Критичная ошибка: ИСН при нормальной работе")
        case 0x97: return qsTr("Критичная ошибка: не найден резонанс при калибровке НЭ")
        case 0x98: return qsTr("Критичная ошибка: АЦП схемы НЭ")

        default:
            return qsTr("Ошибка устройства (код 0x") + code.toString(16).toUpperCase() + ")"
        }
    }

    Component.onCompleted: {
        Qt.inputMethod.hide()
        restoreLanguage()
        restoreCurrentProgramInfo()
        refreshArgonAvailability()
        activationEnable()
    }

    onStartupScreenChanged: activationEnable()
    onStartupInfoVisibleChanged: activationEnable()
    onLanguageChanged: {
        var preservedStartupScreen = startupFlowVisible ? startupScreen : ""
        var preservedDrawerOpen = !startupFlowVisible && leftDrawer.opened
        var preservedMenuSource = preservedDrawerOpen ? menuLoad.loaderSourceString() : ""

        suppressMenuNavigationForLanguageChange = true

        var normalized = normalizedLanguage(container.language)
        if (normalized !== container.language) {
            container.language = normalized
            return
        }
        persistLanguage()
        var languageApplied = true
        if (typeof translationController !== "undefined" && translationController) {
            if (!translationController.setLanguage(normalized)) {
                languageApplied = false
                if (container.language !== "ru") {
                    container.language = "ru"
                    return
                }
            } else {
                // Имена режимов/инструментов читаются из БД при загрузке программы,
                // поэтому пересобираем модель сокетов из сохранённого состояния
                recomHandle.saveCurrentState()
                recomHandle.loadLastSettings()
            }
        }
        if (languageApplied && keyboardLoader.item)
            keyboardLoader.item.syncKeyboardLocales()

        // Сразу оставляем drawer открытым — loadLastSettings / retranslate не должны его схлопнуть
        if (preservedDrawerOpen) {
            keepLeftDrawerOpen()
        }

        Qt.callLater(function() {
            if (languageApplied) {
                container.restoreMenuScreenAfterLanguageChange(
                            preservedStartupScreen, preservedMenuSource, preservedDrawerOpen)
            }
            container.suppressMenuNavigationForLanguageChange = false
        })
    }

   StatusBar {
      id: statusDummy
      //я искал панграммы для русского и хорошо так посмеялся с эфы
      text: qsTr("")
//      versionText: qsTr("Текущая версия: ") + appVersion
      width: parent.width
      height: 75
      anchors {
         top: parent.top
      }
   }
   Connections {
       target: theModel
       function onDataChanged(topLeft, bottomRight, roles) {
           var dirtyRoles = [
               SocketModel.CoagModeIndex,
               SocketModel.CutModeIndex,
               SocketModel.CoagModeId,
               SocketModel.CutModeId,
               SocketModel.CoagModePower,
               SocketModel.CutModePower,
               SocketModel.CoagModeInstrID,
               SocketModel.CutModeInstrID,
               SocketModel.CoagModeInstrIndex,
               SocketModel.CutModeInstrIndex,
               SocketModel.SocketPedal
           ]
           if (rolesContainAny(roles, dirtyRoles)) {
               markUnsavedChanges()
           }
       }
       function onSubProgCountChanged() {
           if (!container.startupFlowVisible) {
               markUnsavedChanges()
           }
       }
   }

    Column {
        id: activationStopWarningList
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: statusDummy.bottom
        anchors.topMargin: 8
        spacing: 8
        z: 12000
        visible: periphHandle.activationStopWarningVisible

        Repeater {
            model: periphHandle.activationStopWarningCodes

            delegate: Rectangle {
                required property var modelData

                readonly property int warningCode: Number(modelData)
                readonly property string warningText: warningTextForCode(warningCode)

                width: Math.min(container.width - 80, warningTextLabel.implicitWidth + 32)
                height: warningTextLabel.implicitHeight + 20
                radius: 8
                color: warningColorForCode(warningCode)
                border.color: "#212121"
                border.width: 1

                Text {
                    id: warningTextLabel
                    anchors.centerIn: parent
                    text: parent.warningText
                    color: "#111111"
                    font.pixelSize: 22
                    font.bold: true
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    SocketContainerV2 {
        id: socketsDummy
        objectName: "socketContainer"
        innerModel: theModel
        anchors {
            left: argNeutralPanel.right
            right: pedalContainer.left
            bottom: parent.bottom
            top: statusDummy.bottom
        }
    }

    PeripheryPanel {
        id: argNeutralPanel
        width: argNeutralPanelWidth
        argonAvailable: container.argonAvailable
        anchors {
            left: parent.left
            bottom: parent.bottom
            top: statusDummy.bottom
        }
    }
    ArgonDrawer {
        id: argonDrawer
        y: 0
        width: container.peripheryDrawerWidth
        height: container.height
        edge: Qt.LeftEdge
    }

    NeutralDrawer {
        id: neutralDrawer
        y: 0
        width: container.peripheryDrawerWidth
        height: container.height
        edge: Qt.LeftEdge
    }

    PedalContainer {
        id: pedalContainer
        innerModel: theModel
        width: pedalPanelWidth
        anchors {
            right: parent.right
            bottom: parent.bottom
            top: statusDummy.bottom
        }
    }

	PedalDrawer {
		id: pedDrawer
		innerModel: theModel
		width: container.pedalDrawerWidth
		height: container.height
		edge: Qt.RightEdge
	}

    // Глобальный отладочный индикатор тача временно отключён,
    // чтобы гарантированно не влиять на обработку событий в приложении.

    Item {
        id: leftDrawer
        readonly property bool drawerActive: opened || slideMenuAnimation.running
        width: drawerActive ? container.width : 0
        height: drawerActive ? container.height : 0
        x: -container.width
        y: 0
        z: drawerActive ? 9998 : -1
        visible: drawerActive
        enabled: drawerActive

        property bool opened: false

        function open() {
            opened = true
            slideMenuAnimation.to = 0
            slideMenuAnimation.start()
        }

        function close() {
            slideMenuAnimation.to = -container.width
            slideMenuAnimation.start()
        }

        NumberAnimation {
            id: slideMenuAnimation
            target: leftDrawer
            property: "x"
            duration: container.panelAnimationDuration
            easing.type: container.panelAnimationEasing
            onFinished: {
                if (leftDrawer.x <= -container.width) {
                    leftDrawer.opened = false
                }
            }
        }

        MenuLoader {
            id: menuLoad
            anchors.fill: parent
        }
    }

    Item {
        id: startupOverlay
        anchors.fill: parent
        z: 20000
        visible: startupFlowVisible
        enabled: startupFlowVisible

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            z: 0
            onPressed: function(mouse) { mouse.accepted = true }
            onReleased: function(mouse) { mouse.accepted = true }
            onClicked: function(mouse) { mouse.accepted = true }

            Loader {
            id: startupContentLoader
            anchors.fill: parent
            sourceComponent: {
                switch (container.startupScreen) {
                case "recommendedList":
                    return recommendedProgramsComponent
                case "userProgramList":
                    return userProgramsComponent
                case "settingsMenu":
                    return startupSettingsMenuComponent
                case "serviceMenu":
                    return startupSettingsMenuComponent
                default:
                    return startupMenuComponent
                }
            }
            }
        }

        Loader {
            id: startupAboutLoader
            anchors.fill: parent
            active: container.startupInfoVisible
            visible: active
            source: "qrc:/StartupInfoScreen.qml"
        }

        Connections {
            target: startupAboutLoader.item
            ignoreUnknownSignals: true
            function onReturnButtonPressed() {
                container.startupInfoVisible = false
            }
        }

        Component {
            id: startupMenuComponent

            Loader {
                id: startupMainMenuLoader
                anchors.fill: parent
                source: "qrc:/MainMenu.qml"

                onItemChanged: {
                    if (!item)
                        return
                    item.startupMode = true
                    item.fotekBlue = container.fotekBlue
                    item.fotekOrange = container.fotekOrange
                }

                Connections {
                    target: startupMainMenuLoader.item
                    ignoreUnknownSignals: true
                    function onRecommendButtonPressed() {
                        container.showStartupScreen("recommendedList")
                    }
                    function onUserButtonPressed() {
                        container.showStartupScreen("userProgramList")
                    }
                    function onFreeSettingsButtonPressed() {
                        recomHandle.loadEmptyFreeSettings()
                        container.setCurrentProgramTitle(qsTr("СВОБОДНЫЕ УСТАНОВКИ"))
                        container.resetUnsavedChanges()
                        container.showMainScreen()
                    }
                    function onLastSettingsButtonPressed() {
                        recomHandle.loadLastSettings()
                        if (!container.restoreCurrentProgramInfo()) {
                            container.setCurrentProgramTitle(qsTr("Последние установки"))
                        }
                        container.showMainScreen()
                    }
                    function onServiceMenuButtonPressed() {
                        container.showStartupScreen("settingsMenu")
                    }
                }
            }
        }

        Component {
            id: startupSettingsMenuComponent

            MenuLoader {
                source: "qrc:/SettingsMenu.qml"
                closeOnServiceRootReturn: true
                onCloseMe: container.showStartupScreen("startMenu")
                onDeleteAllUserProgsRequested: recomHandle.deleteAllUserProgs()
            }
        }

        Component {
            id: recommendedProgramsComponent

            ProgItemList {
                recommended: true
                editable: false
                onReturnButtonPressed: container.showStartupScreen("startMenu")
                onProgramSelected: {
                    container.setCurrentProgram(scopeName, progName, false, true)
                }
                onClickedButton: container.showMainScreen()
            }
        }

        Component {
            id: userProgramsComponent

            ProgItemList {
                recommended: false
                editable: true
                onReturnButtonPressed: container.showStartupScreen("startMenu")
                onProgramSelected: {
                    container.setCurrentProgram(scopeName, progName, true, false)
                }
                onClickedButton: container.showMainScreen()
            }
        }
    }

	ProgSaveDialog {
		id: saveProgDialog
		width: 0.8 * parent.width
		height: 390
        x: (parent.width - width) / 2
        y: statusDummy.height
        originalProgName: container.currentProgName
        currentProgramIsUser: container.currentProgramIsUser
    }
    Dialog {
        id: powerOffConfirmDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        width: Math.min(container.width * 0.82, 900)
        height: 360
        x: Math.round((container.width - width) / 2)
        y: Math.round((container.height - height) / 2)

        property int secondsRemaining: 10

        function openWithTimeout(seconds) {
            secondsRemaining = seconds
            powerOffTimer.restart()
            open()
            periphHandle.enableActivation = false
        }

        function cancelPowerOff() {
            powerOffTimer.stop()
            close()
            appControl.cancelPowerOff()
            container.activationEnable()
        }

        function confirmPowerOff() {
            powerOffTimer.stop()
            container.powerOffShutdownPending = true
            close()
            periphHandle.enableActivation = false
            appControl.confirmPowerOff()
        }

        background: Rectangle {
            color: "#f5f5f5"
            border.color: container.fotekBlue
            border.width: 3
            radius: 6
        }

        contentItem: Rectangle {
            color: "transparent"

            Text {
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 32
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Питание будет выключено через %1 секунд").arg(powerOffConfirmDialog.secondsRemaining)
                font.pixelSize: 38
                font.bold: true
                color: "black"
            }
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 118

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                spacing: 18

                DialogActionButton {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    text: qsTr("ОТМЕНА")
                    labelPixelSize: 32
                    onPressed: powerOffConfirmDialog.cancelPowerOff()
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 260
                    Layout.fillHeight: true
                    text: qsTr("ВЫКЛЮЧИТЬ")
                    primary: true
                    primaryEnabledColor: "#B71C1C"
                    labelPixelSize: 32
                    onPressed: powerOffConfirmDialog.confirmPowerOff()
                }
            }
        }

        Timer {
            id: powerOffTimer
            interval: 1000
            repeat: true
            onTriggered: {
                if (powerOffConfirmDialog.secondsRemaining <= 1) {
                    powerOffConfirmDialog.confirmPowerOff()
                } else {
                    powerOffConfirmDialog.secondsRemaining--
                }
            }
        }

        onOpened: periphHandle.enableActivation = false
    }
    Connections {
        target: appControl
        function onPowerOffConfirmationRequested(timeoutSeconds) {
            container.powerOffShutdownPending = false
            powerOffConfirmDialog.openWithTimeout(timeoutSeconds)
        }
    }
    Dialog {
        id: overwriteConfirmDialog
        modal: true
        width: saveProgDialog.width
        height: saveProgDialog.height
        x: saveProgDialog.x
        y: saveProgDialog.y

        contentItem: Rectangle {
            color: "transparent"

            Text {
                anchors.fill: parent
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Внимание! Текущая программа\n\n%1\n\nбудет перезаписана").arg(saveProgDialog.originalProgName)
                font.pixelSize: 30
                color: "black"
            }
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 108

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                spacing: 16

                DialogActionButton {
                    Layout.preferredWidth: 180
                    Layout.fillHeight: true
                    text: qsTr("ОТМЕНА")
                    onPressed: overwriteConfirmDialog.close()
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 180
                    Layout.fillHeight: true
                    text: qsTr("ПРИНЯТЬ")
                    primary: true
                    onPressed: {
                        overwriteConfirmDialog.close()
                        saveProgDialog.accept()
                    }
                }
            }
        }
    }
    Dialog {
        id: endoProgramMixDialog
        modal: true
        width: saveProgDialog.width
        height: saveProgDialog.height
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)

        background: Rectangle {
            color: "#f5f5f5"
            border.color: container.fotekBlue
            border.width: 3
        }

        contentItem: Rectangle {
            color: "transparent"

            Text {
                anchors.fill: parent
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: qsTr("Эндоскопические программы не могут быть использованы совместно с другими программами")
                font.pixelSize: 30
                color: "black"
            }
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 108

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                spacing: 16

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 180
                    Layout.fillHeight: true
                    text: qsTr("ПРИНЯТЬ")
                    primary: true
                    onPressed: endoProgramMixDialog.close()
                }

                Item { Layout.fillWidth: true }
            }
        }
    }
    Connections {
        target: recomHandle
        function onEndoProgramMixRejected() {
            endoProgramMixDialog.open()
        }
    }
    Connections {
        target: saveProgDialog
        function onOverwriteConfirmationRequested() {
            overwriteConfirmDialog.open()
        }
        function onAccepted() {
            recomHandle.saveProg(saveProgDialog.scopeName,
                                 saveProgDialog.progName)
            recomHandle.saveCurrentState()
            container.currentProgramIsUser = true
            container.setCurrentProgram(saveProgDialog.scopeName, saveProgDialog.progName, true, false)
            Qt.inputMethod.hide()
        }
        function onRejected() {
            Qt.inputMethod.hide()
        }
        // function onOpened() {
        //     // saveProgDialog.progName = ""
        // }
    }
    // Overlay для закрытия drawer'ов при касании вне их

    Connections {
        target: leftDrawer
        function onOpenedChanged() {
            if (!leftDrawer.opened) {
                container.refreshArgonAvailability()
            }
            container.activationEnable()
        }
    }
    Connections {
        target: pedDrawer
        function onOpenedChanged() {
            container.activationEnable()
        }
    }
    Connections {
        target: argonDrawer
        function onOpenedChanged() {
            container.activationEnable()
        }
    }
    Connections {
        target: neutralDrawer
        function onOpenedChanged() {
            container.activationEnable()
        }
    }
    Connections {
        target: socketsDummy
        function onSocketEditorOpenedChanged() {
            container.activationEnable()
        }
        function onFullSocketEditorOpenedChanged() {
            container.activationEnable()
        }
    }

    Connections {
        target: pedalContainer
        function onPedMenuRequest(socketId) {
            pedDrawer.socketId = socketId
            var targetHeight = pedalContainer.socketHeight(socketId)
            if (targetHeight > 0) {
                pedDrawer.y = pedalContainer.y + pedalContainer.socketTop(socketId)
                pedDrawer.height = targetHeight
            } else {
                pedDrawer.y = 0
                pedDrawer.height = container.height
            }
            pedDrawer.open()
        }
    }
    Connections {
        target: argNeutralPanel
        function onOpenArgonDrawer() {
            container.requestOpenArgonDrawer()
        }
        function onOpenNeutralDrawer() {
            neutralDrawer.open()
        }
    }

    Item {
        id: drawerOverlay
        anchors.fill: parent
        z: 1  // Выше основного контента, но drawer'ы будут иметь z намного выше (по умолчанию 10000)
        visible: argonDrawer.opened || neutralDrawer.opened || pedDrawer.opened

        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: 0.7

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    mouse.accepted = true
                }
                onReleased: {
                    if (argonDrawer.opened) argonDrawer.close()
                    if (neutralDrawer.opened) neutralDrawer.attemptClose()
                    if (pedDrawer.opened) pedDrawer.close()
                    mouse.accepted = true
                }
            }
        }
    }

    // MouseArea для обработки свайпов в области PeripheryPanel
    MouseArea {
        id: peripherySwipeArea
        anchors {
            left: parent.left
            top: statusDummy.bottom
            bottom: parent.bottom
        }
        width: 200
        z: 10  // Выше других элементов
        enabled: false
        propagateComposedEvents: true  // Ключевое свойство для пропуска событий

		property real startX: 0
		property real startY: 0
		property bool isSwipeGesture: false
		property bool hadSwipeGesture: false
		property real minSwipeDistance: 50

		onPressed: {
			startX = mouse.x
			startY = mouse.y
			isSwipeGesture = false
			hadSwipeGesture = false
		}

		onPositionChanged: {
			if (pressed) {
				var deltaX = mouse.x - startX
				var deltaY = Math.abs(mouse.y - startY)
				// Если горизонтальное движение больше вертикального и больше 30px вправо
				if (deltaX > 30 && Math.abs(deltaX) > deltaY) {
					isSwipeGesture = true
					hadSwipeGesture = true
					mouse.accepted = true
				} else if (isSwipeGesture) {
					mouse.accepted = true
				} else {
					mouse.accepted = false
				}
			}
		}

		onReleased: {
			if (isSwipeGesture) {
				var deltaX = mouse.x - startX
				// Если свайп вправо больше порога, открываем drawer
				if (deltaX > minSwipeDistance) {
					container.requestOpenArgonDrawer()
					mouse.accepted = true
					isSwipeGesture = false
					hadSwipeGesture = false
					return
				}
			}
			// Если был свайп, но не достиг порога, блокируем событие
			if (hadSwipeGesture) {
				mouse.accepted = true
			} else {
				// Если не было свайпа, пропускаем событие для клика
				mouse.accepted = false
			}
			isSwipeGesture = false
		}

    }

    // MouseArea для обработки свайпов в области PedalContainer
    MouseArea {
        id: pedalSwipeArea
        anchors {
            right: parent.right
            top: statusDummy.bottom
            bottom: parent.bottom
        }
        width: 200
        z: 10  // Выше других элементов
        enabled: false
        propagateComposedEvents: true

		property real startX: 0
		property real startY: 0
		property bool isSwipeGesture: false
		property bool hadSwipeGesture: false  // Сохраняем информацию о свайпе для onClicked
		property real minSwipeDistance: 50

		onPressed: {
			startX = mouse.x
			startY = mouse.y
			isSwipeGesture = false
			hadSwipeGesture = false
		}

		onPositionChanged: {
			if (pressed) {
				var deltaX = mouse.x - startX
				var deltaY = Math.abs(mouse.y - startY)
				// Если горизонтальное движение больше вертикального и больше 30px влево
				if (deltaX < -30 && Math.abs(deltaX) > deltaY) {
					isSwipeGesture = true
					hadSwipeGesture = true
					// Принимаем событие, чтобы оно не проходило дальше
					mouse.accepted = true
				} else if (isSwipeGesture) {
					// Если уже был свайп, продолжаем принимать события
					mouse.accepted = true
				} else {
					mouse.accepted = false
				}
			}
		}

        onReleased: {
            if (isSwipeGesture) {
                var deltaX = mouse.x - startX
                // Если свайп влево больше порога, открываем drawer
                if (deltaX < -minSwipeDistance) {
                    // Ищем expanded сокет
                    var expandedSocketId = -1
                    if (theModel) {
                        for (var i = 0; i < theModel.rowCount(); i++) {
                            var socketIndex = theModel.index(i, 0)
                            if (socketIndex.valid) {
                                var displayMode = theModel.data(socketIndex, SocketModel.SocketDisplayMode)
                                if (displayMode === "expanded") {
                                    expandedSocketId = i
                                    break
                                }
                            }
                        }
                        if (expandedSocketId < 0 && theModel.rowCount() > 0) {
                            expandedSocketId = 0
                        }
                    }
                    pedDrawer.socketId = expandedSocketId
                    pedDrawer.open()
                    mouse.accepted = true  // Блокируем событие при успешном свайпе
                    isSwipeGesture = false
                    hadSwipeGesture = false
                    return
                }
            }
            // Если был свайп, но не достиг порога, все равно блокируем событие
            if (hadSwipeGesture) {
                mouse.accepted = true
            } else {
            // Если не было свайпа, пропускаем событие для клика
                mouse.accepted = false
            }
            isSwipeGesture = false
            hadSwipeGesture = false
        }
    }

    Connections {
        target: statusDummy
        function onDrawerCalled() {
            leftDrawer.open()
        }
        function onSaveCalled() {
            saveProgDialog.open()
        }
        function onProgramTitlePressed() {
            container.openProgramListFromStatus()
        }
    }
    Connections {
        target: menuLoad
        function onCloseMe() {
            if (container.suppressMenuNavigationForLanguageChange) {
                container.keepLeftDrawerOpen()
                return
            }
            leftDrawer.close()
        }
        function onProgramSelected(scopeName, progName) {
            if (menuLoad.shortcut) {
                menuLoad.shortcut = false
                return
            }
            var isUserProgram = false
            var isRecomProgram = false
            if (menuLoad.loaderSourceBaseName() === "ProgItemList.qml" && menuLoad.loader.item) {
                isRecomProgram = menuLoad.loader.item.recommended
                isUserProgram = !isRecomProgram
            }
            container.setCurrentProgram(scopeName, progName, isUserProgram, isRecomProgram)
        }
        function onFreeSettingsModeActivated() {
            container.setCurrentProgramTitle(qsTr("СВОБОДНЫЕ УСТАНОВКИ"))
            container.markUnsavedChanges()
        }
        function onDeleteAllUserProgsRequested() {
            recomHandle.deleteAllUserProgs()
        }
    }
    Connections {
        target: socketsDummy
        function onProgAddRequest(addType) {
            switch (addType) {
            case 0:
            {
                recomHandle.copyCurrent();
                break;
            }
            case 1:
            {
                menuLoad.shortcut = true;
                menuLoad.loader.setSource("qrc:/ProgItemList.qml",
                                          {"recommended" : true,
                                              "loadClear" : false})
                leftDrawer.open()
                break;
            }
            case 2:
            {
                recomHandle.addEmptyDefault();
                break;
            }
            case 3:
            {
                menuLoad.shortcut = true;
                menuLoad.loader.setSource("qrc:/ProgItemList.qml",
                                          {"recommended" : false,
                                              "loadClear" : false})
                leftDrawer.open()
                break;
            }
            }
        }
    }

    // Монитор системы в правом верхнем углу
	SystemMonitor {
		id: systemMonitor
        visible: typeof appControl !== "undefined"
                 && appControl
                 && appControl.cpuMonitorVisible
		anchors {
			right: parent.right
            top: parent.top
			margins: 10
		}
		z: 9999  // Поверх всего
		monitoringActive: true

        // MouseArea для пропуска событий сквозь монитор
        MouseArea {
            anchors.fill: parent
            enabled: false  // Отключаем перехват событий - все проходят сквозь
        }
    }

    Rectangle {
        id: debugOverlay
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            leftMargin: 800
            topMargin: 300
            bottomMargin: 30
        }
//        height: Math.min(parent.height * 0.45, debugText.implicitHeight + 20)
        color: "#99000000"
        radius: 8
        border.color: "#66ffffff"
        border.width: 1
        visible: typeof appControl !== "undefined"
                 && appControl
                 && appControl.debugUartEnabled
                 && appControl.debugOverlayText !== ""
        z: 10000
        clip: true

        readonly property int textPadding: 10
        readonly property int maxVisibleLines: {
            var available = height - textPadding * 2
            var line = Math.max(1, debugFontMetrics.height)
            return Math.max(1, Math.floor(available / line))
        }
        readonly property string visibleDebugText: {
            var src = appControl && appControl.debugOverlayText ? appControl.debugOverlayText : ""
            if (src === "")
                return ""
            var lines = src.split("\n")
            var start = Math.max(0, lines.length - maxVisibleLines)
            return lines.slice(start).join("\n")
        }

        FontMetrics {
            id: debugFontMetrics
            font.family: "monospace"
            font.pixelSize: 18
        }

        Text {
            id: debugText
            anchors.fill: parent
            anchors.margins: debugOverlay.textPadding
            text: debugOverlay.visibleDebugText
            color: "white"
            wrapMode: Text.NoWrap
            font.family: "monospace"
            font.pixelSize: 18
            elide: Text.ElideNone
            clip: true
        }
    }


    // Область для свайпов и закрытия панелей
    // MouseArea {
    //    id: swipeArea
    //    anchors.fill: parent
    //    z: 25  // Всегда выше панелей для обработки свайпов
    //    propagateComposedEvents: true

	//    property real startX: 0
	//    property bool isSwipeGesture: false
	//    property real startTime: 0

	//    onPressed: {
	//       startX = mouse.x
	//       startTime = Date.now()
	//       isSwipeGesture = false

	//       // Вычисляем границы панелей
	//       var rightPanelLeftEdge = rightPanelExpanded ? (container.width - rightPanel.expandedWidth) : (container.width - 85)
	//       var leftPanelRightEdge = leftPanelExpanded ? (container.width / 2) : 85

	//       // Если панели открыты и клик вне их области - обрабатываем
	//       if (leftPanelExpanded && mouse.x > leftPanelRightEdge) {
	//          mouse.accepted = true
	//          return
	//       }

	//       if (rightPanelExpanded && mouse.x < rightPanelLeftEdge) {
	//          mouse.accepted = true
	//          return
	//       }

	//       // Проверяем области для свайпа:
	//       if ((mouse.x < 100) ||
	//             (mouse.x > container.width - 100) ||
	//             (leftPanelExpanded && mouse.x <= leftPanelRightEdge) ||
	//             (rightPanelExpanded && mouse.x >= rightPanelLeftEdge)) {
	//          isSwipeGesture = true
	//          mouse.accepted = true
	//       } else {
	//          // Центральная область (панели закрыты) - пропускаем событие к сокетам
	//          mouse.accepted = false
	//       }
	//    }

	//    onReleased: {
	//       if (!isSwipeGesture) {
	//          return
	//       }

	//       var deltaX = mouse.x - startX
	//       var threshold = 50
	//       var swipeThreshold = Math.abs(deltaX)

	//       if (swipeThreshold > threshold) {
	//          // Закрытие панелей имеет приоритет
	//          if (leftPanelExpanded && deltaX < -threshold) {
	//             leftPanelExpanded = false
	//             mouse.accepted = true
	//          } else if (rightPanelExpanded && deltaX > threshold) {
	//             rightPanelExpanded = false
	//             mouse.accepted = true
	//          }
	//          // Открытие панелей
	//          else if (!leftPanelExpanded && !rightPanelExpanded && startX < 100 && deltaX > threshold) {
	//             leftPanelExpanded = true
	//             mouse.accepted = true
	//          } else if (!leftPanelExpanded && !rightPanelExpanded && startX > container.width - 100 && deltaX < -threshold) {
	//             rightPanelExpanded = true
	//             mouse.accepted = true
	//          }
	//       }
	//    }

	//    onClicked: {
	//       var deltaX = Math.abs(mouse.x - startX)

	//       // Вычисляем границу правой панели (независимо от анимации)
	//       var rightPanelLeftEdge = rightPanelExpanded ? (container.width - rightPanel.expandedWidth) : (container.width - 85)
	//       var leftPanelRightEdge = leftPanelExpanded ? (container.width / 2) : 85

	//       // Игнорируем клики, которые являются частью свайпа
	//       if (deltaX > 30) {
	//          mouse.accepted = true
	//          return
	//       }

	//       // Закрываем ЛЕВУЮ панель при клике вне её области
	//       if (leftPanelExpanded && startX > leftPanelRightEdge) {
	//          leftPanelExpanded = false
	//          mouse.accepted = true
	//          return
	//       }

	//       // Закрываем ПРАВУЮ панель при клике вне её области (слева от панели)
	//       if (rightPanelExpanded && startX < rightPanelLeftEdge) {
	//          rightPanelExpanded = false
	//          mouse.accepted = true
	//          return
	//       }

	//       // Клик по свёрнутой левой панели - разворачиваем
	//       if (!leftPanelExpanded && startX <= 85) {
	//          leftPanelExpanded = true
	//          mouse.accepted = true
	//          return
	//       }

	//       // Клик по свёрнутой правой панели - разворачиваем
	//       if (!rightPanelExpanded && startX >= container.width - 85) {
	//          // Используем функцию из PedalPanel для определения сокета по клику
	//          var socketIndex = rightPanel.findSocketIndexByClick(mouse.x, mouse.y)

	//          if (socketIndex >= 0) {
	//             rightPanel.lastClickedSocketIndex = socketIndex
	//             rightPanel.openedByPedalClick = true
	//          }

   // }
   
   // Клавиатура только в дереве, когда реально нужна — иначе InputPanel (z:9999) перехватывает тач
   Loader {
      id: keyboardLoader
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.bottom: parent.bottom
      z: 9999
      active: Qt.inputMethod.visible
      sourceComponent: keyboardPanelComponent
   }

   Component {
      id: keyboardPanelComponent

      CuteKeyboardUi.InputPanel {
         id: inputPanel
         y: container.height
         languageLayout: container.keyboardPrimaryLayout()
         availableLanguageLayouts: container.availableKeyboardLayouts()
         btnTextFontFamily: STheme.font_family_base.name || "DejaVu Sans"
         anchors.left: parent.left
         anchors.right: parent.right

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

         function syncKeyboardLocales() {
            var layouts = container.availableKeyboardLayouts()
            var primary = container.keyboardPrimaryLayout()
            availableLanguageLayouts = layouts
            InputPanel.availableLanguageLayouts = layouts
            languageLayout = primary
            InputPanel.languageLayout = primary
            applyKeyboardFont()
            applyKeyboardUppercase()
         }

         function tuneKeyboardTree(node) {
            if (!node)
               return
            if (node.autoRepeat !== undefined)
               node.autoRepeat = node.btnKey !== undefined && node.btnKey === Qt.Key_Backspace
            if (node.inputPanelRef !== undefined && !node.inputPanelRef)
               node.inputPanelRef = inputPanel
            if (node.item)
               tuneKeyboardTree(node.item)
            if (!node.children)
               return
            for (var i = 0; i < node.children.length; ++i)
               tuneKeyboardTree(node.children[i])
         }

         function applyTouchTuning() {
            applyKeyboardFont()
            applyKeyboardUppercase()
            tuneKeyboardTree(inputPanel)
         }

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
            when: inputPanel.active
            PropertyChanges {
               target: inputPanel
               y: container.height - inputPanel.height
            }
         }
         transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            ParallelAnimation {
               NumberAnimation {
                  properties: "y"
                  duration: 0
                  easing.type: Easing.InOutQuad
               }
            }
         }
      }
   }

    //       // Центральная область или внутри панели - пропускаем к дочерним элементам
    //       mouse.accepted = false
    //    }

	// }

}
