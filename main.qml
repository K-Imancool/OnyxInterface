import QtQuick 2.15
//<<<<<<< Updated upstream
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.CuteKeyboard 1.0
import StratifyLabs.UI 2.0
// import "/home/kikorik/FOTEK/UserInterface/GUI/ScreenOfSockets"

Window {
    id: container
    width: 1280
    height: 800
    visible: true
    title: qsTr("Ты волшебник, Гарри!")
    color: "black"


    // Свойства для управления панелями
    property bool leftPanelExpanded: false
    property bool rightPanelExpanded: false
    
    // Свойство для нейтрального электрода
    property bool neutralConnected: false
    
    // Отладка изменений состояния панелей
    onRightPanelExpandedChanged: {
        console.log("Right panel expanded changed to:", rightPanelExpanded)
    }


//    Drawer {
//        id: leftDrawer
//        width: 0.5 * container.width
//        height: container.height

//        // Loader
//        // SettingsMain {
//        MenuLoader {
//            id: menuLoad
//            anchors.fill: parent
//        }
//    }

    StatusBar {
        id: statusDummy
        //я искал панграммы для русского и хорошо так посмеялся с эфы
        text: qsTr("В бою с шипящими змеями — эфой и гадюкой — маленький, цепкий, храбрый ёж съел их")
        width: parent.width
        height: 85
        anchors {
            top: parent.top
        }
    }

    SocketContainerV2 {
        id: socketsDummy
        innerModel: theModel
        width: parent.width - 170
//        height: parent.height - 85
        z: 1  // Ниже панелей, но выше MouseArea
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            top: statusDummy.bottom
//            leftMargin: 85
//            rightMargin: 85
        }

    }

    // Левая панель - перекрывает центральный контейнер
    Rectangle {
        id: leftPanel
        width: leftPanelExpanded ? container.width / 2 : 85
        height: socketsDummy.height
        anchors.bottom: socketsDummy.bottom
        color: "#2c2c2c"
        x: leftPanelExpanded ? 0 : 0  // ✅ Всегда видима
        z: 10

        // В развернутом состоянии - полные блоки
        Rectangle {
            id: argonDummy
            radius: 8
            color: "lightgray"
            height: 100
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 10
            }
            border {
                color: "black"
                width: 1
            }
            visible: leftPanelExpanded

            Text {
                anchors.centerIn: parent
                text: "Argon"
                font.pixelSize: 16
                color: "black"
            }
        }

        // В свернутом состоянии - маленькие кнопки
        Rectangle {
            id: argonButton
            width: parent.width
            height: 100
            radius: 5
            color: "lightgray"
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                topMargin: 20
            }
            border {
                color: "black"
                width: 1
            }
            visible: !leftPanelExpanded

            Text {
                anchors.centerIn: parent
                text: "A"
                font.pixelSize: 12
                color: "black"
            }
        }

        // NeutralEl компонент
        NeutralEl {
            id: neutralEl
            height: leftPanelExpanded ? 100 : 85
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                margins: leftPanelExpanded ? 10 : 5
            }

            // Передаем параметры
            neutralConnected: container.neutralConnected
            showControls: leftPanelExpanded

            // Обработчики сигналов
            onNeutralTypeChanged: {
                console.log("Neutral type changed to:", newType)
            }

            onNeutralSizeChanged: {
                console.log("Neutral size changed to:", newSize)
            }
        }
    }

    // Правая панель - перекрывает центральный контейнер
    Rectangle {
        id: rightPanel
        width: rightPanelExpanded ? container.width / 2 : 85
        height: socketsDummy.height
        color: "#2c2c2c"
        anchors.bottom: socketsDummy.bottom
        x: container.width - width
        z: 10  // Поверх центрального контейнера
        
        // 4 объекта pedal, привязанные к положению сокетов
        Repeater {
            model: 4
            delegate: Pedal {
                id: pedalDelegate
                property int socketIndex: index

                // Находим целевой сокет и подписываемся на его сигнал
                Component.onCompleted: {
                    // Используем Timer для задержки
                    searchTimer.start()
                }

                // Timer для задержки поиска сокета
                Timer {
                    id: searchTimer
                    interval: 100  // 100 мс задержки
                    repeat: false
                    onTriggered: {
                        findAndConnectToSocket()
                    }
                }

                // Функция для поиска сокета и подключения к сигналу
                function findAndConnectToSocket() {
                    try {
                        var repeater = null
                        
                        // Пробуем разные способы доступа
                        if (socketsDummy.repeat) {
                            repeater = socketsDummy.repeat
                        } else if (socketsDummy.children.length > 0) {
                            var layout = socketsDummy.children[0]
                            
                            // Ищем SocketRepeater в layout
                            for (var j = 0; j < layout.children.length; j++) {
                                var child = layout.children[j]
                                if (child && typeof child.itemAt === 'function') {
                                    repeater = child
                                    break
                                }
                            }
                        }
                        
                        if (!repeater) {
                            retrySearch()
                            return
                        }
                        
                        if (repeater.count > socketIndex) {
                            var socket = repeater.itemAt(socketIndex)
                            if (socket) {
                                // Подписываемся на сигнал
                                socket.absolutePositionChanged.connect(function(socketId, absoluteY) {
                                    if (socketId === socketIndex) {
                                        updatePosition(absoluteY)
                                    }
                                })
                                
                                // Получаем начальную позицию сокета
                                var initialAbsoluteY = socket.mapToItem(null, 0, 0).y
                                updatePosition(initialAbsoluteY)
                            } else {
                                retrySearch()
                            }
                        } else {
                            // Попробуем найти сокет напрямую в layout
                            var layout = socketsDummy.children[0]
                            var socketFound = false
                            
                            for (var k = 0; k < layout.children.length; k++) {
                                var child = layout.children[k]
                                if (child && child.socketId === socketIndex) {
                                    // Подписываемся на сигнал
                                    child.absolutePositionChanged.connect(function(socketId, absoluteY) {
                                        if (socketId === socketIndex) {
                                            updatePosition(absoluteY)
                                        }
                                    })
                                    
                                    // Получаем начальную позицию сокета
                                    var initialAbsoluteY = child.mapToItem(null, 0, 0).y
                                    updatePosition(initialAbsoluteY)
                                    
                                    socketFound = true
                                    break
                                }
                            }
                            
                            if (!socketFound) {
                                retrySearch()
                            }
                        }
                    } catch (error) {
                        retrySearch()
                    }
                }

                // Функция для повторной попытки поиска
                function retrySearch() {
                    Qt.callLater(function() {
                        findAndConnectToSocket()
                    })
                }

                // Функция для обновления позиции педали
                function updatePosition(absoluteY) {
                    try {
                        var panelAbsoluteY = rightPanel.mapToItem(null, 0, 0).y
                        pedalDelegate.y = absoluteY - panelAbsoluteY
                    } catch (error) {
                        // Ошибка при обновлении позиции педали
                    }
                }

                // Размеры педали
                width: 60
                height: 60
                anchors.right: parent.right
                anchors.rightMargin: 10

                // Стилизация
                color: "darkgray"
                border.color: "white"
                border.width: 2
                radius: 5

                // Текст с номером педали
                Text {
                    anchors.centerIn: parent
                    text: (socketIndex + 1).toString()
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }
            }
        }
    }

    // Область для свайпов и закрытия панелей
    MouseArea {
        id: swipeArea
        anchors.fill: parent
        z: leftPanelExpanded || rightPanelExpanded ? 2 : 0  // Выше сокетов при открытых панелях

        property real startX: 0
        property bool isSwipeGesture: false
        property real startTime: 0

        onPressed: {
            startX = mouse.x
            startTime = Date.now()
            isSwipeGesture = false

            // Проверяем области для свайпа:
            // - Левая область для открытия левой панели
            // - Правая область для открытия правой панели  
            // - Любое место, если панель уже открыта
            if ((mouse.x < 100) ||
               (mouse.x > container.width - 100) ||
               (leftPanelExpanded && (mouse.x < container.width / 2)) ||
               (rightPanelExpanded && (mouse.x > container.width / 2))) {
                isSwipeGesture = true
            }
        }

        onReleased: {
            if (!isSwipeGesture) {
                return
            }

            var deltaX = mouse.x - startX
            var threshold = 50 // Уменьшил порог для лучшей чувствительности
            var swipeThreshold = Math.abs(deltaX)

            // Длинное нажатие определяем как свайп
            if (swipeThreshold > threshold) {
                // Закрытие панелей имеет приоритет
                if (leftPanelExpanded && deltaX < -threshold) {
                    leftPanelExpanded = false
                } else if (rightPanelExpanded && deltaX > threshold) {
                    rightPanelExpanded = false  
                }
                // Открытие панелей
                else if (!leftPanelExpanded && !rightPanelExpanded && startX < 100 && deltaX > threshold) {
                    leftPanelExpanded = true
                } else if (!leftPanelExpanded && !rightPanelExpanded && startX > container.width - 100 && deltaX < -threshold) {
                    rightPanelExpanded = true
                }
            }
        }

        onClicked: {
            var currentTime = Date.now()
            var gestureDuration = currentTime - startTime

            // Игнорируем клики, которые являются частью свайпа (быстрые и с большим смещением)
            if (gestureDuration < 300 && Math.abs(mouse.x - startX) > 50) {
                return
            }
            
            // Закрываем панели при клике вне их области
            if (leftPanelExpanded && mouse.x > leftPanel.width) {
                leftPanelExpanded = false
            }
            if (rightPanelExpanded && mouse.x < rightPanel.x) {
                rightPanelExpanded = false
            }
        }
        
    }
}
