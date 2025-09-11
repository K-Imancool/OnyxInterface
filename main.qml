import QtQuick 2.15
<<<<<<< Updated upstream
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
    color: "darkslategray"

    StatusBar {
        id: statusDummy
        //я искал панграммы для русского и хорошо так посмеялся с эфы
        text: qsTr("В бою с шипящими змеями — эфой и гадюкой — маленький, цепкий, храбрый ёж съел их")
        anchors {
            bottom: socketsDummy.top
            right: parent.right
            left: parent.left
            top: parent.top
            margins: 10
        }
    }

    SocketContainerV2 {
        id: socketsDummy
        innerModel: theModel
        width: 980
        height: 700
        anchors {
            bottom: parent.bottom
            right: parent.right
            margins: 10
        }
    }
    Rectangle {
        id: argonDummy
        radius: 8
        color: "lightgray"
        anchors {
            left: parent.left
            right: socketsDummy.left
            leftMargin: 10
            rightMargin: 10
            bottomMargin: 10
            topMargin: 0
            top: socketsDummy.top
            bottom: neutralDummy.top
        }
        border {
            color: "black"
            width: 1
        }
    }
    Rectangle {
        id: neutralDummy
        height: .25 * socketsDummy.height
        radius: 8
        color: "green"
        anchors {
            left: parent.left
            right: socketsDummy.left
            bottom: parent.bottom
            margins: 10
        }
        border {
            color: "black"
            width: 1
        }
    }

    Drawer {
        id: leftDrawer
        width: 0.8 * container.width
        height: container.height

        // Loader
        // SettingsMain {
        MenuLoader {
            id: menuLoad
            anchors.fill: parent
        }
    }

    Connections {
        target: statusDummy
        function onDrawerCalled() {
            leftDrawer.open()
        }
    }
    Connections {
        target: menuLoad
        function onCloseMe() {
            leftDrawer.close()
        }
    }

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
=======
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Extras 1.4
import QtQuick.CuteKeyboard 1.0

import StratifyLabs.UI 2.0

ApplicationWindow {
    width: 1280
    height: 800
    visible: true

    property int activeIndex: -1

    Rectangle {
            anchors.fill: parent
            color: "white"

            Column {
                anchors.centerIn: parent
                spacing: 20

                Repeater {
                                model: 4
                                delegate: Item {
                                    width: 1155
                                    height: (index === activeIndex ? 300 : 100)

                                    property bool expanded: index === activeIndex

                                    Canvas {
                                        id: canvas
                                        anchors.fill: parent
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.clearRect(0, 0, width, height);

                                            var r = 20;
                                            // Левая половина (жёлтая)
                                            ctx.beginPath();
                                            ctx.moveTo(r, 0);
                                            ctx.lineTo(width/2, 0);
                                            ctx.lineTo(width/2, height);
                                            ctx.lineTo(r, height);
                                            ctx.arcTo(0, height, 0, height - r, r);
                                            ctx.lineTo(0, r);
                                            ctx.arcTo(0, 0, r, 0, r);
                                            ctx.closePath();
                                            ctx.fillStyle = "#FFF82b";
                                            ctx.fill();

                                            // Правая половина (синяя)
                                            ctx.beginPath();
                                            ctx.moveTo(width/2, 0);
                                            ctx.lineTo(width - r, 0);
                                            ctx.arcTo(width, 0, width, r, r);
                                            ctx.lineTo(width, height - r);
                                            ctx.arcTo(width, height, width - r, height, r);
                                            ctx.lineTo(width/2, height);
                                            ctx.closePath();
                                            ctx.fillStyle = "#0B58FF";
                                            ctx.fill();

                                            // Обводка по всему прямоугольнику
                                            ctx.beginPath();
                                            ctx.moveTo(r, 0);
                                            ctx.lineTo(width - r, 0);
                                            ctx.arcTo(width, 0, width, r, r);
                                            ctx.lineTo(width, height - r);
                                            ctx.arcTo(width, height, width - r, height, r);
                                            ctx.lineTo(r, height);
                                            ctx.arcTo(0, height, 0, height - r, r);
                                            ctx.lineTo(0, r);
                                            ctx.arcTo(0, 0, r, 0, r);
                                            ctx.closePath();
                                            ctx.lineWidth = 1;
                                            ctx.strokeStyle = "black";
                                            ctx.stroke();
                                        }
                                    }

                                    // Текст в левой половине
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: 0
                                        width: parent.width / 2
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: expanded ? 36 : 20
                                        font.bold: true
                                        color: "black"
                                        text: expanded ? "Полное Резание" : "Сжато Резание"
                                        elide: Text.ElideRight
                                    }

                                    // Текст в правой половине
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.right: parent.right
                                        anchors.rightMargin: 0
                                        width: parent.width / 2
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: expanded ? 36 : 20
                                        font.bold: true
                                        color: "white"
                                        text: expanded ? "Полное Коагуляция" : "Сжато Коагуляция"
                                        elide: Text.ElideRight
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: activeIndex = index
                                        cursorShape: Qt.PointingHandCursor
                                    }
                                }
                            }
            }
        }
>>>>>>> Stashed changes
}
