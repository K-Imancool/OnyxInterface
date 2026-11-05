import QtQuick 2.15
import QtQuick.Controls 2.15

import BackEnd 1.0
import Ui 1.0

Rectangle {

    id: peripheryPanelRoot
    signal openArgonDrawer()
    signal openNeutralDrawer()

    property bool argonAvailable: true

    color: UiPalette.caseBase

    NeutralEl {
        id: neutralView
        height: 280
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        neutralConnected: periphHandle.neutralElConnected
        neutralDivided: periphHandle.neutralElDivided
        neutralSize: periphHandle.neutralSize
        showControls: false
    }

    Rectangle {
        id: futurePanel
        anchors {
            left: parent.left
            right: parent.right
            top: peripheryPanelRoot.argonAvailable ? argonCard.bottom : parent.top
            bottom: neutralView.top
        }
        color: UiPalette.background
        radius: 10
        border.color: UiPalette.caseSteel
        border.width: 2
    }

    Rectangle {
        id: argonCard
        visible: peripheryPanelRoot.argonAvailable
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: visible ? argonView.compactContentHeight : 0
        color: UiPalette.background
        radius: 10
        border.color: UiPalette.caseSteel
        border.width: 2

        Argon {
            id: argonView
            anchors {
                top: parent.top
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

    MouseArea {
        anchors.fill: neutralView
        z: 10
        onClicked: peripheryPanelRoot.openNeutralDrawer()
    }
}
