import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: control
    property var editorRoot: null
    property bool isCoagSide: false
    property var sideState: ({})

    implicitHeight: editorRoot ? editorRoot.controlButtonHeight : 72
    implicitWidth: 200

    RowLayout {
        anchors.centerIn: parent
        spacing: 10
        visible: editorRoot.modeSelectedInSide(isCoagSide) && !sideState.isEndo

        Button {
            Layout.preferredWidth: editorRoot.powerStepButtonWidth
            Layout.preferredHeight: editorRoot.controlButtonHeight
            text: qsTr("−")
            enabled: sideState.maxPower > 0
            flat: true
            background: Rectangle {
                radius: 16
                color: enabled ? "white" : "#E8ECF2"
                border.width: enabled ? 2 : 1
                border.color: enabled ? editorRoot.fotekOrange : "#C7CEDA"
            }
            contentItem: Text {
                text: parent.text
                color: enabled ? editorRoot.fotekBlue : editorRoot.uiMidGray
                font.pixelSize: 48
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onPressed: editorRoot.startMainPowerRepeat(isCoagSide, false)
            onReleased: editorRoot.stopMainPowerRepeat()
            onCanceled: editorRoot.stopMainPowerRepeat()
        }

        Label {
            Layout.preferredWidth: 100
            text: sideState.power
            font.pixelSize: 46
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            color: editorRoot.fotekBlue
        }

        Button {
            Layout.preferredWidth: editorRoot.powerStepButtonWidth
            Layout.preferredHeight: editorRoot.controlButtonHeight
            text: qsTr("+")
            enabled: sideState.maxPower > 0
            flat: true
            background: Rectangle {
                radius: 16
                color: enabled ? "white" : "#E8ECF2"
                border.width: enabled ? 2 : 1
                border.color: enabled ? editorRoot.fotekOrange : "#C7CEDA"
            }
            contentItem: Text {
                text: parent.text
                color: enabled ? editorRoot.fotekBlue : editorRoot.uiMidGray
                font.pixelSize: 44
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onPressed: editorRoot.startMainPowerRepeat(isCoagSide, true)
            onReleased: editorRoot.stopMainPowerRepeat()
            onCanceled: editorRoot.stopMainPowerRepeat()
        }
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 10
        visible: editorRoot.modeSelectedInSide(isCoagSide) && sideState.isEndo

        Button {
            Layout.preferredWidth: editorRoot.powerStepButtonWidth
            Layout.preferredHeight: editorRoot.controlButtonHeight
            text: qsTr("−")
            flat: true
            background: Rectangle {
                radius: 16
                color: "white"
                border.width: 2
                border.color: editorRoot.fotekOrange
            }
            contentItem: Text {
                text: parent.text
                color: editorRoot.fotekBlue
                font.pixelSize: 42
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onPressed: editorRoot.setEndoPower(isCoagSide, editorRoot.endoCutEffect(isCoagSide) - 1, editorRoot.endoCoagEffect(isCoagSide))
        }
        Label {
            Layout.preferredWidth: 80
            text: editorRoot.endoCutEffect(isCoagSide)
            font.pixelSize: 42
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            color: editorRoot.fotekBlue
        }
        Button {
            Layout.preferredWidth: editorRoot.powerStepButtonWidth
            Layout.preferredHeight: editorRoot.controlButtonHeight
            text: qsTr("+")
            flat: true
            background: Rectangle {
                radius: 16
                color: "white"
                border.width: 2
                border.color: editorRoot.fotekOrange
            }
            contentItem: Text {
                text: parent.text
                color: editorRoot.fotekBlue
                font.pixelSize: 42
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onPressed: editorRoot.setEndoPower(isCoagSide, editorRoot.endoCutEffect(isCoagSide) + 1, editorRoot.endoCoagEffect(isCoagSide))
        }
    }
}
