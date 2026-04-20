import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: addTypeSelector

    signal progLoaderSelected(int buttonType)
    parent: Overlay.overlay
    anchors.centerIn: parent

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
            anchors.fill: parent
            anchors.centerIn: parent
            wrapMode: Text.WordWrap
            color: "white"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                addTypeSelector.progLoaderSelected(someRect.buttonType)
                addTypeSelector.close()
            }
        }
    }
    header: Rectangle {
        anchors.top: parent.top
        height: 50
        color: "transparent"
        Label {
            id: titleText
            anchors.fill: parent
            text: qsTr("Добавить новый \"Лист\" программы")
            font.bold: true
            font.pixelSize: 36
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }
    }

    contentItem: Rectangle {
        anchors.top: header.bottom
        anchors.topMargin: 25
        color: "transparent"
        width: parent.width
        height: 200
        Rectangle {
            id: upper
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 200
            color: "transparent"
            RowLayout {
                id: buttonRow
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                Layout.fillWidth: true
                Layout.fillHeight: true

                spacing: 20
                VariantRect{
                    title: qsTr("ДУБЛИРОВАТЬ ТЕКУЩИЙ")
                    buttonType: 0
                    Layout.alignment: Qt.AlignCenter
                }
                VariantRect{
                    title: qsTr("ЗАГРУЗИТЬ РЕКОМЕНДОВАННЫЙ")
                    buttonType: 1
                    Layout.alignment: Qt.AlignCenter
                }
                VariantRect{
                    title: qsTr("ЗАГРУЗИТЬ ПОЛЬЗОВАТЕЛЬСКИЙ")
                    buttonType: 3
                    Layout.alignment: Qt.AlignCenter
                }
                VariantRect{
                    title: qsTr("ДОБАВИТЬ ПУСТОЙ")
                    buttonType: 2
                    Layout.alignment: Qt.AlignCenter
                }
            }
        }
    }
}
