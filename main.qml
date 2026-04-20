import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.CuteKeyboard 1.0
import StratifyLabs.UI 2.0
import BackEnd 1.0

// О-хо-хо, будем сливать

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
    readonly property int argNeutralPanelWidth: 100

    // Свойство для нейтрального электрода
    property bool neutralConnected: false
    
    // Текущее название программы
    property string currentProgName: ""

    function activationEnable() {
        periphHandle.enableActivation = !(pedDrawer.opened
                                          | leftDrawer.opened
                                          | argNeutDrawer.opened
                                          | socketsDummy.socketEditorOpened)
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

        case 0x90: return qsTr("Критичная ошибка: ИСН при включении")
        case 0x91: return qsTr("Критичная ошибка: АЦП1 (напряжение контура)")
        case 0x92: return qsTr("Критичная ошибка: АЦП2 (ток контура)")
        case 0x93: return qsTr("Критичная ошибка: АЦП3 (ток генератора)")
        case 0x94: return qsTr("Критичная ошибка: АЦП4 (напряжение ИСН)")
        case 0x95: return qsTr("Критичная ошибка: реле")
        case 0x96: return qsTr("Критичная ошибка: ИСН при нормальной работе")
        case 0x97: return qsTr("Критичная ошибка: не найден резонанс при калибровке НЭ")
        case 0x98: return qsTr("Критичная ошибка: АЦП схемы НЭ")

        case 32: return qsTr("Ошибка UART: передача не выполнена")
        case 33: return qsTr("Ошибка UART: нет ответа")
        case 34: return qsTr("Ошибка UART: неверный ответ")
        case 35: return qsTr("Ошибка UART: неверная длина пакета")
        case 36: return qsTr("Ошибка UART: CRC не совпадает")
        default:
            return qsTr("Ошибка устройства (код 0x") + code.toString(16).toUpperCase() + ")"
        }
    }

   StatusBar {
      id: statusDummy
      //я искал панграммы для русского и хорошо так посмеялся с эфы
      text: qsTr("")
      versionText: qsTr("Текущая версия: ") + appVersion
      width: parent.width
      height: 85
      anchors {
         top: parent.top
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
        anchors {
            left: parent.left
            bottom: parent.bottom
            top: statusDummy.bottom
        }
    }
    Connections {
        target: argNeutralPanel
        function onOpenPeriphDrawer() {
            argNeutDrawer.open()
        }
    }
    PeripheryDrawer {
        id: argNeutDrawer
        width: .5 * container.width
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
		width: .5 * container.width
		height: container.height
		edge: Qt.RightEdge
	}

    // Глобальный отладочный индикатор тача временно отключён,
    // чтобы гарантированно не влиять на обработку событий в приложении.

    Item {
        id: leftDrawer
        width: container.width
        height: container.height
        x: -container.width
        y: 0
        z: 9998
        visible: opened || slideMenuAnimation.running

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

        // Блокируем прохождение событий к элементам за панелью.
        // z: -1 гарантирует, что область всегда ниже контента MenuLoader (z:0)
        // и получает только те события, которые не были приняты кнопками.
        MouseArea {
            anchors.fill: parent
            z: -1
            onPressed: mouse.accepted = true
            onReleased: mouse.accepted = true
            onPositionChanged: mouse.accepted = true
        }

        MenuLoader {
            id: menuLoad
            anchors.fill: parent
        }
    }

	ProgSaveDialog {
		id: saveProgDialog
		anchors.centerIn: parent
		width: 0.8 * parent.width
		height: 350

    }
    Connections {
        target: saveProgDialog
        function onAccepted() {
            recomHandle.saveProg(saveProgDialog.scopeName,
                                 saveProgDialog.progName)
            statusDummy.text = saveProgDialog.scopeName + ": " + saveProgDialog.progName
            container.currentProgName = saveProgDialog.progName
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
        target: argNeutDrawer
        function onOpenedChanged() {
            container.activationEnable()
        }
    }
    Connections {
        target: socketsDummy
        function onSocketEditorOpenedChanged() {
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
        function onOpenPeriphDrawer() {
            argNeutDrawer.open()
        }
    }

    Item {
        id: drawerOverlay
        anchors.fill: parent
        z: 1  // Выше основного контента, но drawer'ы будут иметь z намного выше (по умолчанию 10000)
        visible: argNeutDrawer.opened || pedDrawer.opened

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
                    if (argNeutDrawer.opened) argNeutDrawer.close()
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
					argNeutDrawer.open()
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
    }
    Connections {
        target: menuLoad
        function onCloseMe() {
            leftDrawer.close()
        }
        function onProgramSelected(scopeName, progName) {
            statusDummy.text = scopeName + ": " + progName
            container.currentProgName = progName
        }
        function onFreeSettingsModeActivated() {
            statusDummy.text = "Свободные установки"
            container.currentProgName = "Свободные установки"
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

	// Монитор системы в правом нижнем углу
	SystemMonitor {
		id: systemMonitor
		anchors {
			right: parent.right
			bottom: parent.bottom
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
   
   // Виртуальная клавиатура CuteKeyboard
   InputPanel {
      id: inputPanel
     
      function tuneKeyboardTree(node) {
         if (!node)
            return
         if (node.autoRepeat !== undefined) {
            node.autoRepeat = false
         }
         if (node.alternativeKeys !== undefined) {
            node.alternativeKeys = []
         }
         if (!node.children)
            return
         for (var i = 0; i < node.children.length; ++i) {
            tuneKeyboardTree(node.children[i])
         }
      }

      function applyTouchTuning() {
         tuneKeyboardTree(inputPanel)
      }
      
      z: 9999
      y: container.height
      availableLanguageLayouts: ["Ru","En"]
      anchors.left: parent.left
      anchors.right: parent.right

      onActiveChanged: {
         if (active) {
            keyboardTuningTimer.restart()
         }
      }

      onLanguageLayoutChanged: {
         keyboardTuningTimer.restart()
      }

      Timer {
         id: keyboardTuningTimer
         interval: 40
         repeat: false
         onTriggered: {
            inputPanel.applyTouchTuning()
            Qt.callLater(inputPanel.applyTouchTuning)
         }
      }

      states: State {
         name: "visible"
         when: Qt.inputMethod.visible
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

    //       // Центральная область или внутри панели - пропускаем к дочерним элементам
    //       mouse.accepted = false
    //    }

	// }

}
