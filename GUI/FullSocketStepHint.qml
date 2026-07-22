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

    Column {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: line1
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 22
            font.bold: true
            color: textColor
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: line2
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 22
            font.bold: true
            color: textColor
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 14

            Text {
                text: qsTr("←")
                font.pixelSize: 28
                font.bold: true
                color: textColor
            }
            Text {
                text: qsTr("→")
                font.pixelSize: 28
                font.bold: true
                color: textColor
            }
        }
    }
}
