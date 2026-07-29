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
    readonly property bool canStepPower: modeSelected && sideState.maxPower > 0
    readonly property bool canDecreasePower: canStepPower && sideState.power > 1
    readonly property bool canIncreasePower: canStepPower && sideState.power < sideState.maxPower
    readonly property int cutEffect: editorRoot ? editorRoot.endoCutEffect(isCoagSide) : 1
    readonly property int coagEffect: editorRoot ? editorRoot.endoCoagEffect(isCoagSide) : 1

    implicitHeight: showEndo ? (btnH + endoLabelHeight + 6) : btnH
    implicitWidth: 200

    // Подсветка нажатия как у кнопок расхода аргона: фон fotekOrange, текст чёрный
    component StepButton: Button {
        id: stepBtn
        flat: true
        property int labelPixelSize: 52

        background: Rectangle {
            radius: 16
            color: !stepBtn.enabled
                   ? "#E8ECF2"
                   : (stepBtn.down ? editorRoot.fotekOrange : "white")
            border.width: stepBtn.enabled ? 2 : 1
            border.color: stepBtn.enabled ? editorRoot.fotekOrange : "#C7CEDA"
        }
        contentItem: Text {
            text: stepBtn.text
            color: !stepBtn.enabled
                   ? editorRoot.uiMidGray
                   : (stepBtn.down ? "black" : editorRoot.fotekBlue)
            font.pixelSize: stepBtn.labelPixelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 10
        visible: modeSelected && !sideState.isEndo
        enabled: visible
        z: visible ? 1 : 0

        Item { Layout.fillWidth: true }

        StepButton {
            Layout.preferredWidth: control.btnW
            Layout.preferredHeight: control.btnH
            text: qsTr("−")
            enabled: control.canDecreasePower
            labelPixelSize: 52
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

        StepButton {
            Layout.preferredWidth: control.btnW
            Layout.preferredHeight: control.btnH
            text: qsTr("+")
            enabled: control.canIncreasePower
            labelPixelSize: 50
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
        enabled: visible
        z: visible ? 1 : 0

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

                StepButton {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
                    text: qsTr("−")
                    enabled: control.cutEffect > 1
                    labelPixelSize: 44
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       control.cutEffect - 1,
                                                       control.coagEffect)
                }

                Label {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: control.btnH
                    text: control.cutEffect
                    font.pixelSize: 54
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: editorRoot.fotekBlue
                }

                StepButton {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
                    text: qsTr("+")
                    enabled: control.cutEffect < 3
                    labelPixelSize: 44
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       control.cutEffect + 1,
                                                       control.coagEffect)
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

                StepButton {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
                    text: qsTr("−")
                    enabled: control.coagEffect > 1
                    labelPixelSize: 44
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       control.cutEffect,
                                                       control.coagEffect - 1)
                }

                Label {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    Layout.preferredHeight: control.btnH
                    text: control.coagEffect
                    font.pixelSize: 54
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: editorRoot.fotekBlue
                }

                StepButton {
                    Layout.preferredWidth: control.endoBtnW
                    Layout.preferredHeight: control.btnH
                    text: qsTr("+")
                    enabled: control.coagEffect < 3
                    labelPixelSize: 44
                    onPressed: editorRoot.setEndoPower(isCoagSide,
                                                       control.cutEffect,
                                                       control.coagEffect + 1)
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
