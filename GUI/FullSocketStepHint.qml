import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: control
    property string line1: ""
    property string line2: ""
    property color textColor: "#5A6478"
    property int rowHeight: 76

    implicitHeight: rowHeight
    implicitWidth: 170

    Row {
        anchors.centerIn: parent
        spacing: 6

        Text {
            text: qsTr("←")
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 28
            font.bold: true
            color: textColor
        }

        Column {
            spacing: 2
            anchors.verticalCenter: parent.verticalCenter

            Label {
                width: implicitWidth
                anchors.horizontalCenter: parent.horizontalCenter
                text: line1
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 22
                font.bold: true
                color: textColor
            }
            Label {
                width: implicitWidth
                anchors.horizontalCenter: parent.horizontalCenter
                text: line2
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 22
                font.bold: true
                color: textColor
            }
        }

        Text {
            text: qsTr("→")
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 28
            font.bold: true
            color: textColor
        }
    }
}
