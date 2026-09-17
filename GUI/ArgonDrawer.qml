import QtQuick 2.15
import QtQuick.Controls 2.15

import BackEnd 1.0

Drawer {
    id: argonDrawerRoot

    interactive: false
    modal: false
    closePolicy: Popup.NoAutoClose

    background: Rectangle {
        color: "white"
    }

    Rectangle {
        anchors.fill: parent
        color: "white"
        z: -1

        MouseArea {
            anchors.fill: parent
            onPressed: mouse.accepted = true
            onReleased: mouse.accepted = true
            onClicked: mouse.accepted = true
            onPositionChanged: mouse.accepted = true
            onDoubleClicked: mouse.accepted = true
        }
    }

    Rectangle {
        id: wrapRect
        anchors.fill: parent
        color: "transparent"

        Label {
            id: argonChoice
            anchors {
                top: parent.top
                topMargin: 15
                horizontalCenter: parent.horizontalCenter
            }
            horizontalAlignment: Qt.AlignHCenter
            text: qsTr("НАСТРОЙКА ГАЗОВОГО ТРАКТА")
            color: fotekBlue
            font.pixelSize: 30
            font.bold: true
        }

        Argon {
            id: argonView
            anchors {
                top: argonChoice.bottom
                left: parent.left
                right: parent.right
                bottom: parent.bottom
                topMargin: 10
                leftMargin: 5
                rightMargin: 5
                bottomMargin: 10
            }
            showControls: true
            cylinder1Connected: periphHandle.argonCylinder1Connected
            cylinder2Connected: periphHandle.argonCylinder2Connected
            flowRate: periphHandle.argonFlowRate
            realFlowRate: periphHandle.argonRealRate
            isActivation: periphHandle.activation
            isBlowing: periphHandle.argonBlowing
            activCylinderFirst: periphHandle.activCylinderFirst
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 1000
        enabled: argonDrawerRoot.opened
        propagateComposedEvents: true

        property real startX: 0
        property real startY: 0
        property bool isSwipeGesture: false
        readonly property real minSwipeDistance: 50

        onPressed: function(mouse) {
            startX = mouse.x
            startY = mouse.y
            isSwipeGesture = false

            var item = wrapRect.childAt(mouse.x - wrapRect.x, mouse.y - wrapRect.y)
            if (item && item.hasInteractiveContent === true) {
                mouse.accepted = false
                return
            }
            mouse.accepted = true
        }

        onPositionChanged: function(mouse) {
            if (!pressed)
                return
            var deltaX = mouse.x - startX
            var deltaY = Math.abs(mouse.y - startY)
            if (deltaX < -50 && Math.abs(deltaX) > deltaY * 2 && !isSwipeGesture)
                isSwipeGesture = true
        }

        onReleased: function(mouse) {
            var deltaX = mouse.x - startX
            if (isSwipeGesture && deltaX < -minSwipeDistance)
                argonDrawerRoot.close()
            isSwipeGesture = false
        }
    }

    Button {
        id: closeButton
        anchors {
            top: parent.top
            topMargin: 10
            right: parent.right
            rightMargin: 10
        }
        width: 68
        height: 68
        z: 1001
        onPressed: argonDrawerRoot.close()

        background: Rectangle {
            color: "transparent"
        }

        contentItem: Text {
            text: qsTr("X")
            font.pixelSize: 32
            font.bold: true
            color: "black"
//            color: fotekBlue
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            visible: false          // сделал невидимой, а то плохо смотрится
        }
    }

    Connections {
        target: argonView
        function onFlowRateUpdated(newRate) {
            periphHandle.argonFlowRate = newRate
        }
        function onArgonBlow() {
            periphHandle.argonBlow()
        }
        function onActivCylinderToggled(first) {
            periphHandle.activCylinderFirst = first
        }
    }
}
