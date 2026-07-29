import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Dialog {
    id: addTypeSelector

    signal progLoaderSelected(int buttonType)
    parent: Overlay.overlay
    anchors.centerIn: parent
    readonly property string iconsBasePath: AppPaths.iconsBaseUrl

    component VariantRect: Rectangle {
        id: someRect
        property int buttonType: 0
        property alias title : titleLabel.text
        property alias iconSource: icon.source

        width: 380
        height: 250
        radius: 8
        color: "transparent"
        border {
            color: "grey"
            width: 1
        }
        Column {
            anchors.centerIn: parent
            width: parent.width - 24
            spacing: 12

            Label {
                id: titleLabel
                width: parent.width
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                font.pixelSize: 26
                font.bold: true
                wrapMode: Text.WordWrap
                color: "black"
            }

            Image {
                id: icon
                anchors.horizontalCenter: parent.horizontalCenter
//                width:
                height: 130
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
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
        height: 300
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
                    title: qsTr("ДУБЛИРОВАТЬ\nТЕКУЩИЙ")
                    iconSource: addTypeSelector.iconsBasePath + "listDub.png"
                    buttonType: 0
                    Layout.alignment: Qt.AlignCenter
                }
                VariantRect{
                    title: qsTr("ВЫБРАТЬ ИЗ\nРЕКОМЕНДОВАННЫХ")
                    iconSource: addTypeSelector.iconsBasePath + "listRecom.png"
                    buttonType: 1
                    Layout.alignment: Qt.AlignCenter
                }
//                VariantRect{
//                    title: qsTr("ЗАГРУЗИТЬ ПОЛЬЗОВАТЕЛЬСКИЙ")
//                    buttonType: 3
//                    Layout.alignment: Qt.AlignCenter
//                }
                VariantRect{
                    title: qsTr("ДОБАВИТЬ\nПУСТОЙ")
                    iconSource: addTypeSelector.iconsBasePath + "listFree.png"
                    buttonType: 2
                    Layout.alignment: Qt.AlignCenter
                }
            }
        }
    }
}
