import QtQuick 2.15
import QtQuick.Controls 2.15

import BackEnd 1.0
import Ui 1.0

Rectangle {

    id: peripheryPanelRoot
    signal openArgonDrawer()
    signal openNeutralDrawer()
    signal menuCalled()

    property bool argonAvailable: true

    // Геометрия как у PedalContainer / SocketContainerV2: аргон = БИ1+БИ2, НЭ = МОНО1+МОНО2.
    readonly property int layoutMargins: 5
    readonly property int columnSpacing: 10
    readonly property int socketSpacing: 10
    readonly property int progPageHeight: (typeof theModel !== "undefined" && theModel && theModel.endoProgramView) ? 0 : 50
    readonly property int socketCount: (typeof theModel !== "undefined" && theModel) ? theModel.rowCount() : 4
    readonly property int socketTopOffset: (typeof theModel !== "undefined" && theModel && theModel.endoProgramView)
            ? layoutMargins
            : (layoutMargins + progPageHeight + columnSpacing)

    readonly property real uniformSocketHeight: {
        var count = Math.max(1, socketCount)
        var layoutInnerHeight = height - layoutMargins
        var containerHeight = layoutInnerHeight - columnSpacing - progPageHeight
        return (containerHeight - count * socketSpacing) / count
    }

    function socketTop(socketId) {
        return socketTopOffset + socketId * (uniformSocketHeight + socketSpacing)
    }

    function socketBottom(socketId) {
        return socketTop(socketId) + uniformSocketHeight
    }

    // БИ1..БИ2
    readonly property real biBandTop: socketTop(0)
    readonly property real biBandBottom: socketBottom(1)
    readonly property real biBandHeight: Math.max(0, biBandBottom - biBandTop)
    // МОНО1..МОНО2
    readonly property real monoBandTop: socketTop(2)
    readonly property real monoBandBottom: socketBottom(3)
    readonly property real monoBandHeight: Math.max(0, monoBandBottom - monoBandTop)

    readonly property int menuButtonHeight: Math.round(biBandTop)

    color: UiPalette.caseBase

    Rectangle {
        id: menuButton
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: peripheryPanelRoot.menuButtonHeight
        color: UiPalette.background
        radius: 10
        border.color: UiPalette.caseSteel
        border.width: 2

        Text {
            anchors.fill: parent
            anchors.margins: 4
            text: qsTr("МЕНЮ")
            font.pixelSize: Math.round(Math.min(width / 3.2, Math.max(18, height * 0.45)))
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: UiPalette.text
        }

        MouseArea {
            anchors.fill: parent
            onClicked: peripheryPanelRoot.menuCalled()
        }
    }

    Rectangle {
        id: argonCard
        visible: peripheryPanelRoot.argonAvailable
        x: 0
        y: Math.round(peripheryPanelRoot.biBandTop)
        width: parent.width
        height: Math.round(peripheryPanelRoot.biBandHeight)
        color: UiPalette.background
        radius: 10
        border.color: UiPalette.caseSteel
        border.width: 2

        Argon {
            id: argonView
            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.verticalCenter
                left: parent.left
                right: parent.right
            }
            height: compactContentHeight
            showControls: false
            compactOnLightBackground: true
            cylinder1Connected: periphHandle.argonCylinder1Connected
            cylinder2Connected: periphHandle.argonCylinder2Connected
            flowRate: periphHandle.argonFlowRate
            realFlowRate: periphHandle.argonRealRate
            isActivation: periphHandle.activation
            isBlowing: periphHandle.argonBlowing
            activCylinderFirst: periphHandle.activCylinderFirst
        }

        MouseArea {
            anchors.fill: parent
            z: 10
            enabled: peripheryPanelRoot.argonAvailable
            onClicked: peripheryPanelRoot.openArgonDrawer()
        }
    }

    Rectangle {
        id: argonPlaceholder
        visible: !peripheryPanelRoot.argonAvailable
        x: 0
        y: Math.round(peripheryPanelRoot.biBandTop)
        width: parent.width
        height: Math.round(peripheryPanelRoot.biBandHeight)
        color: UiPalette.background
        radius: 10
        border.color: UiPalette.caseSteel
        border.width: 2
    }

    // Как разделитель между педалями в PedalContainer.
    Rectangle {
        width: parent.width
        height: 2
        color: UiPalette.caseSteel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        y: Math.round(peripheryPanelRoot.monoBandTop) - 5
        z: 5
    }

    Item {
        id: neutralSlot
        x: 0
        y: Math.round(peripheryPanelRoot.monoBandTop)
        width: parent.width
        height: Math.round(peripheryPanelRoot.monoBandHeight)

        NeutralEl {
            id: neutralView
            anchors.fill: parent
            neutralConnected: periphHandle.neutralElConnected
            neutralDivided: periphHandle.neutralElDivided
            neutralSize: periphHandle.neutralSize
            showControls: false
        }

        MouseArea {
            anchors.fill: parent
            z: 10
            onClicked: peripheryPanelRoot.openNeutralDrawer()
        }
    }

    Connections {
        target: argonView
        function onFlowRateUpdated(newRate) {
            periphHandle.argonFlowRate = newRate
        }
        function onArgonBlow() {
            periphHandle.argonBlow()
        }
        function onActivCylinderToggled(first) {
            console.log("PeripheryPanel.qml: onActivCylinderToggled", first)
            periphHandle.activCylinderFirst = first
        }
    }
}
