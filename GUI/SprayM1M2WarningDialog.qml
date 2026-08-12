import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: root

    signal acceptChosen()
    signal cancelChosen()

    modal: true
    title: ""
    height: 520

    readonly property real overlayWidth: Overlay.overlay ? Overlay.overlay.width : 0
    readonly property real overlayHeight: Overlay.overlay ? Overlay.overlay.height : 0
    width: overlayWidth > 0 ? Math.min(overlayWidth * 0.95, 1100) : 1100

    function showWarning() {
        if (!Overlay.overlay)
            return

        parent = Overlay.overlay
        z = 20001
        x = Math.round((Overlay.overlay.width - width) / 2)
        y = Math.round((Overlay.overlay.height - height) / 2)
        open()
    }

    readonly property string messageText:
        qsTr("При включенной функции М1+М2 режим СПРЕЙ без подачи аргона может активироваться одновременно с выходов МОНО1 и МОНО2 синими кнопками держателей!\nПедали блокируются!\nМощность каждого выхода может изменяться, в зависимости от работы второго выхода, поскольку поддерживается только суммарная мощность!")

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
                text: root.messageText
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 34
            }

            Item { Layout.fillHeight: true }
        }
    }

    footer: Rectangle {
        color: "transparent"
        implicitHeight: 132

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 20
            anchors.bottomMargin: 20
            spacing: 18

            DialogActionButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                text: qsTr("ПРИНЯТЬ")
                primary: true
                primaryEnabledColor: "#C66828"
                primaryBorderWidth: 1
                primaryBorderColor: "#8E0000"
                cornerRadius: 20
                labelPixelSize: 32
                onPressed: {
                    root.acceptChosen()
                    root.close()
                }
            }

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
        }
    }
}
