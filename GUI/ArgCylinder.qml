import QtQuick 2.15
import QtQuick.Controls 2.15
import Ui 1.0

Rectangle {

    property bool cylConnected
    property bool cylSelected
    property bool isFirst: true
    property bool interactive: true  // Разрешить клики по баллону
    signal cylClicked()

    // Размеры баллона рассчитываются от габаритов компонента,
    // чтобы корректно масштабироваться в узкой боковой панели.
    readonly property real compactScale: width < 60 ? 0.9 : 0.78
    readonly property real bodyWidth: Math.max(20, width * compactScale)
    readonly property real bodyHeight: Math.max(40, height * 0.88)
    readonly property real unit: bodyWidth / 4
    readonly property real borderSize: Math.max(2, bodyWidth * 0.09)

    id: cylinderRoot
    color: "transparent"

    Rectangle {
        id: cylinderBody
        width: bodyWidth
        height: bodyHeight
        radius: width / 2
        color: cylConnected
               ? (cylSelected ? UiPalette.success : Qt.lighter(UiPalette.success, 1.45))
               : UiPalette.surface
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        border.color: UiPalette.caseSteel
        border.width: borderSize
    }
    Rectangle {
        id: cylinderNeck
        width: unit
        height: unit * 2
        radius: width / 2
        color: UiPalette.textSecondary
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: cylinderBody.top
        anchors.bottomMargin: -(unit / 2)
    }
    Rectangle {
        id: cylinderValve
        width: unit * 3
        height: unit
        radius: height / 2
        color: UiPalette.textSecondary
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: cylinderNeck.top
        anchors.topMargin: Math.max(1, unit * 0.2)
    }
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: bodyHeight * 0.11
        text: isFirst ? "1" : "2"
        font.pixelSize: {
            if (cylConnected) {
                if (cylSelected) return bodyWidth * 0.62
                else return bodyWidth * 0.5
            }
            else return bodyWidth * 0.5
        }
        font.bold: true
        color: UiPalette.text
    }
    Label {
        id: empty1
        text: "X"
        anchors.fill: parent
        font.pixelSize: Math.min(width, height) * 0.8
        font.bold: true
        color: UiPalette.danger
        visible: !cylConnected
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
    }

    MouseArea {
        id: cylinder1MA
        anchors.fill: parent
        enabled: cylinderRoot.interactive
        onClicked: cylinderRoot.cylClicked()
    }
}
