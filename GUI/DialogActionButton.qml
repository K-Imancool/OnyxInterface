import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control

    property bool primary: false
    property color primaryEnabledColor: "#2E7D32"
    property color primaryDisabledColor: "#702E7D32"
    property color primaryBorderColor: "transparent"
    property int primaryBorderWidth: 0
    property color secondaryColor: "#808080"
    property color secondaryBorderColor: "transparent"
    property int secondaryBorderWidth: 0
    property int cornerRadius: 18
    property int labelPixelSize: 24
    property color labelColor: "white"

    background: Rectangle {
        radius: control.cornerRadius
        color: control.primary
               ? (control.enabled ? control.primaryEnabledColor : control.primaryDisabledColor)
               : control.secondaryColor
        border.color: control.primary ? control.primaryBorderColor : control.secondaryBorderColor
        border.width: control.primary ? control.primaryBorderWidth : control.secondaryBorderWidth
    }

    contentItem: Text {
        text: control.text
        color: control.labelColor
        font.pixelSize: control.labelPixelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
