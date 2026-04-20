import QtQuick 2.15
import QtQuick.Controls 2.15

Popup {
    id: addTypeSelector

    signal typeChosen(int buttonType)
    parent: Overlay.overlay

    component VariantRect: Rectangle {
        id: someRect

        property int buttonType: 0
        property alias title : titleLabel.text

        width: 250
        height: 100
        radius: 8
        color: "transparent"
        border {
            color: "white"
            width: 1
        }
        Label {
            id: titleLabel
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            font.pixelSize: 20
            font.bold: true
            wrapMode: Text.WordWrap
            color: "white"
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: addTypeSelector.typeChosen(someRect.buttonType)
        }
    }

    signal pageAdditionRequest(int additionType)

    Rectangle {

        // anchors.fill: parent
        anchors.centerIn: parent
        color: "black"

        Row {
            id: buttonRow
            // anchors.fill: parent
            anchors.centerIn: parent

            spacing: 50

            VariantRect{
                title: qsTr("ДУБЛИРОВАТЬ ТЕКУЩИЙ")
                buttonType: 0
            }
            VariantRect{
                title: qsTr("ЗАГРУЗИТЬ РЕКОМЕНДОВАННЫЙ")
                buttonType: 1
            }
            VariantRect{
                title: qsTr("ДОБАВИТЬ ПУСТОЙ")
                buttonType: 2
            }
        }
    }

}
