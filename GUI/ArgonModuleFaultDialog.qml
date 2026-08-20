import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: root

    signal acceptChosen()
    signal cancelChosen()

    modal: true
    title: ""
    closePolicy: Popup.NoAutoClose
    height: 420

    readonly property real overlayWidth: Overlay.overlay ? Overlay.overlay.width : 0
    readonly property real overlayHeight: Overlay.overlay ? Overlay.overlay.height : 0
    width: overlayWidth > 0 ? Math.min(overlayWidth * 0.92, 980) : 980

    function showWarning() {
        if (!Overlay.overlay)
            return

        parent = Overlay.overlay
        z = 2000000
        x = Math.round((Overlay.overlay.width - width) / 2)
        y = Math.round((Overlay.overlay.height - height) / 2)
        open()
    }

    readonly property string messageText:
        qsTr("Неисправность газового модуля. Продолжить работу без него? Режимы с аргоном будут недоступны.")

    contentItem: Rectangle {
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            anchors.topMargin: 26
            anchors.bottomMargin: 14
            spacing: 18

            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: root.messageText
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 34
            }
        }
    }

    footer: Rectangle {
        color: "transparent"
        implicitHeight: 120

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 16
            anchors.bottomMargin: 20
            spacing: 18

            DialogActionButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                text: qsTr("ОТМЕНА")
                secondaryColor: "white"
                secondaryBorderWidth: 2
                secondaryBorderColor: "#264093"
                cornerRadius: 20
                labelPixelSize: 32
                labelColor: "#264093"
                onPressed: {
                    root.cancelChosen()
                    root.close()
                }
            }

            DialogActionButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                text: qsTr("ПРОДОЛЖИТЬ")
                primary: true
                cornerRadius: 20
                labelPixelSize: 32
                onPressed: {
                    root.acceptChosen()
                    root.close()
                }
            }
        }
    }
}
