import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: control
    property var editorRoot: null
    property bool isCoagSide: false
    property var sideState: ({})

    readonly property bool modeSelected: editorRoot && editorRoot.modeSelectedInSide(isCoagSide)
    readonly property int btnW: editorRoot ? editorRoot.powerStepButtonWidth : 110
    readonly property int btnH: editorRoot ? editorRoot.controlButtonHeight : 88
    readonly property int endoBtnW: editorRoot ? Math.max(64, Math.round(editorRoot.powerStepButtonWidth * 0.72)) : 72
    readonly property int endoLabelHeight: 28
    readonly property bool showEndo: modeSelected && sideState.isEndo
    readonly property int endoGroupsGap: 28

    implicitHeight: showEndo ? (btnH + endoLabelHeight + 6) : btnH
    implicitWidth: 200

    RowLayout {
        anchors.fill: parent
        spacing: 10
        visible: modeSelected && !sideState.isEndo

        Item { Layout.fillWidth: true }

        Button {
            Layout.preferredWidth: control.btnW
            Layout.preferredHeight: control.btnH
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
                color: parent.enabled ? editorRoot.fotekBlue : editorRoot.uiMidGray
                font.pixelSize: 52
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onPressed: editorRoot.startMainPowerRepeat(isCoagSide, false)
            onReleased: editorRoot.stopMainPowerRepeat()
            onCanceled: editorRoot.stopMainPowerRepeat()
        }

        Label {
            Layout.preferredWidth: 120
            Layout.preferredHeight: control.btnH
            text: sideState.power
            font.pixelSize: 58
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: editorRoot.fotekBlue
        }

        Button {
            Layout.preferredWidth: control.btnW
            Layout.preferredHeight: control.btnH
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
                color: parent.enabled ? editorRoot.fotekBlue : editorRoot.uiMidGray
                font.pixelSize: 50
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onPressed: editorRoot.startMainPowerRepeat(isCoagSide, true)
            onReleased: editorRoot.stopMainPowerRepeat()
            onCanceled: editorRoot.stopMainPowerRepeat()
        }

        Item { Layout.fillWidth: true }
    }

    // Две равные половины: крайние кнопки по бокам ряда, цифры строго между своими ±,
    // между группами зазор, чтобы центральные ± не пересекались.
    RowLayout {
        anchors.fill: parent
        spacing: control.endoGroupsGap
        visible: showEndo

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignTop
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                spacing: 6

                Button {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
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
                        font.pixelSize: 44
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       editorRoot.endoCutEffect(isCoagSide) - 1,
                                                       editorRoot.endoCoagEffect(isCoagSide))
                }

                Label {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: control.btnH
                    text: editorRoot.endoCutEffect(isCoagSide)
                    font.pixelSize: 54
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: editorRoot.fotekBlue
                }

                Button {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
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
                        font.pixelSize: 44
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       editorRoot.endoCutEffect(isCoagSide) + 1,
                                                       editorRoot.endoCoagEffect(isCoagSide))
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: control.endoLabelHeight
                text: qsTr("эффект резания")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 22
                font.bold: true
                color: editorRoot.uiMidGray
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 0
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignTop
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                spacing: 6

                Button {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
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
                        font.pixelSize: 44
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       editorRoot.endoCutEffect(isCoagSide),
                                                       editorRoot.endoCoagEffect(isCoagSide) - 1)
                }

                Label {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: control.btnH
                    text: editorRoot.endoCoagEffect(isCoagSide)
                    font.pixelSize: 54
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: editorRoot.fotekBlue
                }

                Button {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
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
                        font.pixelSize: 44
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       editorRoot.endoCutEffect(isCoagSide),
                                                       editorRoot.endoCoagEffect(isCoagSide) + 1)
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: control.endoLabelHeight
                text: qsTr("эффект коагуляции")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 22
                font.bold: true
                color: editorRoot.uiMidGray
            }
        }
    }
}
