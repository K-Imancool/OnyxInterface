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

//    Connections {
//        target: statusDummy
//        function onDrawerCalled() {
//            leftDrawer.open()
//        }
//    }
//    Connections {
//        target: menuLoad
//        function onCloseMe() {
//            leftDrawer.close()
//        }
//    }

    // InputPanel {
    //     id: inputPanel
    //     z: 99
    //     y: root.height
    //     availableLanguageLayouts: ["Ru","En"]
    //     anchors.left: parent.left
    //     anchors.right: parent.right
    //     states: State {
    //         name: "visible"
    //         when: Qt.inputMethod.visible
    //         PropertyChanges {
    //             target: inputPanel
    //             y: root.height - inputPanel.height
    //         }
    //     }
    //     transitions: Transition {
    //         from: ""
    //         to: "visible"
    //         reversible: true
    //         ParallelAnimation {
    //             NumberAnimation {
    //                 properties: "y"
    //                 duration: 150
    //                 easing.type: Easing.InOutQuad
    //             }
    //         }
    //     }
    // }
//=======
//import QtQuick.Controls 2.15
//import QtQuick.Window 2.15
//import QtQuick.Extras 1.4
//import QtQuick.CuteKeyboard 1.0

//import StratifyLabs.UI 2.0

//ApplicationWindow {
//    width: 1280
//    height: 800
//    visible: true

//    property int activeIndex: -1

//    Rectangle {
//            anchors.fill: parent
//            color: "white"

//            Column {
//                anchors.centerIn: parent
//                spacing: 20

//                Repeater {
//                                model: 4
//                                delegate: Item {
//                                    width: 1155
//                                    height: (index === activeIndex ? 300 : 100)

//                                    property bool expanded: index === activeIndex

//                                    Canvas {
//                                        id: canvas
//                                        anchors.fill: parent
//                                        onPaint: {
//                                            var ctx = getContext("2d");
//                                            ctx.clearRect(0, 0, width, height);

//                                            var r = 20;
//                                            // Левая половина (жёлтая)
//                                            ctx.beginPath();
//                                            ctx.moveTo(r, 0);
//                                            ctx.lineTo(width/2, 0);
//                                            ctx.lineTo(width/2, height);
//                                            ctx.lineTo(r, height);
//                                            ctx.arcTo(0, height, 0, height - r, r);
//                                            ctx.lineTo(0, r);
//                                            ctx.arcTo(0, 0, r, 0, r);
//                                            ctx.closePath();
//                                            ctx.fillStyle = "#FFF82b";
//                                            ctx.fill();

//                                            // Правая половина (синяя)
//                                            ctx.beginPath();
//                                            ctx.moveTo(width/2, 0);
//                                            ctx.lineTo(width - r, 0);
//                                            ctx.arcTo(width, 0, width, r, r);
//                                            ctx.lineTo(width, height - r);
//                                            ctx.arcTo(width, height, width - r, height, r);
//                                            ctx.lineTo(width/2, height);
//                                            ctx.closePath();
//                                            ctx.fillStyle = "#0B58FF";
//                                            ctx.fill();

//                                            // Обводка по всему прямоугольнику
//                                            ctx.beginPath();
//                                            ctx.moveTo(r, 0);
//                                            ctx.lineTo(width - r, 0);
//                                            ctx.arcTo(width, 0, width, r, r);
//                                            ctx.lineTo(width, height - r);
//                                            ctx.arcTo(width, height, width - r, height, r);
//                                            ctx.lineTo(r, height);
//                                            ctx.arcTo(0, height, 0, height - r, r);
//                                            ctx.lineTo(0, r);
//                                            ctx.arcTo(0, 0, r, 0, r);
//                                            ctx.closePath();
//                                            ctx.lineWidth = 1;
//                                            ctx.strokeStyle = "black";
//                                            ctx.stroke();
//                                        }
//                                    }

//                                    // Текст в левой половине
//                                    Text {
//                                        anchors.verticalCenter: parent.verticalCenter
//                                        anchors.left: parent.left
//                                        anchors.leftMargin: 0
//                                        width: parent.width / 2
//                                        horizontalAlignment: Text.AlignHCenter
//                                        verticalAlignment: Text.AlignVCenter
//                                        font.pixelSize: expanded ? 36 : 20
//                                        font.bold: true
//                                        color: "black"
//                                        text: expanded ? "Полное Резание" : "Сжато Резание"
//                                        elide: Text.ElideRight
//                                    }

//                                    // Текст в правой половине
//                                    Text {
//                                        anchors.verticalCenter: parent.verticalCenter
//                                        anchors.right: parent.right
//                                        anchors.rightMargin: 0
//                                        width: parent.width / 2
//                                        horizontalAlignment: Text.AlignHCenter
//                                        verticalAlignment: Text.AlignVCenter
//                                        font.pixelSize: expanded ? 36 : 20
//                                        font.bold: true
//                                        color: "white"
//                                        text: expanded ? "Полное Коагуляция" : "Сжато Коагуляция"
//                                        elide: Text.ElideRight
//                                    }

//                                    MouseArea {
//                                        anchors.fill: parent
//                                        onClicked: activeIndex = index
//                                        cursorShape: Qt.PointingHandCursor
//                                    }
//                                }
//                            }
//            }
//        }
//>>>>>>> Stashed changes
}
