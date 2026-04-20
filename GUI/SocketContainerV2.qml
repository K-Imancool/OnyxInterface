import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: socketContainer

    signal progAddRequest(int type)
    property var innerModel
    property alias socketEditorOpened: socketEditor.opened
    color: "gray"
    
    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.topMargin: 5
        anchors.bottomMargin: 0
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        spacing: 10
        Rectangle {
            id: progPage
            height: 50
            Layout.fillWidth: true
            color: "transparent"
            RowLayout {
                anchors.fill: parent
                spacing: 20
                Item {
                    Layout.fillWidth: true
                }
                Repeater {
                    model: theModel.subProgCount
                    delegate: Rectangle {
                        height: parent.height
                        width: 60
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
                Rectangle {
                    id: progAddSign
                    height: parent.height
                    width: 60
                    color: "black"
                    border.color: "white"
                    border.width: 1
                    radius: 6
                    visible: theModel.subProgCount < 5 ? true : false

                    Text {
                        text: "+"
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        anchors.fill: parent
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent

                        onClicked:  {
                            periphHandle.enableActivation = true;
                            progSelector.open()
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Rectangle {
                    id: progDeleteSign
                    height: parent.height
                    width: 40*5
                    color: "black"
                    border.color: "red"
                    border.width: 1
                    radius: 6
                    visible: theModel.subProgCount < 5 ? true : false

                    Text {
                        text: qsTr("удалить")
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        anchors.fill: parent
                        color: "white"
                    }
                    MouseArea {
                        anchors.fill: parent

                        onClicked: recomHandle.removeSubProg()
                    }
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

    SocketEditor {
        id: socketEditor
    }
    ProgAdditionPop {
        id: progSelector
        width: socketContainer.width
        height: socketContainer.height/3
        y: 0
        modal: true
    }

    Connections {
        target: progSelector
        function onProgLoaderSelected (addType) {
            socketContainer.progAddRequest(addType)
        }
    }
    Connections {
        target: progSelector
        function onClosed() {
            periphHandle.enableActivation = true;
        }
    }
    Connections {
        target: socketEditor
        function onClosed() {
            periphHandle.enableActivation = true;
        }
    }

    Connections {
        target: repeat
        function onSocketEditorRequest(soc, mod, iscoag) {
            socketEditor.socId = soc
            socketEditor.modeIndex = mod
            socketEditor.isCoag = iscoag
            periphHandle.enableActivation = false;
            socketEditor.open()
        }
    }
}

