import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: sidePanelRoot
    property var editorRoot: null
    property bool isCoagSide: false
    property var sideState: ({})

    readonly property int autoRowHeight: editorRoot ? editorRoot.modeRowHeight : 100

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: isCoagSide && editorRoot.modeSelectedInSide(true)
                     && ((editorRoot.socId <= 1 && editorRoot.isBiCoagModeInSide(true))
                         || (editorRoot.socId >= 2 && editorRoot.socId <= 3 && editorRoot.isSoftModeInSide(true)))

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: sidePanelRoot.autoRowHeight
                Layout.minimumHeight: sidePanelRoot.autoRowHeight
                Layout.maximumHeight: sidePanelRoot.autoRowHeight
                spacing: 8
                visible: editorRoot.socId <= 1 && editorRoot.isBiCoagModeInSide(true)

                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("АВТОСТОП")
                    flat: true
                    background: Rectangle {
                        radius: 14
                        border.width: editorRoot.socketAutoModeState === 1 ? 2 : 1
                        border.color: editorRoot.socketAutoModeState === 1 ? editorRoot.autoBtnOnBorder : editorRoot.autoBtnOffBorder
                        color: editorRoot.socketAutoModeState === 1 ? editorRoot.autoBtnOnFill : editorRoot.autoBtnOffFill
                    }
                    contentItem: Text {
                        text: parent.text
                        color: editorRoot.socketAutoModeState === 1 ? "black" : editorRoot.autoBtnOffText
                        font.pixelSize: 26
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: editorRoot.requestAutoMode(1, qsTr("В режиме АВТОСТОП инструмент активируется с помощью педали.\nПо завершении коагуляции процесс прекращается автоматически"))
                }

                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("АВТОСТАРТ/СТОП")
                    flat: true
                    background: Rectangle {
                        radius: 14
                        border.width: editorRoot.socketAutoModeState === 2 ? 2 : 1
                        border.color: editorRoot.socketAutoModeState === 2 ? editorRoot.autoBtnOnBorder : editorRoot.autoBtnOffBorder
                        color: editorRoot.socketAutoModeState === 2 ? editorRoot.autoBtnOnFill : editorRoot.autoBtnOffFill
                    }
                    contentItem: Text {
                        text: parent.text
                        color: editorRoot.socketAutoModeState === 2 ? "black" : editorRoot.autoBtnOffText
                        font.pixelSize: 26
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: editorRoot.requestAutoMode(2, qsTr("В режиме АВТОСТАРТ/СТОП активация происходит автоматически без нажатия педали!"))
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: sidePanelRoot.autoRowHeight
                Layout.minimumHeight: sidePanelRoot.autoRowHeight
                Layout.maximumHeight: sidePanelRoot.autoRowHeight
                spacing: 8
                visible: editorRoot.socId >= 2 && editorRoot.socId <= 3 && editorRoot.isSoftModeInSide(true)

                Button {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("АВТОСТОП")
                    flat: true
                    background: Rectangle {
                        radius: 14
                        border.width: editorRoot.socketAutoModeState === 1 ? 2 : 1
                        border.color: editorRoot.socketAutoModeState === 1 ? editorRoot.autoBtnOnBorder : editorRoot.autoBtnOffBorder
                        color: editorRoot.socketAutoModeState === 1 ? editorRoot.autoBtnOnFill : editorRoot.autoBtnOffFill
                    }
                    contentItem: Text {
                        text: parent.text
                        color: editorRoot.socketAutoModeState === 1 ? "black" : editorRoot.autoBtnOffText
                        font.pixelSize: 26
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: editorRoot.requestAutoMode(1, qsTr("В режиме АВТОСТОП инструмент активируется с помощью педали или держателя инструментов.\nПо завершении коагуляции процесс прекращается автоматически"))
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Задержка автозапуска")
                visible: editorRoot.socId <= 1 && editorRoot.isBiCoagModeInSide(true) && editorRoot.socketAutoModeState === 2
                horizontalAlignment: Text.AlignHCenter
                color: editorRoot.uiMidGray
                font.pixelSize: 22
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: sidePanelRoot.autoRowHeight - 15
                Layout.minimumHeight: sidePanelRoot.autoRowHeight - 15
                Layout.maximumHeight: sidePanelRoot.autoRowHeight - 15
                spacing: 6
                visible: editorRoot.socId <= 1 && editorRoot.isBiCoagModeInSide(true) && editorRoot.socketAutoModeState === 2

                Repeater {
                    model: [
                        { label: qsTr("0\nсек"), ms: 0 },
                        { label: qsTr("0.5\nсек"), ms: 500 },
                        { label: qsTr("1.0\nсек"), ms: 1000 },
                        { label: qsTr("1.5\nсек"), ms: 1500 }
                    ]
                    delegate: Button {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: modelData.label
                        flat: true
                        background: Rectangle {
                            radius: 12
                            border.width: periphHandle.autoDelayMs === modelData.ms ? 2 : 1
                            border.color: periphHandle.autoDelayMs === modelData.ms ? editorRoot.autoBtnOnBorder : editorRoot.autoBtnOffBorder
                            color: periphHandle.autoDelayMs === modelData.ms ? editorRoot.autoBtnOnFill : editorRoot.autoBtnOffFill
                        }
                        contentItem: Text {
                            text: parent.text
                            color: periphHandle.autoDelayMs === modelData.ms ? "black" : editorRoot.autoBtnOffText
                            font.pixelSize: 26
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onPressed: periphHandle.setAutoDelayMs(modelData.ms)
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: editorRoot.modeSelectedInSide(isCoagSide) && sideState.isEndo
            text: editorRoot.endoPulseRateText(sideState.modeId)
            horizontalAlignment: Text.AlignHCenter
            color: "black"
            font.pixelSize: 24
            font.bold: true
        }

        EndoChart {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            visible: editorRoot.modeSelectedInSide(isCoagSide) && sideState.isEndo
            cutEffect: editorRoot.endoCutEffect(isCoagSide)
            coagEffect: editorRoot.endoCoagEffect(isCoagSide)
            modeName: sideState.modeName
        }

        Item { Layout.fillHeight: true }
    }
}
