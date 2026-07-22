import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Popup {
    id: root
    modal: true
    parent: Overlay.overlay
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    x: 0
    y: 0
    padding: 0

    property int socId
    property int modeIndex
    property bool isCoag
    property bool deferCommit: false
    property bool dialogAccepted: false
    property bool openingInProgress: false
    readonly property var modeEditor: Editor

    readonly property color fotekBlue: "#264093"
    readonly property color uiMidGray: "#5A6478"
    readonly property int screenMargin: 20
    readonly property int previewImageSize: 110
    readonly property int briefFontSize: 26
    readonly property int briefVerticalMargin: 10
    readonly property color previewBorderColor: "#C7CEDA"
    readonly property color listSelectedBackground: isCoag ? "#0B4FB3" : "#F4D13D"
    readonly property color listSelectedText: isCoag ? "white" : "black"

    property var itemIdArr: []
    property var itemNameArr: []
    property var itemNumArr: []
    property string imagePrefix: (socId <= 1) ? "bimode" : "monomode"

    property int currentModeNum: {
        var idx = modeEditor.currentModeIndex
        if (idx < 0 || idx >= itemNumArr.length)
            return 0
        return itemNumArr[idx]
    }

    background: Rectangle {
        color: "#F3F5F9"
    }

    ListModel {
        id: combinedModel
    }

    function selectedModeTitle() {
        var idx = modeEditor.currentModeIndex
        if (idx >= 0 && idx < itemNameArr.length)
            return itemNameArr[idx]
        var mode = modeEditor.currentMode
        if (mode && mode.name !== undefined && mode.name !== null)
            return mode.name
        return ""
    }

    readonly property bool modeSelected: {
        var idx = modeEditor.currentModeIndex
        if (idx < 0)
            return false
        if (itemNameArr.length > 0 && idx === itemNameArr.length - 1)
            return false
        var mode = modeEditor.currentMode
        if (mode && mode.id !== undefined && mode.id !== null)
            return parseInt(mode.id) !== 1000
        return true
    }

    function deselectMode() {
        if (openingInProgress)
            return
        var idx = itemNameArr.length - 1
        if (idx < 0)
            return
        modeListView.selectIndex(idx)
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

    function closeWithoutCommit() {
        dialogAccepted = false
        if (!deferCommit)
            modeEditor.rollBack()
        root.close()
    }

    function commitAndClose() {
        dialogAccepted = true
        if (!deferCommit)
            modeEditor.commitChanges()
        root.close()
    }

    function acceptAndClose() {
        if (!modeEditor.hasChanges) {
            closeWithoutCommit()
            return
        }
        commitAndClose()
    }

    onAboutToShow: dialogAccepted = false

    onOpened: {
        openingInProgress = true
        modeEditor.initialize(socId, modeIndex, isCoag)

        itemNameArr = modeEditor.modeNames
        itemIdArr = modeEditor.modeNamesIds()
        itemNumArr = modeEditor.modeNamesNums()

        updateModel()

        var idx = root.modeIndex
        if (idx < 0 || idx >= itemNameArr.length)
            idx = modeEditor.currentModeIndex
        if (idx < 0 || idx >= itemNameArr.length)
            idx = 0

        modeEditor.currentModeIndex = idx
        // Explicit sync: model rebuild resets ListView.currentIndex and used to break
        // the curIndex binding, leaving highlight on item 0 while descriptions stayed correct.
        modeListView.curIndex = idx

        Qt.callLater(function() {
            modeListView.curIndex = modeEditor.currentModeIndex
            modeListView.positionSelectedItem()
            Qt.callLater(function() {
                modeListView.positionSelectedItem()
                openingInProgress = false
            })
        })
    }

    Rectangle {
        anchors.fill: parent
        color: "#F3F5F9"

        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 78
            color: isCoag ? "#0B4FB3" : "#F4D13D"

            Label {
                anchors {
                    left: parent.left
                    right: closeButton.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: root.screenMargin
                    rightMargin: 12
                }
                text: !isCoag
                      ? qsTr("Выберите режим РЕЗАНИЯ для выхода %1").arg(modeEditor.socketName)
                      : qsTr("Выберите режим КОАГУЛЯЦИИ для выхода %1").arg(modeEditor.socketName)
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 34
                font.bold: true
                color: isCoag ? "white" : "black"
            }

            Button {
                id: closeButton
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                    rightMargin: root.screenMargin
                }
                width: 68
                onPressed: closeWithoutCommit()

                background: Rectangle {
                    color: "transparent"
                }

                contentItem: Text {
                    text: qsTr("X")
                    font.pixelSize: 34
                    font.bold: true
                    color: isCoag ? "white" : "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        RowLayout {
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            spacing: 9

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                color: "transparent"

                Button {
                    id: modeScrollUp
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 56
                    text: qsTr("▲")
                    font.pixelSize: 24
                    background: Rectangle {
                        radius: 18
                        color: "white"
                        border.color: root.fotekBlue
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: root.fotekBlue
                        font.pixelSize: 24
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: modeListView.scrollUp()
                }

                ItemList {
                    id: modeListView
                    anchors.top: modeScrollUp.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: modeScrollDown.top
                    anchors.bottomMargin: 10
                    curIndex: modeEditor.currentModeIndex
                    imageSourceTemplate: "image://modes/" + imagePrefix + "%1"
                    selectedBackgroundColor: root.listSelectedBackground
                    selectedTextColor: root.listSelectedText
                    unselectedTextColor: root.fotekBlue
                    itemBackgroundColor: "transparent"
                    selectedBorderColor: "transparent"
                    itemBorderColor: "transparent"
                    selectedBorderWidth: 0
                    itemBorderWidth: 0
                    itemCornerRadius: 8
                    keepSelectedItemAtTop: true
                    selectedItemRowsAbove: 2
                    noAutoScrollItemId: 1000
                    itemFontPixelSize: 22
                }

                Button {
                    id: modeScrollDown
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 56
                    text: qsTr("▼")
                    font.pixelSize: 24
                    background: Rectangle {
                        radius: 18
                        color: "white"
                        border.color: root.fotekBlue
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: root.fotekBlue
                        font.pixelSize: 24
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: modeListView.scrollDown()
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "#C7CEDA"
            }

            ColumnLayout {
                id: previewPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                readonly property int briefAreaWidth: {
                    var w = previewPanel.width - previewImageSize - 12
                    return w > 10 ? w : 10
                }

                Label {
                    Layout.fillWidth: true
                    visible: selectedModeTitle().length > 0
                    text: root.modeSelected ? selectedModeTitle() : qsTr("РЕЖИМ НЕ ВЫБРАН\n\n(ВЫКЛЮЧЕН)")
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 32
                    font.bold: true
                    color: fotekBlue
                }

                RowLayout {
                    id: briefRow
                    Layout.fillWidth: true
                    visible: root.modeSelected
                    Layout.preferredHeight: Math.max(previewImageSize, briefRect.briefContentHeight)
                    spacing: 12

                    Rectangle {
                        id: briefRect
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(previewImageSize, briefContentHeight)
                        Layout.minimumHeight: previewImageSize
                        Layout.alignment: Qt.AlignTop
                        radius: 20
                        color: "white"
                        border.width: 1
                        border.color: previewBorderColor

                        readonly property int briefTextWidth: {
                            var inner = width - 2 * briefVerticalMargin
                            if (inner > 1)
                                return inner
                            var fallback = previewPanel.briefAreaWidth - 2 * briefVerticalMargin
                            return fallback > 1 ? fallback : 1
                        }

                        readonly property int briefContentHeight:
                            Math.ceil(briefMeasure.paintedHeight) + 2 * briefVerticalMargin + 4

                        Text {
                            id: briefMeasure
                            visible: false
                            width: briefRect.briefTextWidth
                            text: modeEditor.modeBrief
                            font.pixelSize: briefFontSize
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            width: briefRect.briefTextWidth
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: briefVerticalMargin
                            text: modeEditor.modeBrief
                            color: root.fotekBlue
                            font.pixelSize: briefFontSize
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: previewImageSize
                        Layout.preferredHeight: previewImageSize
                        Layout.alignment: Qt.AlignTop
                        radius: 20
                        color: "white"
                        border.width: 1
                        border.color: previewBorderColor

                        Image {
                            anchors.fill: parent
                            anchors.margins: 8
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: ("image://modes/" + imagePrefix + "%1").arg(currentModeNum)
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.modeSelected
                    radius: 20
                    color: "white"
                    border.width: 1
                    border.color: previewBorderColor

                    Label {
                        anchors.fill: parent
                        anchors.margins: 16
                        text: modeEditor.modeDescript
                        color: root.fotekBlue
                        font.pixelSize: 24
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Rectangle {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 86
            color: "transparent"

            DialogActionButton {
                anchors.left: parent.left
                anchors.leftMargin: root.screenMargin
                anchors.verticalCenter: parent.verticalCenter
                width: 195
                height: 62
                text: qsTr("ОТМЕНА")
                secondaryColor: "white"
                secondaryBorderWidth: 2
                secondaryBorderColor: root.fotekBlue
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: root.fotekBlue
                onPressed: closeWithoutCommit()
            }

            DialogActionButton {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: 420
                height: 62
                visible: root.modeSelected
                text: qsTr("ВЫКЛЮЧИТЬ РЕЖИМ")
                secondaryColor: "white"
                secondaryBorderWidth: 2
                secondaryBorderColor: root.fotekBlue
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: root.fotekBlue
                onPressed: deselectMode()
            }

            DialogActionButton {
                anchors.right: parent.right
                anchors.rightMargin: root.screenMargin
                anchors.verticalCenter: parent.verticalCenter
                width: 195
                height: 62
                text: qsTr("ПРИНЯТЬ")
                primary: true
                enabled: true
                primaryEnabledColor: modeEditor.hasChanges ? root.fotekBlue : "#26409370"
                primaryDisabledColor: "#26409370"
                primaryBorderWidth: 1
                primaryBorderColor: "#1E3274"
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: "white"
                onPressed: acceptAndClose()
            }
        }
    }

    Connections {
        target: modeListView
        function onNewIndexSelected(index) {
            if (openingInProgress)
                return
            modeEditor.currentModeIndex = index
        }
    }
}
