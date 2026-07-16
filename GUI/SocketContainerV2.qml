import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Rectangle {
    id: socketContainer

    signal progAddRequest(int type)
    property var innerModel
    property alias socketEditorOpened: socketEditor.opened
    property alias fullSocketEditorOpened: fullSocketEditor.opened
    color: "gray"

    function updateActivationOverlayGeometry() {
        if (repeat.count <= 0) {
            return
        }
        var first = repeat.itemAt(0)
        var last = repeat.itemAt(repeat.count - 1)
        if (!first || !last) {
            return
        }
        var topLeft = first.mapToItem(socketContainer, 0, 0)
        var bottomRight = last.mapToItem(socketContainer, 0, last.height)
        activationIndicator.x = topLeft.x
        activationIndicator.y = topLeft.y
        activationIndicator.width = Math.max(0, first.width)
        activationIndicator.height = Math.max(0, bottomRight.y - topLeft.y)
    }

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
            height: theModel.endoProgramView ? 0 : 50
            Layout.fillWidth: true
            visible: !theModel.endoProgramView
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
                            font.pixelSize: 34
                            font.bold: true
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
                    visible: theModel.subProgCount < 4 && !theModel.endoProgramView

                    Text {
                        text: "+"
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        anchors.fill: parent
                        color: "white"
                        font.pixelSize: 36
                        font.bold: true
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
                    visible: theModel.subProgCount > 1 && !theModel.endoProgramView

                    Text {
                        text: qsTr("удалить")
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        anchors.fill: parent
                        color: "white"
                        font.pixelSize: 30
                        font.bold: true
                    }
                    MouseArea {
                        anchors.fill: parent

                        onClicked: confirmDeleteSubProgDialog.open()
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
            activationOverlay: activationIndicator
        }
        Item {
            Layout.fillHeight: true
        }
    }

    Activation {
        id: activationIndicator
        parent: socketContainer
        z: 500
        onAboutToShow: Qt.callLater(socketContainer.updateActivationOverlayGeometry)
    }

    Timer {
        id: activationGeometryTimer
        interval: 50
        repeat: true
        running: activationIndicator.visible
        onTriggered: socketContainer.updateActivationOverlayGeometry()
    }

    SocketEditor {
        id: socketEditor
    }

    FullSocketEditor {
        id: fullSocketEditor
    }
    ProgAdditionPop {
        id: progSelector
        width: socketContainer.width
        height: socketContainer.height/3
        y: 0
        modal: true
    }

    Dialog {
        id: confirmDeleteSubProgDialog
        title: ""
        modal: true
        width: Math.min(socketContainer.width * 0.92, 980)
        height: 420
        x: (socketContainer.width - width) / 2
        y: (socketContainer.height - height) / 2

        contentItem: Rectangle {
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 26
                anchors.bottomMargin: 14
                spacing: 18

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Подтверждение удаления")
                    horizontalAlignment: Qt.AlignHCenter
                    font.pixelSize: 40
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Текущий лист программы будет удалён. Продолжить?")
                    horizontalAlignment: Qt.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 36
                }

                Item { Layout.fillHeight: true }
            }
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 132

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                spacing: 18

                DialogActionButton {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    text: qsTr("ОТМЕНА")
                    labelPixelSize: 34
                    onPressed: confirmDeleteSubProgDialog.close()
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    text: qsTr("ПРИНЯТЬ")
                    primary: true
                    labelPixelSize: 34
                    onPressed: {
                        recomHandle.removeSubProg()
                        confirmDeleteSubProgDialog.close()
                    }
                }
            }
        }
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
        target: fullSocketEditor
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
            socketEditor.prepareEditorData()
            socketEditor.open()
        }
        function onFullSocketEditorRequest(soc) {
            fullSocketEditor.socId = soc
            periphHandle.enableActivation = false
            fullSocketEditor.open()
        }
    }
}

