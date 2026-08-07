import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: pedalRoot

    signal pedalMenuRequest()
    property int pedalStateIdx
    property int socketId
    property real shellSize: Math.min(width, height)

    Connections {
        target: pedalRoot
        function onPedalStateIdxChanged() {
            if (pedalStateIdx == 0) {
                pedalRoot.state = "empty"
            } else if (pedalStateIdx == 1) {
                pedalRoot.state = "single"
            } else if (pedalStateIdx == 2) {
                pedalRoot.state = "double"
            } else if (pedalStateIdx == 3) {
                pedalRoot.state = "handleBi"
            } else if (pedalStateIdx == 4) {
                pedalRoot.state = "monoHandle"
            }
        }
    }
    
    Component.onCompleted: {
        // Инициализируем состояние на основе pedalStateIdx
        if (pedalStateIdx == 0) {
            pedalRoot.state = "empty"
        } else if (pedalStateIdx == 1) {
            pedalRoot.state = "single"
        } else if (pedalStateIdx == 2) {
            pedalRoot.state = "double"
        } else if (pedalStateIdx == 3) {
            pedalRoot.state = "handleBi"
        } else if (pedalStateIdx == 4) {
            pedalRoot.state = "monoHandle"
        }
    }
    color: "transparent"

    state: "empty"
    states: [
        State {
            name: "empty"
            PropertyChanges { target: singleRect; opacity: 0 }
            PropertyChanges { target: doubleRect; opacity: 0 }
            PropertyChanges { target: biHandle; opacity: 0 }
            PropertyChanges { target: monoHandle; opacity: 0 }
        },
        State {
            name: "single"
            PropertyChanges { target: singleRect; opacity: 1 }
            PropertyChanges { target: doubleRect; opacity: 0 }
            PropertyChanges { target: biHandle; opacity: 0 }
            PropertyChanges { target: monoHandle; opacity: 0 }
        },
        State {
            name: "double"
            PropertyChanges { target: singleRect; opacity: 0 }
            PropertyChanges { target: doubleRect; opacity: 1 }
            PropertyChanges { target: biHandle; opacity: 0 }
            PropertyChanges { target: monoHandle; opacity: 0 }
        },
        State {
            name: "handleBi"
            PropertyChanges { target: singleRect; opacity: 0 }
            PropertyChanges { target: doubleRect; opacity: 0 }
            PropertyChanges { target: biHandle; opacity: 1 }
            PropertyChanges { target: monoHandle; opacity: 0 }
        },
        State {
            name: "monoHandle"
            PropertyChanges { target: singleRect; opacity: 0 }
            PropertyChanges { target: doubleRect; opacity: 0 }
            PropertyChanges { target: biHandle; opacity: 0 }
            PropertyChanges { target: monoHandle; opacity: 1 }
        }
    ]
//    transitions: [
//        Transition {
//            from: "*"; to: "*"
//            NumberAnimation { properties: "opacity"; duration: 500; easing.type: Easing.InOutQuad }
//        }
//    ]

    Rectangle {
        id: shell
        width: pedalRoot.shellSize
        height: pedalRoot.shellSize
        anchors.bottom: parent.bottom
        anchors.right: parent.right

        radius: 10
        border.color: "orange"

        color: "grey"

        Rectangle {
            id: singleRect
            anchors {
                fill: parent
            }
            color: "transparent"
            Rectangle {
                color: "blue"
                height: parent.height * .8
                width: 0.3 * parent.width
                anchors.centerIn: parent
                radius: 6
                border.color: "white"
            }
        }
        Rectangle {
            id: doubleRect
            anchors {
                fill: parent
            }
            color: "transparent"
            Rectangle {
                color: "blue"
                height: parent.height * .8
                width: 0.3 * parent.width
                radius: 6
                border.color: "white"
                anchors {
                    left: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                    leftMargin: 6
                }
            }
            Rectangle {
                color: "yellow"
                height: parent.height *.8
                width: 0.3 * parent.width
                radius: 6
                border.color: "white"
                anchors {
                    right: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                    rightMargin: 6
                }
            }
        }
        Rectangle {
            id: biHandle
            anchors {
                fill: parent
                margins: 10
            }
            color: "transparent"
            Rectangle {
                width: parent.height *.8
                height: width
                radius: width/2
                color: "purple"
                border {
                    color: "white"
                    width:2
                }
                anchors.centerIn: parent
            }
        }

        Rectangle {
            id: monoHandle
            anchors {
                fill: parent
                margins: 10
            }
            color: "transparent"
            Rectangle {
                width: parent.width * .42
                height: parent.height * .42
                radius: width/2
                color: "yellow"
                border {
                    width: 2
                    color: "black"
                }
                anchors {
                    top: parent.top
                    left: parent.left
                    margins: parent.width * .085
                }
            }
            Rectangle {
                width: parent.width * .42
                height: parent.height * .42
                radius: width/2
                color: "blue"
                border {
                    width: 2
                    color: "white"
                }
                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    margins: parent.width * .085
                }
            }
        }
        MouseArea {

            anchors.fill: parent
            onPressed: {
//                console.log("initial PedClick")
                pedalRoot.pedalMenuRequest()
            }
        }
    }
}
