import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    property string iconSource: ""
    property color accentColor: "#faa731"
    property color textColor: "#264093"
    property int labelPixelSize: 36
    property int cornerRadius: 28
    property int iconSize: 88
    property int maxLabelLines: 2
    property bool labelCentered: false
    property bool iconCenter: true

    padding: 0
    topPadding: 10
    bottomPadding: 10
    leftPadding: 16
    rightPadding: 12

    background: Rectangle {
        radius: control.cornerRadius
        color: "white"
        border.width: 2
        border.color: control.accentColor
    }

    contentItem: Item {
        Text {
            id: actionLabel
            anchors {
                left: parent.left
                right: control.labelCentered || control.iconSize <= 0 ? parent.right : actionIcon.left
                verticalCenter: parent.verticalCenter
                leftMargin: control.labelCentered ? 16 : 0
                rightMargin: control.labelCentered ? 20 : 12
            }
            text: control.text
            color: control.textColor
            font.pixelSize: control.labelPixelSize
            font.bold: true
            horizontalAlignment: control.labelCentered ? Text.AlignHCenter : Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
            maximumLineCount: control.maxLabelLines
            lineHeight: 1.04
            elide: Text.ElideRight
        }

        Image {
            id: actionIcon
            visible: control.iconSize > 0 && control.iconSource.length > 0
            anchors {
                right: parent.right
                verticalCenter: control.iconCenter ? parent.verticalCenter : undefined
                top: control.iconCenter ? undefined : parent.top
                topMargin: control.iconCenter ? 0 : 20
                rightMargin: 20
            }
            source: control.iconSource
            width: control.iconSize
            height: control.iconSize
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }
    }
}
