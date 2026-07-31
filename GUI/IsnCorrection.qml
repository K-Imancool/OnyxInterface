import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import StratifyLabs.UI 2.0
import BackEnd 1.0

Item {
    id: isnRoot
    signal returnButtonPressed()

    property bool isnEnabled: false
    property int voltage: 0
    readonly property int voltageStep: 5
    readonly property int voltageMax: 110
    readonly property int rowHeight: 90
    readonly property int ctrlButtonWidth: 160
    readonly property int labelWidth: 260
    readonly property int dummyWidth: 200

    function sendIsn(dacAction) {
        if (typeof appControl === "undefined" || !appControl)
            return
        appControl.manageIsn(isnRoot.isnEnabled, isnRoot.voltage, dacAction)
    }

    function setVoltage(value) {
        var clamped = Math.max(0, Math.min(isnRoot.voltageMax, value))
        clamped = Math.round(clamped / isnRoot.voltageStep) * isnRoot.voltageStep
        isnRoot.voltage = clamped
        if (isnRoot.isnEnabled)
            sendIsn(LinkStm.IsnDacNone)
    }

    function toggleIsn() {
        isnRoot.isnEnabled = !isnRoot.isnEnabled
        sendIsn(LinkStm.IsnDacNone)
    }

    Component.onDestruction: {
        if (isnRoot.isnEnabled && typeof appControl !== "undefined" && appControl)
            appControl.manageIsn(false, 0, 0)
    }

    Rectangle {
        anchors.fill: parent
        color: "darkslategray"
    }

    SLabel {
        id: screenTitle
        style: "label-primary lg"
        text: qsTr("Управление ИСН")
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    ColumnLayout {
        anchors {
            top: screenTitle.bottom
            topMargin: 28
            left: parent.left
            right: parent.right
            bottom: retButton.top
            bottomMargin: 12
            leftMargin: 40
            rightMargin: 40
        }
        spacing: 22

        SButton {
            id: isnToggleButton
            style: isnRoot.isnEnabled ? "btn-danger lg" : "btn-primary lg"
            Layout.fillWidth: true
            Layout.preferredHeight: isnRoot.rowHeight
            text: isnRoot.isnEnabled ? qsTr("ИСН: Включен") : qsTr("ИСН: Отключен")
            onPressed: isnRoot.toggleIsn()
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: 22
            rowSpacing: 30

            // --- Напряжение ---
            Text {
                text: qsTr("Напряжение")
                Layout.preferredWidth: isnRoot.labelWidth
                Layout.preferredHeight: isnRoot.rowHeight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                color: "white"
                font.pixelSize: 36
                font.bold: true
            }

            Item {
                Layout.preferredWidth: isnRoot.dummyWidth
            }

            SButton {
                style: "btn-primary lg"
                Layout.preferredWidth: isnRoot.ctrlButtonWidth
                Layout.preferredHeight: isnRoot.rowHeight
                text: qsTr("−5")
                enabled: isnRoot.voltage > 0
                onPressed: isnRoot.setVoltage(isnRoot.voltage - isnRoot.voltageStep)
            }

            Text {
                text: qsTr("%1 В").arg(isnRoot.voltage)
                Layout.fillWidth: true
                Layout.preferredHeight: isnRoot.rowHeight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "white"
                font.pixelSize: 48
                font.bold: true
            }

            SButton {
                style: "btn-primary lg"
                Layout.preferredWidth: isnRoot.ctrlButtonWidth
                Layout.preferredHeight: isnRoot.rowHeight
                text: qsTr("+5")
                enabled: isnRoot.voltage < isnRoot.voltageMax
                onPressed: isnRoot.setVoltage(isnRoot.voltage + isnRoot.voltageStep)
            }

            // --- ЦАП ---
            Text {
                text: qsTr("ЦАП")
                Layout.preferredWidth: isnRoot.labelWidth
                Layout.preferredHeight: isnRoot.rowHeight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                color: "white"
                font.pixelSize: 36
                font.bold: true
            }

            Item {
                Layout.preferredWidth: isnRoot.dummyWidth
            }

            SButton {
                style: "btn-primary lg"
                Layout.preferredWidth: isnRoot.ctrlButtonWidth
                Layout.preferredHeight: isnRoot.rowHeight
                text: qsTr("−")
                enabled: isnRoot.isnEnabled
                onPressed: isnRoot.sendIsn(LinkStm.IsnDacDec)
            }

            Text {
                text: (typeof appControl !== "undefined" && appControl && appControl.isnDacValue >= 0)
                      ? String(appControl.isnDacValue)
                      : "—"
                Layout.fillWidth: true
                Layout.preferredHeight: isnRoot.rowHeight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "white"
                font.pixelSize: 48
                font.bold: true
            }

            SButton {
                style: "btn-primary lg"
                Layout.preferredWidth: isnRoot.ctrlButtonWidth
                Layout.preferredHeight: isnRoot.rowHeight
                text: qsTr("+")
                enabled: isnRoot.isnEnabled
                onPressed: isnRoot.sendIsn(LinkStm.IsnDacInc)
            }

            // --- АЦП ---
            Text {
                text: qsTr("АЦП")
                Layout.preferredWidth: isnRoot.labelWidth
                Layout.preferredHeight: isnRoot.rowHeight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                color: "white"
                font.pixelSize: 36
                font.bold: true
            }

            Item {
                Layout.preferredWidth: isnRoot.dummyWidth
            }

            Item {
                Layout.preferredWidth: isnRoot.ctrlButtonWidth
                Layout.preferredHeight: isnRoot.rowHeight
            }

            Text {
                text: (typeof appControl !== "undefined" && appControl && appControl.isnAdcValue >= 0)
                      ? String(appControl.isnAdcValue)
                      : "—"
                Layout.fillWidth: true
                Layout.preferredHeight: isnRoot.rowHeight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: "white"
                font.pixelSize: 48
                font.bold: true
            }

            Item {
                Layout.preferredWidth: isnRoot.ctrlButtonWidth
                Layout.preferredHeight: isnRoot.rowHeight
            }
        }

        SButton {
            style: "btn-primary lg"
            Layout.fillWidth: true
            Layout.preferredHeight: isnRoot.rowHeight
            text: qsTr("Запомнить значение ЦАП")
            enabled: isnRoot.isnEnabled
            onPressed: isnRoot.sendIsn(LinkStm.IsnDacSave)
        }

        Item { Layout.fillHeight: true }
    }

    SButton {
        id: retButton
        style: "btn-secondary"
        text: qsTr("Назад")
        onPressed: returnButtonPressed()
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 15
        }
    }
}
