import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15

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
    property string currentScopeName: ""
    property string currentProgramDisplayTitle: ""
    // "" | "free" | "last" | "named"
    property string currentProgramTitleKind: ""
    property int currentProgId: -1
    property int currentScopeId: -1
    property bool currentProgramIsUser: false
    property bool currentProgramIsRecom: false
    property bool hasUnsavedChanges: false
    property string language: "ru"
    property bool argonAvailable: true
    readonly property color fotekBlue: "#264093"
    readonly property color fotekOrange: "#faa731"
    readonly property color fotekGreen: "#77dd77"
    property string startupScreen: "startMenu"
    property bool startupInfoVisible: false
    property bool powerOffShutdownPending: false
    readonly property bool startupFlowVisible: startupScreen !== "mainScreen"


    // --- deferred WorkScreen ---
    readonly property var work: workScreenLoader.item
    readonly property bool workReady: workScreenLoader.status === Loader.Ready && workScreenLoader.item !== null
    property bool pendingShowMainScreen: false
    property bool workScreenLoadStarted: false

    function ensureWorkScreenLoading() {
        if (workScreenLoadStarted)
            return
        workScreenLoadStarted = true
        workScreenLoader.setSource("qrc:/WorkScreen.qml", { "host": container })
    }

    function onWorkScreenReady() {
        refreshStatusTitle()
        activationEnable()
        if (pendingShowMainScreen)
            showMainScreen()
    }

    function updateActivationOverlayGeometry() {
        if (!workReady)
            return
        var socketsDummy = work.socketsDummy
        var pedalContainer = work.pedalContainer
        var activationIndicator = work.activationIndicator
        if (!socketsDummy || !pedalContainer || !activationIndicator)
            return

        activationIndicator.x = socketsDummy.x
        activationIndicator.y = socketsDummy.y
        activationIndicator.width = Math.max(0, pedalContainer.x + pedalContainer.width - socketsDummy.x)
        activationIndicator.height = Math.max(0, socketsDummy.height)
    }

    function activationEnable() {
        if (!workReady) {
            periphHandle.enableActivation = false
            return
        }
        periphHandle.enableActivation = !(work.pedDrawer.opened
                                          | work.leftDrawer.opened
                                          | work.argonDrawer.opened
                                          | work.neutralDrawer.opened
                                          | work.socketsDummy.socketEditorOpened
                                          | work.socketsDummy.fullSocketEditorOpened
                                          | startupFlowVisible
                                          | powerOffShutdownPending
                                          | work.powerOffConfirmDialog.opened)
    }

    function openMainMenuFromStatus() {
        if (startupFlowVisible) {
            showStartupScreen("startMenu")
        } else if (workReady) {
            work.menuLoad.navigateTo("qrc:/MainMenu.qml")
            work.leftDrawer.open()
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
        if (!argonAvailable && workReady && work.argonDrawer.opened) {
            work.argonDrawer.close()
        }
    }

    function requestOpenArgonDrawer() {
        refreshArgonAvailability()
        if (!argonAvailable || !workReady) {
            return
        }
        if (work.activationHidesArgon) {
            return
        }
        work.argonDrawer.open()
    }

    function setCurrentProgram(scopeName, progName, isUserProgram, isRecomProgram, scopeId, progId) {
        if (isUserProgram === undefined) {
            isUserProgram = false
        }
        if (isRecomProgram === undefined) {
            isRecomProgram = false
        }
        if (scopeId === undefined) {
            scopeId = -1
        }
        if (progId === undefined) {
            progId = -1
        }
        currentProgramIsUser = isUserProgram
        currentProgramIsRecom = isRecomProgram
        currentProgramTitleKind = "named"
        currentScopeId = scopeId
        currentProgId = progId
        currentProgramDisplayTitle = scopeName + ": " + progName
        container.currentScopeName = scopeName
        container.currentProgName = progName
        resetUnsavedChanges()
        refreshStatusTitle()
        persistCurrentProgramInfo()
    }

    function setCurrentProgramTitle(titleText, titleKind) {
        currentProgramIsUser = false
        currentProgramIsRecom = false
        currentProgramTitleKind = titleKind || ""
        currentScopeId = -1
        currentProgId = -1
        currentProgramDisplayTitle = titleText
        container.currentScopeName = ""
        container.currentProgName = titleText
        resetUnsavedChanges()
        refreshStatusTitle()
        persistCurrentProgramInfo()
    }

    function refreshProgramTitleForLanguage() {
        if (currentProgramTitleKind === "free") {
            currentProgramDisplayTitle = qsTr("СВОБОДНЫЕ УСТАНОВКИ")
            container.currentProgName = currentProgramDisplayTitle
            container.currentScopeName = ""
        } else if (currentProgramTitleKind === "last") {
            currentProgramDisplayTitle = qsTr("Последние установки")
            container.currentProgName = currentProgramDisplayTitle
            container.currentScopeName = ""
        } else if (currentProgramIsRecom && currentProgId > 0) {
            var parts = appControl.localizedProgramTitle(currentScopeId, currentProgId)
            var scopeName = parts && parts.scopeName ? String(parts.scopeName) : ""
            var progName = parts && parts.progName ? String(parts.progName) : ""
            if (scopeName.length === 0 && progName.length === 0)
                return
            if (scopeName.length > 0)
                container.currentScopeName = scopeName
            if (progName.length > 0)
                container.currentProgName = progName
            if (container.currentScopeName.length > 0 && container.currentProgName.length > 0)
                currentProgramDisplayTitle = container.currentScopeName + ": " + container.currentProgName
            else if (container.currentProgName.length > 0)
                currentProgramDisplayTitle = container.currentProgName
        } else {
            return
        }
        refreshStatusTitle()
        persistCurrentProgramInfo()
    }

    function openProgramListFromStatus() {
        if (currentProgramIsRecom) {
            if (startupFlowVisible) {
                showStartupScreen("recommendedList")
            } else if (workReady) {
                work.menuLoad.loader.setSource("qrc:/ProgItemList.qml", {"recommended": true})
                work.leftDrawer.open()
            }
            return
        }
        if (currentProgramIsUser) {
            if (startupFlowVisible) {
                showStartupScreen("userProgramList")
            } else if (workReady) {
                work.menuLoad.loader.setSource("qrc:/ProgItemList.qml",
                                          {"recommended": false, "editable": true})
                work.leftDrawer.open()
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
        if (!workReady)
            return
        var suffix = hasUnsavedChanges ? "*" : ""
        work.statusDummy.text = currentProgramDisplayTitle + suffix
        work.statusDummy.saveHighlighted = hasUnsavedChanges
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
        if (workReady)
            work.statusDummy.saveHighlighted = false
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
        if (!workReady)
            return
        if (!work.leftDrawer.opened)
            work.leftDrawer.open()
    }

    function restoreMenuScreenAfterLanguageChange(preservedStartupScreen, preservedMenuSource, preservedDrawerOpen) {
        if (preservedStartupScreen === "settingsMenu" || preservedStartupScreen === "serviceMenu") {
            if (startupScreen !== preservedStartupScreen) {
                showStartupScreen(preservedStartupScreen)
            }
            // qsTr уже обновлён через retranslate(); разрушающий reload не нужен
            return
        }

        if (!preservedDrawerOpen || !workReady) {
            return
        }
        if (preservedMenuSource && preservedMenuSource.indexOf("MainMenu.qml") < 0) {
            if (work.menuLoad.loaderSourceString() !== preservedMenuSource) {
                work.menuLoad.navigateTo(preservedMenuSource)
            }
        }
        keepLeftDrawerOpen()
    }

    function persistCurrentProgramInfo() {
        savedJson.saveString("lastProgramDisplayName", currentProgramDisplayTitle)
        savedJson.saveString("lastProgramName", container.currentProgName)
        savedJson.saveString("lastProgramTitleKind", currentProgramTitleKind)
        savedJson.saveString("lastProgramIsUser", currentProgramIsUser ? "1" : "0")
        savedJson.saveString("lastProgramIsRecom", currentProgramIsRecom ? "1" : "0")
        savedJson.saveInt("lastProgramId", currentProgId)
        savedJson.saveInt("lastScopeId", currentScopeId)
    }

    function restoreCurrentProgramInfo() {
        var displayName = String(savedJson.readString("lastProgramDisplayName", "")).trim()
        var progName = String(savedJson.readString("lastProgramName", "")).trim()
        var restored = false

        currentProgramIsUser = savedJson.readString("lastProgramIsUser", "0") === "1"
        currentProgramIsRecom = savedJson.readString("lastProgramIsRecom", "0") === "1"
        currentProgramTitleKind = String(savedJson.readString("lastProgramTitleKind", "")).trim()
        currentProgId = savedJson.readInt("lastProgramId", -1)
        currentScopeId = savedJson.readInt("lastScopeId", -1)
        currentScopeName = ""

        if (displayName !== "") {
            currentProgramDisplayTitle = displayTitleWithoutUnsavedMark(displayName)
            var sep = currentProgramDisplayTitle.indexOf(": ")
            if (sep >= 0)
                currentScopeName = currentProgramDisplayTitle.substring(0, sep)
            restored = true
        }

        if (progName !== "") {
            container.currentProgName = progName
            restored = true
        } else if (displayName !== "") {
            container.currentProgName = displayTitleWithoutUnsavedMark(displayName)
        }
        resetUnsavedChanges()
        refreshProgramTitleForLanguage()
        refreshStatusTitle()

        return restored
    }

    function showStartupScreen(screenName) {
        startupScreen = screenName
        startupInfoVisible = false
        activationEnable()
    }

    function showMainScreen() {
        ensureWorkScreenLoading()
        if (!workReady) {
            pendingShowMainScreen = true
            return
        }
        pendingShowMainScreen = false
        if (work.leftDrawer.opened) {
            work.leftDrawer.close()
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
        case 0x46:
            return "#ff8a80"
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
        case 0x46: return qsTr("Ошибка модуля связи")
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
        Qt.callLater(ensureWorkScreenLoading)
    }

    onStartupScreenChanged: activationEnable()
    onStartupInfoVisibleChanged: activationEnable()
    onLanguageChanged: {
        var preservedStartupScreen = startupFlowVisible ? startupScreen : ""
        var preservedDrawerOpen = !startupFlowVisible && workReady && work.leftDrawer.opened
        var preservedMenuSource = preservedDrawerOpen ? work.menuLoad.loaderSourceString() : ""

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
            container.refreshProgramTitleForLanguage()
        }
        if (languageApplied && workReady && work.keyboardLoader.item)
            work.keyboardLoader.item.syncKeyboardLocales()

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

    Loader {
        id: workScreenLoader
        anchors.fill: parent
        z: 0
        asynchronous: true

        onStatusChanged: {
            if (status === Loader.Ready && item) {
                container.onWorkScreenReady()
            } else if (status === Loader.Error) {
                console.error("WorkScreen load failed:", source)
            }
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
                        container.setCurrentProgramTitle(qsTr("СВОБОДНЫЕ УСТАНОВКИ"), "free")
                        container.resetUnsavedChanges()
                        container.showMainScreen()
                    }
                    function onLastSettingsButtonPressed() {
                        recomHandle.loadLastSettings()
                        if (!container.restoreCurrentProgramInfo()) {
                            container.setCurrentProgramTitle(qsTr("Последние установки"), "last")
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

            Loader {
                id: recommendedProgramsLoader
                anchors.fill: parent
                Component.onCompleted: {
                    setSource("qrc:/ProgItemList.qml", {
                                  "recommended": true,
                                  "editable": false
                              })
                }
                Connections {
                    target: recommendedProgramsLoader.item
                    ignoreUnknownSignals: true
                    function onReturnButtonPressed() {
                        container.showStartupScreen("startMenu")
                    }
                    function onProgramSelected(scopeName, progName, scopeId, progId) {
                        container.setCurrentProgram(scopeName, progName, false, true, scopeId, progId)
                    }
                    function onClickedButton() {
                        container.showMainScreen()
                    }
                }
            }
        }

        Component {
            id: userProgramsComponent

            Loader {
                id: userProgramsLoader
                anchors.fill: parent
                Component.onCompleted: {
                    setSource("qrc:/ProgItemList.qml", {
                                  "recommended": false,
                                  "editable": true
                              })
                }
                Connections {
                    target: userProgramsLoader.item
                    ignoreUnknownSignals: true
                    function onReturnButtonPressed() {
                        container.showStartupScreen("startMenu")
                    }
                    function onProgramSelected(scopeName, progName, scopeId, progId) {
                        container.setCurrentProgram(scopeName, progName, true, false, scopeId, progId)
                    }
                    function onClickedButton() {
                        container.showMainScreen()
                    }
                }
            }
        }
    }

    // Индикация поверх Popup/Drawer: не перехватывает тач (enabled: false).
    Item {
        id: globalHudLayer
        parent: Overlay.overlay ? Overlay.overlay : container
        anchors.fill: parent
        z: 1000000
        enabled: false

        SystemMonitor {
            id: systemMonitor
            visible: appControl && appControl.cpuMonitorVisible
            anchors {
                right: parent.right
                top: parent.top
                margins: 10
            }
            monitoringActive: true
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
            color: "#99000000"
            radius: 8
            border.color: "#66ffffff"
            border.width: 1
            visible: appControl && appControl.debugUartEnabled
                     && appControl.debugOverlayText !== ""
            clip: true

            readonly property int textPadding: 10
            readonly property int maxVisibleLines: {
                var available = height - textPadding * 2
                var line = Math.max(1, debugFontMetrics.height)
                return Math.max(1, Math.floor(available / line))
            }
            readonly property string visibleDebugText: {
                var src = appControl && appControl.debugOverlayText
                          ? appControl.debugOverlayText : ""
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

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 83
            spacing: 8
            visible: periphHandle.activationStopWarningVisible

            Repeater {
                model: periphHandle.activationStopWarningCodes

                delegate: Rectangle {
                    required property var modelData

                    readonly property int warningCode: Number(modelData)
                    readonly property string warningText: container.warningTextForCode(warningCode)

                    width: Math.min(container.width - 80, warningTextLabel.implicitWidth + 32)
                    height: warningTextLabel.implicitHeight + 20
                    radius: 8
                    color: container.warningColorForCode(warningCode)
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
    }

}
