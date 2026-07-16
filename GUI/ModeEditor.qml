import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Popup {
    property int socId
    property int modeIndex
    property bool isCoag
    property bool deferCommit: false
    property bool dialogAccepted: false
    readonly property var modeEditor: Editor

    id: root
    modal: true
    parent: Overlay.overlay
    width: parent.width
    height: parent.height
    x: 0
    y: 0

    readonly property color fotekBlue: "#264093"
    readonly property color fotekOrange: "#faa731"
    readonly property int screenMargin: 34

    property var itemIdArr: []
    property var itemNameArr: []
    property var itemNumArr: []
    property string imagePrefix: (socId <= 1) ? "bimode" : "monomode"

    property int currentModeNum: {
        if (modeIndex < 0 || modeIndex >= itemNumArr.length)
            return 0
        return itemNumArr[modeIndex]
    }

    ListModel {
        id: combinedModel
    }

    function updateModel() {
        combinedModel.clear()

        if (itemIdArr.length !== itemNameArr.length || itemIdArr.length !== itemNumArr.length) {
            console.warn("Lists from C++ have different lengths!")
            return
        }
        for (var i = 0; i < itemIdArr.length; i++) {
            combinedModel.append({
                itemId: itemNumArr[i],
                itemName: itemNameArr[i],
            })
        }
        modeListView.innerModel = combinedModel
    }

    onAboutToShow: dialogAccepted = false

    onOpened: {
        modeEditor.initialize(socId, modeIndex, isCoag)

        itemNameArr = modeEditor.modeNames
        itemIdArr = modeEditor.modeNamesIds()
        itemNumArr = modeEditor.modeNamesNums()

        updateModel()
        modeEditor.currentModeIndex = root.modeIndex
        modeListView.curIndex = modeEditor.currentModeIndex
    }

    Rectangle {
        id: back
        anchors.fill: parent
        color: "#F3F5F9"

        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 100
            color: "transparent"

            Rectangle {
                id: titleCard
                anchors {
                    left: parent.left
                    right: cancelButton.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: root.screenMargin
                    rightMargin: 16
                }
                height: 72
                radius: 18
                color: "white"
                border.width: 2
                border.color: root.fotekOrange

                Label {
                    id: titleLable
                    anchors.fill: parent
                    anchors.margins: 12
                    text: !isCoag
                          ? qsTr("Выбор режима РЕЗАНИЯ для выхода %1").arg(modeEditor.socketName)
                          : qsTr("Выбор режима КОАГУЛЯЦИИ для выхода %1").arg(modeEditor.socketName)
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 26
                    font.bold: true
                    color: root.fotekBlue
                }
            }

            Button {
                id: cancelButton
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                    rightMargin: root.screenMargin
                }
                width: 72
                onClicked: {
                    root.dialogAccepted = false
                    if (!root.deferCommit)
                        modeEditor.rollBack()
                    root.close()
                }

                background: Rectangle {
                    radius: 18
                    color: "white"
                    border.width: 1
                    border.color: root.fotekBlue
                }

                contentItem: Text {
                    text: qsTr("X")
                    font.pixelSize: 30
                    font.bold: true
                    color: root.fotekBlue
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Rectangle {
            id: modeList
            color: "transparent"
            anchors {
                left: parent.left
                bottom: parent.bottom
                top: header.bottom
            }
            width: 0.3 * parent.width

            ItemList {
                id: modeListView
                curIndex: modeEditor.currentModeIndex
                color: "transparent"
                itemFontPixelSize: 22
                itemCornerRadius: 24
                selectedBackgroundColor: "white"
                selectedTextColor: root.fotekBlue
                unselectedTextColor: root.fotekBlue
                itemBackgroundColor: "white"
                selectedBorderColor: root.fotekOrange
                itemBorderColor: "#C7CEDA"
                selectedBorderWidth: 2
                itemBorderWidth: 1
                anchors {
                    top: parent.top
                    bottom: footer.top
                    left: parent.left
                    right: parent.right
                    leftMargin: root.screenMargin
                    rightMargin: 8
                }
                imageSourceTemplate: "image://modes/" + imagePrefix + "%1"
            }

            RowLayout {
                id: footer
                height: 80
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    leftMargin: root.screenMargin
                    rightMargin: 8
                    bottomMargin: 8
                }
                spacing: 16

                Button {
                    id: downButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onClicked: modeListView.scrollDown()

                    background: Rectangle {
                        radius: 18
                        color: "white"
                        border.width: 1
                        border.color: root.fotekBlue
                    }

                    contentItem: Text {
                        text: qsTr("▼")
                        font.pixelSize: 28
                        font.bold: true
                        color: root.fotekBlue
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    id: upButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onClicked: modeListView.scrollUp()

                    background: Rectangle {
                        radius: 18
                        color: "white"
                        border.width: 1
                        border.color: root.fotekBlue
                    }

                    contentItem: Text {
                        text: qsTr("▲")
                        font.pixelSize: 28
                        font.bold: true
                        color: root.fotekBlue
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Rectangle {
            id: instrumPreview
            anchors {
                top: header.bottom
                bottom: declineButton.top
                right: parent.right
                left: modeList.right
                rightMargin: root.screenMargin
            }
            color: "transparent"

            Rectangle {
                id: briefRect
                height: previewImage.height
                anchors {
                    top: parent.top
                    left: parent.left
                    right: previewImageFrame.left
                    topMargin: 12
                    rightMargin: 12
                }
                radius: 24
                color: "white"
                border.width: 2
                border.color: root.fotekOrange

                Label {
                    id: briefText
                    anchors.fill: parent
                    anchors.margins: 14
                    text: modeEditor.modeBrief
                    color: root.fotekBlue
                    font.pixelSize: 22
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                id: previewImageFrame
                width: 150
                height: width
                radius: 24
                color: "white"
                border.width: 1
                border.color: "#C7CEDA"
                anchors {
                    right: parent.right
                    top: parent.top
                    topMargin: 12
                }

                Image {
                    id: previewImage
                    anchors.fill: parent
                    anchors.margins: 10
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    source: ("image://modes/" + imagePrefix + "%1").arg(currentModeNum)
                }
            }

            Rectangle {
                id: descriptRect
                anchors {
                    left: parent.left
                    top: previewImageFrame.bottom
                    bottom: parent.bottom
                    topMargin: 12
                }
                width: parent.width
                radius: 24
                color: "white"
                border.width: 1
                border.color: "#C7CEDA"

                Label {
                    id: descriptText
                    anchors.fill: parent
                    anchors.margins: 16
                    text: modeEditor.modeDescript
                    color: root.fotekBlue
                    font.pixelSize: 24
                    wrapMode: Text.WordWrap
                }
            }
        }

        DialogActionButton {
            id: declineButton
            width: parent.width * 0.2
            height: parent.height * 0.15
            anchors {
                bottom: parent.bottom
                bottomMargin: root.screenMargin
                left: modeList.right
                leftMargin: 20
            }
            text: qsTr("ОТМЕНА")
            secondaryColor: "white"
            secondaryBorderWidth: 2
            secondaryBorderColor: root.fotekBlue
            cornerRadius: 20
            labelPixelSize: 30
            labelColor: root.fotekBlue
            onClicked: {
                root.dialogAccepted = false
                if (!root.deferCommit)
                    modeEditor.rollBack()
                root.close()
            }
        }

        DialogActionButton {
            id: acceptButton
            width: parent.width * 0.2
            height: parent.height * 0.15
            visible: modeEditor.hasChanges
            anchors {
                bottom: parent.bottom
                bottomMargin: root.screenMargin
                left: declineButton.right
                leftMargin: 24
            }
            text: qsTr("ПРИНЯТЬ")
            primary: true
            primaryEnabledColor: root.fotekBlue
            primaryDisabledColor: "#26409370"
            primaryBorderWidth: 1
            primaryBorderColor: "#1E3274"
            cornerRadius: 20
            labelPixelSize: 30
            labelColor: "white"
            onClicked: {
                root.dialogAccepted = true
                if (!root.deferCommit)
                    modeEditor.commitChanges()
                root.close()
            }
        }
    }

    Connections {
        target: modeListView
        function onNewIndexSelected(index) {
            modeEditor.currentModeIndex = index
        }
    }
}
