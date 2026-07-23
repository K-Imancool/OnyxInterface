import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: root

    property string socketName: ""
    property int maxNeutralPower: 50

    signal continueChosen()
    signal reduceChosen()

    modal: true
    title: ""
    height: 480

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

    function maxPowerForNeutralSize(neutralSize) {
        switch (neutralSize) {
        case 0:
            return 50
        case 1:
            return 75
        default:
            return 400
        }
    }

    readonly property string messageText:
        qsTr("Опасность перегрева области наложения нейтрального электрода (НЭ)! Мощность выхода %1 превышает безопасный уровень для выбранного НЭ (макс %2).\nПРОДОЛЖИТЬ с текущим уровнем мощности?\nИЛИ\nПОНИЗИТЬ мощность до безопасного уровня?")
            .arg(socketName)
            .arg(maxNeutralPower)

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
                text: qsTr("ПРОДОЛЖИТЬ")
                primary: true
                primaryEnabledColor: "#C62828"
                primaryBorderWidth: 1
                primaryBorderColor: "#8E0000"
                cornerRadius: 20
                labelPixelSize: 32
                onPressed: {
                    // Сигнал до close(): иначе onClosed у родителя может очистить
                    // состояние (например pendingPowerViolations) до обработчика.
                    root.continueChosen()
                    root.close()
                }
            }

            DialogActionButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                text: qsTr("ПОНИЗИТЬ")
                primary: true
                cornerRadius: 20
                labelPixelSize: 32
                onPressed: {
                    root.reduceChosen()
                    root.close()
                }
            }
        }
    }
}
