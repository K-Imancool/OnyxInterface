import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Item {
    id: transferMenuRoot

    signal returnButtonPressed()
    signal userProgDownloadButtonPressed()
    signal userProgUploadButtonPressed()

    property color fotekBlue: "#264093"
    property color fotekOrange: "#faa731"
    readonly property string iconsBasePath: AppPaths.iconsBaseUrl
    readonly property int screenMargin: 34
    readonly property int mainSpacing: 20
    readonly property int headerHeight: 70
    readonly property int menuActionLabelSize: 38
    readonly property int menuActionIconSize: 85
    readonly property int panelRadius: 20

    Rectangle {
        anchors.fill: parent
        color: "#8A929E"
    }

    Item {
        id: headerArea
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: transferMenuRoot.screenMargin - 8
        }
        width: parent.width - transferMenuRoot.screenMargin * 2
        height: transferMenuRoot.headerHeight

        Text {
            text: qsTr("ПЕРЕНОС ПРОГРАММ ПОЛЬЗОВАТЕЛЯ")
            anchors.centerIn: parent
            color: transferMenuRoot.fotekBlue
            font.pixelSize: 36
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width
            lineHeight: 1.05
        }
    }

    Item {
        id: footerArea
        height: 72
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: transferMenuRoot.screenMargin
            rightMargin: transferMenuRoot.screenMargin
            bottomMargin: transferMenuRoot.screenMargin
        }

        DialogActionButton {
            width: 180
            height: parent.height
            text: qsTr("НАЗАД")
            secondaryColor: transferMenuRoot.fotekBlue
            secondaryBorderWidth: 1
            secondaryBorderColor: "#1E3274"
            cornerRadius: transferMenuRoot.panelRadius
            labelPixelSize: 30
            onPressed: transferMenuRoot.returnButtonPressed()
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
        }
    }

    ColumnLayout {
        anchors {
            top: headerArea.bottom
            topMargin: 22
            left: parent.left
            right: parent.right
            bottom: footerArea.top
            bottomMargin: transferMenuRoot.mainSpacing
            leftMargin: transferMenuRoot.screenMargin
            rightMargin: transferMenuRoot.screenMargin
        }
        spacing: transferMenuRoot.mainSpacing

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: hintPanel.height

            Rectangle {
                id: hintPanel
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width * 0.9, hintText.implicitWidth + 48)
                height: hintText.height + 36
                radius: transferMenuRoot.panelRadius
                color: "#80FFFFFF"
                border.width: 1
                border.color: "#C5CAD3"

                Text {
                    id: hintText
                    anchors.centerIn: parent
                    width: parent.width - 32
                    text: qsTr("При необходимости перенести Ваши пользовательские программы с одного аппарата ONYX-(A)M на другой, используйте устройство с возможностью подключения к сети WiFi, например, смартфон")
                    color: "black"
                    font.pixelSize: 28
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    lineHeight: 1.15
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: transferMenuRoot.mainSpacing

            MenuActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("СКАЧАТЬ\nС АППАРАТА\nНА УСТРОЙСТВО")
                iconSource: transferMenuRoot.iconsBasePath + "iconExport.png"
                iconSize: transferMenuRoot.menuActionIconSize
                iconCenter: false
                accentColor: transferMenuRoot.fotekOrange
                textColor: transferMenuRoot.fotekBlue
                labelPixelSize: transferMenuRoot.menuActionLabelSize
                maxLabelLines: 3
                cornerRadius: transferMenuRoot.panelRadius
                onPressed: transferMenuRoot.userProgDownloadButtonPressed()
            }

            MenuActionButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: qsTr("ЗАГРУЗИТЬ\nС УСТРОЙСТВА\nНА АППАРАТ")
                iconSource: transferMenuRoot.iconsBasePath + "iconImport.png"
                iconSize: transferMenuRoot.menuActionIconSize
                iconCenter: false
                accentColor: transferMenuRoot.fotekOrange
                textColor: transferMenuRoot.fotekBlue
                labelPixelSize: transferMenuRoot.menuActionLabelSize
                maxLabelLines: 3
                cornerRadius: transferMenuRoot.panelRadius
                onPressed: transferMenuRoot.userProgUploadButtonPressed()
            }
        }
    }
}
