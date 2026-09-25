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

    // Подсветка и автоповтор локально на кнопку (как Argon).
    // Не трогаем Item.enabled: при enabled→false Qt гасит MouseArea и теряет release,
    // из‑за чего подсветка/жест залипают. Доступность шага — через interactive.
    component StepButton: Item {
        id: stepBtn
        property string text: ""
        property int labelPixelSize: 52
        property bool holdRepeat: false
        property bool interactive: true
        property bool pressActive: false

        signal activated()

        Rectangle {
            anchors.fill: parent
            radius: 16
            color: !stepBtn.interactive
                   ? "#E8ECF2"
                   : (stepBtn.pressActive ? editorRoot.fotekOrange : "white")
            border.width: stepBtn.interactive ? 2 : 1
            border.color: stepBtn.interactive ? editorRoot.fotekOrange : "#C7CEDA"
        }

        Text {
            anchors.fill: parent
            text: stepBtn.text
            color: !stepBtn.interactive
                   ? editorRoot.uiMidGray
                   : (stepBtn.pressActive ? "black" : editorRoot.fotekBlue)
            font.pixelSize: stepBtn.labelPixelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        MouseArea {
            id: holdArea
            // Поле касания шире визуала ~на 20% (по 10% с каждой стороны).
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: -Math.round(stepBtn.width * 0.10)
            anchors.rightMargin: -Math.round(stepBtn.width * 0.10)
            // Всегда enabled, чтобы гарантированно получить released/canceled.
            onPressed: {
                if (!stepBtn.interactive) {
                    mouse.accepted = true
                    return
                }
                stepBtn.pressActive = true
                stepBtn.activated()
                if (stepBtn.holdRepeat)
                    repeatDelay.restart()
            }
            onReleased: stepBtn.endPress()
            onCanceled: stepBtn.endPress()
        }

        function endPress() {
            pressActive = false
            stopRepeat()
        }

        function stopRepeat() {
            repeatDelay.stop()
            repeatTick.stop()
        }

        onInteractiveChanged: {
            if (!interactive) {
                // Достигли min/max во время удержания — гасим повтор и подсветку,
                // но MouseArea остаётся живым до отпускания пальца.
                pressActive = false
                stopRepeat()
            }
        }

        Timer {
            id: repeatDelay
            interval: 900
            repeat: false
            onTriggered: repeatTick.start()
        }

        Timer {
            id: repeatTick
            interval: 120
            repeat: true
            onTriggered: {
                if (!editorRoot || !editorRoot.opened || !stepBtn.interactive) {
                    stepBtn.endPress()
                    return
                }
                stepBtn.activated()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 10
        visible: modeSelected && !sideState.isEndo
        z: visible ? 1 : 0

        Item { Layout.fillWidth: true }

        StepButton {
            Layout.preferredWidth: control.btnW
            Layout.preferredHeight: control.btnH
            text: qsTr("−")
            interactive: control.canDecreasePower
            labelPixelSize: 52
            holdRepeat: true
            onActivated: {
                if (editorRoot)
                    editorRoot.stepMainPower(control.isCoagSide, false)
            }
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
            interactive: control.canIncreasePower
            labelPixelSize: 50
            holdRepeat: true
            onActivated: {
                if (editorRoot)
                    editorRoot.stepMainPower(control.isCoagSide, true)
            }
        }

        Item { Layout.fillWidth: true }
    }

    // Две равные половины: крайние кнопки по бокам ряда, цифры строго между своими ±,
    // между группами зазор, чтобы центральные ± не пересекались.
    RowLayout {
        anchors.fill: parent
        spacing: control.endoGroupsGap
        visible: showEndo
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
                    interactive: control.cutEffect > 1
                    labelPixelSize: 44
                    onActivated: editorRoot.setEndoPower(control.isCoagSide,
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
                    interactive: control.cutEffect < 3
                    labelPixelSize: 44
                    onActivated: editorRoot.setEndoPower(control.isCoagSide,
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
                    interactive: control.coagEffect > 1
                    labelPixelSize: 44
                    onActivated: editorRoot.setEndoPower(control.isCoagSide,
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
                    interactive: control.coagEffect < 3
                    labelPixelSize: 44
                    onActivated: editorRoot.setEndoPower(control.isCoagSide,
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
