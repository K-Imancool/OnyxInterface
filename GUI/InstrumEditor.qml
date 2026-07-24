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

    property int socId: -1
    property int modeIndex: -1
    property int instrIndex: -1
    property bool isCoag: false
    property bool deferCommit: false
    property bool dialogAccepted: false
    property bool openingInProgress: false
    readonly property var modeEditor: Editor

    readonly property color fotekBlue: "#264093"
    readonly property color uiMidGray: "#5A6478"
    readonly property int screenMargin: 20
    readonly property color previewBorderColor: "#C7CEDA"
    readonly property color listSelectedBackground: isCoag ? "#0B4FB3" : "#F4D13D"
    readonly property color listSelectedText: isCoag ? "white" : "black"
    readonly property string instrImagePrefix: isCoag ? "coaginstr" : "cutinstr"
    readonly property int recommendedButtonHeight: 90
    readonly property int recommendedEndoButtonHeight: 110

    property var itemIdArr: []
    property var itemNameArr: []
    property var itemNumArr: []

    property int currentInstrNum: {
        var idx = modeEditor.currentInstrIndex
        if (idx < 0 || idx >= itemNumArr.length)
            return 0
        return itemNumArr[idx]
    }

    readonly property bool instrumentSelected: {
        var idx = modeEditor.currentInstrIndex
        if (idx < 0 || idx >= itemNumArr.length)
            return false
        return parseInt(itemNumArr[idx]) !== 1000
    }

    background: Rectangle {
        color: "#F3F5F9"
    }

    ListModel {
        id: combinedModel
    }

    function selectedInstrTitle() {
        var idx = modeEditor.currentInstrIndex
        if (idx >= 0 && idx < itemNameArr.length)
            return itemNameArr[idx]
        return ""
    }

    function modeTitleText() {
        var mode = modeEditor.currentMode
        if (mode && mode.name !== undefined && mode.name !== null)
            return mode.name
        return ""
    }

    function instrBriefText() {
        var brief = modeEditor.instrBrief
        var title = selectedInstrTitle()
        if (!brief || brief === title)
            return ""
        return brief
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
        instrumListView.innerModel = combinedModel
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

    function defaultRecommendedPower() {
        if (modeEditor.midPowerBound !== 0)
            return modeEditor.midPowerBound
        if (modeEditor.lowPowerBound !== 0)
            return modeEditor.lowPowerBound
        if (modeEditor.highPowerBound !== 0)
            return modeEditor.highPowerBound
        return modeEditor.currentPower
    }

    function applyDefaultRecommendedPower() {
        var power = defaultRecommendedPower()
        if (power !== 0)
            modeEditor.updateParameter("currentpower", power)
    }

    function deselectInstrument() {
        if (openingInProgress)
            return
        var idx = itemNameArr.length - 1
        if (idx < 0)
            return
        instrumListView.selectIndex(idx)
    }

    onAboutToShow: dialogAccepted = false

    onOpened: {
        openingInProgress = true

        if (deferCommit) {
            // FullSocketEditor already keeps live (possibly uncommitted) mode in Editor.
            // Do not re-initialize from the committed model — that would show the old mode's instruments.
            if (modeIndex >= 0 && modeEditor.currentModeIndex !== modeIndex)
                modeEditor.currentModeIndex = modeIndex
        } else {
            modeEditor.initialize(socId, modeIndex, isCoag)
            if (modeIndex >= 0 && modeEditor.currentModeIndex !== modeIndex)
                modeEditor.currentModeIndex = modeIndex
        }

        itemNameArr = modeEditor.instrList
        itemIdArr = modeEditor.instrListIds()
        itemNumArr = modeEditor.instrListNums()
        updateModel()

        var idx = instrIndex
        if (idx < 0 || idx >= itemNameArr.length)
            idx = modeEditor.currentInstrIndex
        if (idx < 0 || idx >= itemNameArr.length)
            idx = itemNameArr.length > 0 ? itemNameArr.length - 1 : 0
        modeEditor.currentInstrIndex = idx
        instrumListView.curIndex = idx

        Qt.callLater(function() {
            instrumListView.curIndex = modeEditor.currentInstrIndex
            instrumListView.positionSelectedItem()
            Qt.callLater(function() {
                instrumListView.positionSelectedItem()
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
                      ? qsTr("Выберите инструмент РЕЗАНИЯ для выхода %1").arg(modeEditor.socketName)
                      : qsTr("Выберите инструмент КОАГУЛЯЦИИ для выхода %1").arg(modeEditor.socketName)
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
                Layout.preferredWidth: 425
                color: "transparent"

                Button {
                    id: instrScrollUp
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
                    onPressed: instrumListView.scrollUp()
                }

                ItemList {
                    id: instrumListView
                    anchors.top: instrScrollUp.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: instrScrollDown.top
                    anchors.bottomMargin: 10
                    curIndex: modeEditor.currentInstrIndex
                    imageSourceTemplate: "image://instruments/" + root.instrImagePrefix + "%1"
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
                    id: instrScrollDown
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
                    onPressed: instrumListView.scrollDown()
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
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: qsTr("РЕЖИМ: %1").arg(modeTitleText())
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 24
                    font.bold: true
                    color: uiMidGray
                }

                Label {
                    Layout.fillWidth: true
                    visible: selectedInstrTitle().length > 0
                    text: selectedInstrTitle()
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 32
                    font.bold: true
                    color: fotekBlue
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.instrumentSelected
                    radius: 20
                    color: "white"
                    border.width: 1
                    border.color: previewBorderColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        Image {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 280
                            Layout.preferredHeight: 320
                            Layout.alignment: Qt.AlignHCenter
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: root.instrumentSelected
                                    ? ("image://instruments/instrum" + "%1").arg(currentInstrNum)
                                    : ""
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.maximumHeight: instrBriefText().length > 0 ? 120 : 0
                            visible: instrBriefText().length > 0
                            text: instrBriefText()
                            color: root.fotekBlue
                            font.pixelSize: 24
                            wrapMode: Text.WordWrap
                            elide: Text.ElideRight
                            maximumLineCount: 4
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignTop
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            text: qsTr("Рекомендуемый уровень")
                            horizontalAlignment: Text.AlignHCenter
                            color: uiMidGray
                            font.pixelSize: 24
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: false
                            Layout.preferredHeight: modeEditor.isEndo
                                                    ? recommendedEndoButtonHeight
                                                    : recommendedButtonHeight
                            Layout.minimumHeight: modeEditor.isEndo
                                                    ? recommendedEndoButtonHeight
                                                    : recommendedButtonHeight
                            spacing: 8

                            PowerRect {
                                id: lowPowerButton
                                Layout.fillWidth: true
                                Layout.preferredHeight: modeEditor.isEndo
                                                        ? recommendedEndoButtonHeight
                                                        : recommendedButtonHeight
                                Layout.minimumHeight: modeEditor.isEndo
                                                        ? recommendedEndoButtonHeight
                                                        : recommendedButtonHeight
                                borderColor: "#A5D6A7"
                                idleFillColor: "#F3F5F9"
                                idleTextColor: root.fotekBlue
                                power: modeEditor.lowPowerBound
                                selected: modeEditor.currentPower === power
                                isEndo: modeEditor.isEndo
                            }
                            PowerRect {
                                id: midPowerButton
                                Layout.fillWidth: true
                                Layout.preferredHeight: modeEditor.isEndo
                                                        ? recommendedEndoButtonHeight
                                                        : recommendedButtonHeight
                                Layout.minimumHeight: modeEditor.isEndo
                                                        ? recommendedEndoButtonHeight
                                                        : recommendedButtonHeight
                                borderColor: "#78D87C"
                                idleFillColor: "#F3F5F9"
                                idleTextColor: root.fotekBlue
                                power: modeEditor.midPowerBound
                                selected: modeEditor.currentPower === power
                                isEndo: modeEditor.isEndo
                            }
                            PowerRect {
                                id: highPowerButton
                                Layout.fillWidth: true
                                Layout.preferredHeight: modeEditor.isEndo
                                                        ? recommendedEndoButtonHeight
                                                        : recommendedButtonHeight
                                Layout.minimumHeight: modeEditor.isEndo
                                                        ? recommendedEndoButtonHeight
                                                        : recommendedButtonHeight
                                borderColor: "#51D456"
                                idleFillColor: "#F3F5F9"
                                idleTextColor: root.fotekBlue
                                power: modeEditor.highPowerBound
                                selected: modeEditor.currentPower === power
                                isEndo: modeEditor.isEndo
                            }
                        }
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
                width: 520
                height: 62
                visible: root.instrumentSelected
                text: qsTr("Другой инструмент")
                secondaryColor: "white"
                secondaryBorderWidth: 2
                secondaryBorderColor: root.fotekBlue
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: root.fotekBlue
                onPressed: deselectInstrument()
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
        target: highPowerButton
        function onPowerChosen() {
            modeEditor.updateParameter("currentpower", highPowerButton.power)
        }
    }
    Connections {
        target: midPowerButton
        function onPowerChosen() {
            modeEditor.updateParameter("currentpower", midPowerButton.power)
        }
    }
    Connections {
        target: lowPowerButton
        function onPowerChosen() {
            modeEditor.updateParameter("currentpower", lowPowerButton.power)
        }
    }

    Connections {
        target: instrumListView
        function onNewIndexSelected(index) {
            if (openingInProgress)
                return
            modeEditor.currentInstrIndex = index
            // Явно, а не через onPowerChanged у PowerRect: иначе клик по low/high
            // мог тут же затираться повторным mid, а initialize() с закрытым пикером
            // затирал сохранённую мощность.
            applyDefaultRecommendedPower()
        }
    }
}
