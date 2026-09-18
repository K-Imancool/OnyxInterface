import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import StratifyLabs.UI 2.0

ServiceFrame {
    id: featureOptionsRoot

    title: qsTr("УПРАВЛЕНИЕ ОПЦИЯМИ")

    property int refreshCounter: 0

    readonly property int menuButtonWidth: 420
    readonly property int menuButtonHeight: 96
    readonly property int menuColumnsSpacing: 24

    function isKeyActivated(keyNumber) {
        return typeof featureUnlock !== "undefined"
                && featureUnlock
                && featureUnlock.isKeyActivated(keyNumber)
    }

    function toggleKey(keyNumber) {
        if (typeof featureUnlock === "undefined" || !featureUnlock) {
            return
        }
        featureUnlock.setKeyActivated(keyNumber, !isKeyActivated(keyNumber))
    }

    function toggleAllKeys() {
        if (typeof featureUnlock === "undefined" || !featureUnlock) {
            return
        }
        featureUnlock.setAllKeysActivated(!allKeysActive())
    }

    function allKeysActive() {
        if (typeof featureUnlock === "undefined" || !featureUnlock) {
            return false
        }
        return featureUnlock.areAllKeysActivated(9)
    }

    Component.onCompleted: {
        refreshCounter++
    }

    Connections {
        target: typeof featureUnlock !== "undefined" ? featureUnlock : null
        function onActivatedKeysChanged() {
            featureOptionsRoot.refreshCounter++
        }
    }

    GridLayout {
        anchors {
            top: screenTitle.bottom
            topMargin: 36
            horizontalCenter: parent.horizontalCenter
        }
        columns: 2
        columnSpacing: featureOptionsRoot.menuColumnsSpacing
        rowSpacing: 15
        width: featureOptionsRoot.menuButtonWidth * 2 + featureOptionsRoot.menuColumnsSpacing

        SButton {
            id: allKeysButton
            property bool allActive: {
                var _ = featureOptionsRoot.refreshCounter
                return featureOptionsRoot.allKeysActive()
            }

            style: allActive ? "btn-primary lg" : "btn-danger lg"
            Layout.columnSpan: 2
            Layout.fillWidth: true
            Layout.preferredHeight: featureOptionsRoot.menuButtonHeight
            text: allActive ? qsTr("Все ключи: ВКЛ") : qsTr("Все ключи: ВЫКЛ")
            onPressed: featureOptionsRoot.toggleAllKeys()
        }

        Repeater {
            model: 9

            delegate: SButton {
                property int keyNumber: index + 1
                property bool keyActive: {
                    var _ = featureOptionsRoot.refreshCounter
                    return featureOptionsRoot.isKeyActivated(keyNumber)
                }

                style: keyActive ? "btn-primary lg" : "btn-danger lg"
                Layout.preferredWidth: featureOptionsRoot.menuButtonWidth
                Layout.preferredHeight: featureOptionsRoot.menuButtonHeight
                text: keyActive
                      ? qsTr("Ключ №%1: ВКЛ").arg(keyNumber)
                      : qsTr("Ключ №%1: ВЫКЛ").arg(keyNumber)
                onPressed: featureOptionsRoot.toggleKey(keyNumber)
            }
        }
    }
}
