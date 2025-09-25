import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    property var innerModel
    color: "darkgrey"
    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.topMargin: 10
        anchors.bottomMargin: 0
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10
        Rectangle {
            id: progPage
            height: 20
            Layout.fillWidth: true
            color: "transparent"
            RowLayout {
                anchors.fill: parent
                Item {
                    Layout.fillWidth: true
                }
                Repeater {
                    model: theModel.subProgCount
                    delegate: Rectangle {
                        height: 18
                        width: 36
                        color: index === theModel.subProgIdx ? "white" : "black"
                        border.color: "white"
                        border.width: 1
                        radius: 6
                        Text {
                            id: name
                            text: index + 1
                            horizontalAlignment: Qt.AlignHCenter
                            verticalAlignment: Qt.AlignVCenter
                            anchors.fill: parent
                            color: index === theModel.subProgIdx ? "black" : "white"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: theModel.subProgIdx = index
                        }

                    }
                }
                Item {
                    Layout.fillWidth: true
                }
            }
        }

        SocketRepeater {
            id: repeat
            model: innerModel
            containerMargins: layout.anchors.margins
            containerHeight: layout.height - layout.spacing - progPage.height
            usedSpacing: layout.spacing
        }
        Item {
            Layout.fillHeight: true
        }
    }

    InstrumEditor {
        id: instrDialog
    }
    ModeEditor {
        id: modeDialog
    }

    Connections {
        target: repeat
        function onInstrumDialogRequest(soc, mod, iscoag) {
            instrDialog.socId = soc
            instrDialog.modeIndex = mod
            instrDialog.isCoag = iscoag
            instrDialog.open()
        }
        function onModeDialogRequest(soc, mod, iscoag) {
            modeDialog.socId = soc
            modeDialog.modeIndex = mod
            modeDialog.isCoag = iscoag
            modeDialog.open()
        }
    }
}

