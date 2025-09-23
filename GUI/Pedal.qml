import QtQuick 2.15

Rectangle {
    id: pedalRoot

    signal pedalMenuRequest()

    radius: 8
    color: "darkgray"
    border {
        width: 2
        color: "lightgray"
    }
    state: "empty"
    states: [
        State {
            name: "empty"
            PropertyChanges { target: singleRect; opacity: 0 }
            PropertyChanges { target: doubleRect; opacity: 0 }
            PropertyChanges { target: biHandle; opacity: 0 }
        },
        State {
            name: "single"
            PropertyChanges { target: singleRect; opacity: 1 }
            PropertyChanges { target: doubleRect; opacity: 0 }
            PropertyChanges { target: biHandle; opacity: 0 }
        },
        State {
            name: "double"
            PropertyChanges { target: singleRect; opacity: 0 }
            PropertyChanges { target: doubleRect; opacity: 1 }
            PropertyChanges { target: biHandle; opacity: 0 }
        },
        State {
            name: "handleBi"
            PropertyChanges { target: singleRect; opacity: 0 }
            PropertyChanges { target: doubleRect; opacity: 0 }
            PropertyChanges { target: biHandle; opacity: 1 }
        }
    ]
    transitions: [
        Transition {
            from: "*"; to: "*"
            NumberAnimation { properties: "opacity"; duration: 500; easing.type: Easing.InOutQuad }
        }
    ]
    Rectangle {
        id: singleRect
        anchors {
            fill: parent
            margins: 15
        }
        Rectangle {
            color: "blue"
            height: parent.height
            width: 0.3 * parent.width
            anchors.centerIn: parent
            radius: 6
        }
    }
    Rectangle {
        id: doubleRect
        anchors {
            fill: parent
            margins: 15
        }
        Rectangle {
            color: "blue"
            height: parent.height
            width: 0.3 * parent.width
            radius: 6
            anchors {
                left: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
                leftMargin: 6
            }
        }
        Rectangle {
            color: "yellow"
            height: parent.height
            width: 0.3 * parent.width
            radius: 6
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
        Rectangle {
            width: parent.height
            height: width
            radius: width
            color: "purple"
            border {
                color: "white"
                width:2
            }
            anchors.centerIn: parent
        }
    }
    MouseArea {
        id: pressHandle
        anchors.fill: parent
    }
    Connections {
        target: pressHandle
        function onClicked () {
            pedalRoot.pedalMenuRequest()
        }
    }

}
