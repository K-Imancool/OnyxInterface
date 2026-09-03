import QtQuick 2.15
import QtQuick.Controls 2.15
import BackEnd 1.0

Popup {
    id: activationPopup

    property string socketName
    property string modeName
    property int power: 0
    property bool isCoag: false
    property bool isEndo: false
    property int activeSocketId: -1
    readonly property int autoModeAcc: 2

    readonly property color foregroundColor: isCoag ? "white" : "black"
    readonly property color foregroundOutlineColor: isCoag ? "black" : "white"

    modal: true
    dim: false
    closePolicy: Popup.NoAutoClose
    padding: 0
    Overlay.modal: Item {}

    function stopIfAccTouch() {
        if (activeSocketId >= 0 && periphHandle.autoMode(activeSocketId) === autoModeAcc)
            appControl.stopActivation()
    }

    onClosed: {
        activeSocketId = -1
    }

    // Popup рисуется в Overlay поверх WorkScreen; без этого касание вне плашки
    // глотается модальным слоем и не останавливает АСС.
    MouseArea {
        parent: Overlay.overlay
        anchors.fill: parent
        z: 1000000
        enabled: activationPopup.opened
        visible: activationPopup.opened
        propagateComposedEvents: false
        preventStealing: true

        onPressed: function(mouse) {
            activationPopup.stopIfAccTouch()
            mouse.accepted = true
        }
        onReleased: function(mouse) { mouse.accepted = true }
        onClicked: function(mouse) { mouse.accepted = true }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: false
        preventStealing: true

        onPressed: function(mouse) {
            activationPopup.stopIfAccTouch()
            mouse.accepted = true
        }

        onReleased: function(mouse) {
            mouse.accepted = true
        }
    }

    background: Rectangle {
        color: isCoag ? "#0f58fa" : "#ffd900"
        radius: 20
        border.color: "white"
        border.width: 3
        opacity: 1.0
    }

    contentItem: Item {
        Column {
            anchors.centerIn: parent
            spacing: 32

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("АКТИВАЦИЯ ") + socketName
                font.pixelSize: 64
                font.bold: true
                color: foregroundColor
                style: Text.Outline
                styleColor: foregroundOutlineColor
            }

            Rectangle {
                width: 560
                height: 4
                color: foregroundColor
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Label {
                id: modeNameLabel
                anchors.horizontalCenter: parent.horizontalCenter
                text: modeName
                font.pixelSize: 96
                color: foregroundColor
                style: Text.Outline
                styleColor: foregroundOutlineColor
            }

            Rectangle {
                id: powerInfoRect
                anchors.horizontalCenter: parent.horizontalCenter
                width: 680
                height: 280
                color: "transparent"

                Column {
                    visible: !activationPopup.isEndo
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width
                    spacing: 16
                    Label {
                        width: parent.width
                        font.pixelSize: 56
                        font.bold: true
                        color: foregroundColor
                        style: Text.Outline
                        styleColor: foregroundOutlineColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Мощность")
                    }
                    Label {
                        width: parent.width
                        height: parent.height - parent.children[0].height - parent.spacing
                        font.pixelSize: 140
                        font.bold: true
                        color: foregroundColor
                        style: Text.Outline
                        styleColor: foregroundOutlineColor
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: activationPopup.power
                    }
                }

                Column {
                    id: endoCutColumn
                    visible: activationPopup.isEndo
                    anchors.left: parent.left
                    anchors.right: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    spacing: 16
                    Label {
                        width: parent.width
                        font.pixelSize: 56
                        font.bold: true
                        color: foregroundColor
                        style: Text.Outline
                        styleColor: foregroundOutlineColor
                        text: qsTr("Эффект\nрезания")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    Label {
                        width: parent.width
                        height: parent.height - parent.children[0].height - parent.spacing
                        font.pixelSize: 140
                        font.bold: true
                        color: foregroundColor
                        style: Text.Outline
                        styleColor: foregroundOutlineColor
                        text: Math.floor(activationPopup.power / 10)
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Column {
                    id: endoCoagColumn
                    visible: activationPopup.isEndo
                    anchors.left: parent.horizontalCenter
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    spacing: 16
                    Label {
                        width: parent.width
                        font.pixelSize: 56
                        font.bold: true
                        color: foregroundColor
                        style: Text.Outline
                        styleColor: foregroundOutlineColor
                        text: qsTr("Эффект\nкоагуляции")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    Label {
                        width: parent.width
                        height: parent.height - parent.children[0].height - parent.spacing
                        font.pixelSize: 140
                        font.bold: true
                        color: foregroundColor
                        style: Text.Outline
                        styleColor: foregroundOutlineColor
                        text: activationPopup.power % 10
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Rectangle {
            id: pulseBorder
            anchors.fill: parent
            anchors.margins: 10
            color: "transparent"
            border.color: "white"
            border.width: 3
            radius: 15
            opacity: 0.7

            SequentialAnimation {
                running: activationPopup.opened
                loops: Animation.Infinite

                OpacityAnimator {
                    target: pulseBorder
                    from: 0.35
                    to: 1.0
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
                OpacityAnimator {
                    target: pulseBorder
                    from: 1.0
                    to: 0.35
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 50
        }
        NumberAnimation {
            property: "scale"
            from: 0.9
            to: 1.0
            duration: 50
            easing.type: Easing.OutQuad
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 50
        }
        NumberAnimation {
            property: "scale"
            from: 1.0
            to: 0.9
            duration: 50
            easing.type: Easing.InQuad
        }
    }
}
