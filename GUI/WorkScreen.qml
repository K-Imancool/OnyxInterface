import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.CuteKeyboard 1.0 as CuteKeyboardUi
import CuteKeyboard 1.0
import BackEnd 1.0

Item {
    id: workRoot
    anchors.fill: parent

    // Окно-хозяин (main.qml / container)
    property var host

    // Публичные узлы для вызовов из main.qml
    property alias statusDummy: statusDummy
    property alias socketsDummy: socketsDummy
    property alias pedalContainer: pedalContainer
    property alias argNeutralPanel: argNeutralPanel
    property alias argonDrawer: argonDrawer
    property alias neutralDrawer: neutralDrawer
    property alias pedDrawer: pedDrawer
    property alias leftDrawer: leftDrawer
    property alias menuLoad: menuLoad
    property alias activationIndicator: activationIndicator
    property alias saveProgDialog: saveProgDialog
    property alias powerOffConfirmDialog: powerOffConfirmDialog
    property alias keyboardLoader: keyboardLoader
    property alias systemMonitor: systemMonitor

   StatusBar {
      id: statusDummy
      //я искал панграммы для русского и хорошо так посмеялся с эфы
      text: qsTr("")
//      versionText: qsTr("Текущая версия: ") + appVersion
      width: parent.width
      height: 75
      saveButtonWidth: host ? host.pedalPanelWidth : 100
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
           if (!host.startupFlowVisible) {
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

                width: Math.min(host.width - 80, warningTextLabel.implicitWidth + 32)
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
        activationOverlay: activationIndicator
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
        argonAvailable: host.argonAvailable
        anchors {
            left: parent.left
            bottom: parent.bottom
            top: statusDummy.bottom
        }
    }
    ArgonDrawer {
        id: argonDrawer
        y: 0
        width: host.peripheryDrawerWidth
        height: host.height
        edge: Qt.LeftEdge
    }

    NeutralDrawer {
        id: neutralDrawer
        y: 0
        width: host.peripheryDrawerWidth
        height: host.height
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

    Item {
        id: activationLayer
        anchors.fill: parent
        z: 5000

        Activation {
            id: activationIndicator
            parent: activationLayer

            onOpenedChanged: {
                if (opened) {
                    Qt.callLater(host.updateActivationOverlayGeometry)
                }
            }
        }

        Timer {
            id: activationGeometryTimer
            interval: 50
            repeat: true
            running: activationIndicator.opened
            onTriggered: host.updateActivationOverlayGeometry()
        }
    }

	PedalDrawer {
		id: pedDrawer
		innerModel: theModel
		width: host.pedalDrawerWidth
		height: host.height
		edge: Qt.RightEdge
	}

    // Полноэкранное меню (как FullSocketEditor), без выезда сбоку
    Popup {
        id: leftDrawer
        modal: true
        parent: Overlay.overlay
        width: parent ? parent.width : host.width
        height: parent ? parent.height : host.height
        x: 0
        y: 0
        padding: 0
        closePolicy: Popup.NoAutoClose
        enter: Transition {}
        exit: Transition {}

        readonly property bool drawerActive: opened

        background: Rectangle {
            color: "darkslategray"
        }

        MenuLoader {
            id: menuLoad
            anchors.fill: parent
            // MainMenu подгружается при открытии — не при создании WorkScreen
            source: ""
        }
    }

	ProgSaveDialog {
		id: saveProgDialog
		width: parent.width
        height: 490
        x: 0
        // У верхнего края: кнопки остаются над виртуальной клавиатурой
        y: 0
        originalProgName: host.currentProgName
        originalScopeName: host.currentScopeName
        currentProgramIsUser: host.currentProgramIsUser
    }
    Dialog {
        id: powerOffConfirmDialog
        modal: true
        closePolicy: Popup.NoAutoClose
        width: Math.min(host.width * 0.82, 900)
        height: 360
        x: Math.round((host.width - width) / 2)
        y: Math.round((host.height - height) / 2)

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
            host.activationEnable()
        }

        function confirmPowerOff() {
            powerOffTimer.stop()
            host.powerOffShutdownPending = true
            close()
            periphHandle.enableActivation = false
            appControl.confirmPowerOff()
        }

        background: Rectangle {
            color: "#f5f5f5"
            border.color: host.fotekBlue
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
            host.powerOffShutdownPending = false
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
                text: qsTr("Внимание! Программа\n\n%1\n\nбудет перезаписана").arg(saveProgDialog.progName)
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
                    Layout.preferredWidth: 220
                    Layout.fillHeight: true
                    text: qsTr("ОТМЕНА")
                    onPressed: overwriteConfirmDialog.close()
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 220
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
            border.color: host.fotekBlue
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
            host.currentProgramIsUser = true
            host.setCurrentProgram(saveProgDialog.scopeName, saveProgDialog.progName, true, false)
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
                host.refreshArgonAvailability()
            }
            host.activationEnable()
        }
    }
    Connections {
        target: pedDrawer
        function onOpenedChanged() {
            host.activationEnable()
        }
    }
    Connections {
        target: argonDrawer
        function onOpenedChanged() {
            host.activationEnable()
        }
    }
    Connections {
        target: neutralDrawer
        function onOpenedChanged() {
            host.activationEnable()
        }
    }
    Connections {
        target: socketsDummy
        function onSocketEditorOpenedChanged() {
            host.activationEnable()
        }
        function onFullSocketEditorOpenedChanged() {
            host.activationEnable()
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
                pedDrawer.height = host.height
            }
            pedDrawer.open()
        }
    }
    Connections {
        target: argNeutralPanel
        function onOpenArgonDrawer() {
            host.requestOpenArgonDrawer()
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
					host.requestOpenArgonDrawer()
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
            if (menuLoad.loaderSourceBaseName() === "")
                menuLoad.navigateTo("qrc:/MainMenu.qml")
            leftDrawer.open()
        }
        function onSaveCalled() {
            saveProgDialog.open()
        }
        function onProgramTitlePressed() {
            host.openProgramListFromStatus()
        }
    }
    Connections {
        target: menuLoad
        function onCloseMe() {
            if (host.suppressMenuNavigationForLanguageChange) {
                host.keepLeftDrawerOpen()
                return
            }
            leftDrawer.close()
        }
        function onSaveSettingsButtonPressed() {
            saveProgDialog.open()
        }
        function onProgramSelected(scopeName, progName, scopeId, progId) {
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
            host.setCurrentProgram(scopeName, progName, isUserProgram, isRecomProgram, scopeId, progId)
        }
        function onFreeSettingsModeActivated() {
            host.setCurrentProgramTitle(qsTr("СВОБОДНЫЕ УСТАНОВКИ"), "free")
            host.markUnsavedChanges()
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
	//       var rightPanelLeftEdge = rightPanelExpanded ? (host.width - rightPanel.expandedWidth) : (host.width - 85)
	//       var leftPanelRightEdge = leftPanelExpanded ? (host.width / 2) : 85

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
	//             (mouse.x > host.width - 100) ||
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
	//          } else if (!leftPanelExpanded && !rightPanelExpanded && startX > host.width - 100 && deltaX < -threshold) {
	//             rightPanelExpanded = true
	//             mouse.accepted = true
	//          }
	//       }
	//    }

	//    onClicked: {
	//       var deltaX = Math.abs(mouse.x - startX)

	//       // Вычисляем границу правой панели (независимо от анимации)
	//       var rightPanelLeftEdge = rightPanelExpanded ? (host.width - rightPanel.expandedWidth) : (host.width - 85)
	//       var leftPanelRightEdge = leftPanelExpanded ? (host.width / 2) : 85

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
	//       if (!rightPanelExpanded && startX >= host.width - 85) {
	//          // Используем функцию из PedalPanel для определения сокета по клику
	//          var socketIndex = rightPanel.findSocketIndexByClick(mouse.x, mouse.y)

	//          if (socketIndex >= 0) {
	//             rightPanel.lastClickedSocketIndex = socketIndex
	//             rightPanel.openedByPedalClick = true
	//          }

   // }
   
   // Клавиатура только в дереве, когда реально нужна — иначе InputPanel (z:9999) перехватывает тач.
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
         y: host.height
         languageLayout: host.keyboardPrimaryLayout()
         availableLanguageLayouts: host.availableKeyboardLayouts()
         btnTextFontFamily: "DejaVu Sans"
         // Свободное поле цифровой раскладки берёт backgroundColor (по умолчанию чёрный).
         backgroundColor: "#E8ECF2"
         btnBackgroundColor: "#FFFFFF"
         btnSpecialBackgroundColor: "#D0D5DD"
         btnTextColor: "#264093"
         anchors.left: parent.left
         anchors.right: parent.right

         function keyboardFontFamily() {
            return "DejaVu Sans"
         }

         function applyKeyboardColors() {
            InputPanel.backgroundColor = backgroundColor
            InputPanel.btnBackgroundColor = btnBackgroundColor
            InputPanel.btnSpecialBackgroundColor = btnSpecialBackgroundColor
            InputPanel.btnTextColor = btnTextColor
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
            var layouts = host.availableKeyboardLayouts()
            var primary = host.keyboardPrimaryLayout()
            availableLanguageLayouts = layouts
            InputPanel.availableLanguageLayouts = layouts
            languageLayout = primary
            InputPanel.languageLayout = primary
            applyKeyboardColors()
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

         function reassertInputModeLayout() {
            // После пересоздания InputPanel loadLettersLayout() в onCompleted
            // может оставить буквы, если DigitsOnly уже был выставлен и notify не пришёл.
            var mode = InputEngine.inputMode
            var symbols = InputEngine.symbolMode
            InputEngine.inputMode = (mode === InputEngine.DigitsOnly)
                  ? InputEngine.Letters : InputEngine.DigitsOnly
            InputEngine.symbolMode = symbols
            InputEngine.inputMode = mode
         }

         function applyTouchTuning() {
            applyKeyboardColors()
            applyKeyboardFont()
            applyKeyboardUppercase()
            reassertInputModeLayout()
            tuneKeyboardTree(inputPanel)
         }

         onActiveChanged: {
            if (active) {
               syncKeyboardLocales()
               applyKeyboardColors()
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
               y: host.height - inputPanel.height
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

}
