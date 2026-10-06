import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: root

    signal cancelChosen()
    signal confirmChosen()

    property color accentColor: "#264093"
    property int secondsRemaining: 10

    parent: Overlay.overlay
    modal: true
    closePolicy: Popup.NoAutoClose
    z: 3000000
    width: parent ? Math.min(parent.width * 0.82, 900) : 900
    height: 360
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    function openWithTimeout(seconds) {
        secondsRemaining = seconds
        countdownTimer.restart()
        open()
    }

    function cancel() {
        countdownTimer.stop()
        close()
        cancelChosen()
    }

    function confirm() {
        countdownTimer.stop()
        close()
        confirmChosen()
    }

    background: Rectangle {
        color: "#f5f5f5"
        border.color: root.accentColor
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
            text: qsTr("Питание будет выключено через %1 секунд").arg(root.secondsRemaining)
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
                onPressed: root.cancel()
            }

            Item { Layout.fillWidth: true }

            DialogActionButton {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                text: qsTr("ВЫКЛЮЧИТЬ")
                primary: true
                primaryEnabledColor: "#B71C1C"
                labelPixelSize: 32
                onPressed: root.confirm()
            }
        }
    }

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (root.secondsRemaining <= 1) {
                root.confirm()
            } else {
                root.secondsRemaining--
            }
        }
    }
}
