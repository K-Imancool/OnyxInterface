import QtQuick 2.15

import StratifyLabs.UI 2.0

Item {
    id: frame

    property string title: ""
    property int titlePixelSize: 32
    property int titleBarHeight: 60
    property bool backButtonVisible: true
    property int backButtonMargins: 15
    property bool titleClickEnabled: false

    readonly property alias screenTitle: titleBar
    readonly property alias returnButton: backButton
    readonly property alias retButton: backButton

    signal returnButtonPressed()
    signal titleClicked()

    Rectangle {
        anchors.fill: parent
        color: "darkslategray"
        z: -1
    }

    Rectangle {
        id: titleBar
        z: 10
        color: "#1D7DC5"
        height: frame.titleBarHeight
        width: parent.width
        anchors.top: parent.top

        Text {
            text: frame.title
            color: "white"
            font.pixelSize: frame.titlePixelSize
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        MouseArea {
            anchors.fill: parent
            enabled: frame.titleClickEnabled
            onClicked: frame.titleClicked()
        }
    }

    SButton {
        id: backButton
        z: 11
        visible: frame.backButtonVisible
        style: "btn-secondary"
        text: qsTr("Назад")
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: frame.backButtonMargins
        }
        onPressed: frame.returnButtonPressed()
    }
}
