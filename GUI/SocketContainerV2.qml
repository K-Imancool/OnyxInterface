import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Rectangle {
    id: socketContainer

    signal progAddRequest(int type)
    property var innerModel
    property alias socketEditorOpened: socketEditor.opened
    property alias fullSocketEditorOpened: fullSocketEditor.opened
    property var activationOverlay: null
    property bool activationUiAllowed: true
    property var onActivationEnableRequested: null

    function requestActivationEnable() {
        if (onActivationEnableRequested)
            onActivationEnableRequested()
        else
            periphHandle.enableActivation = true
    }

    color: "gray"

    property bool monoSprayM1M2Active: false

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
                            socketContainer.requestActivationEnable()
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
            activationOverlay: socketContainer.activationOverlay
            activationUiAllowed: socketContainer.activationUiAllowed
        }
        Item {
            Layout.fillHeight: true
        }
    }

    // Плашка СПРЕЙ М1+М2
    Item {
        id: sprayM1M2Overlay
        z: 20
        visible: false
        clip: true

        readonly property color coagBlue: "blue"
        readonly property int cornerRadius: 20
        readonly property int instrBaseSize: 150
        readonly property int coagImageLeftInset: 8
        readonly property int coagLabelRightInset: 8
        readonly property int coagLabelToImageGap: 6
        readonly property int coagLabelImageOverlap: 20

        property var mono1Item: null
        property var mono2Item: null

        readonly property string modeName: mono1Item ? mono1Item.coagModeName : ""
        readonly property int modePower: mono1Item ? mono1Item.coagModePower : 0
        readonly property int instrumNum: mono1Item ? mono1Item.coagInstrumNum : 0
        readonly property bool hasInstrImage: instrumNum > 0 && instrumNum !== 1000

        function socketsVisibleForOverlay() {
            if (!mono1Item || !mono2Item)
                return false
            if (mono1Item.dimmed || mono2Item.dimmed)
                return false
            return true
        }

        function syncGeometry() {
            mono1Item = repeat.itemAt(2)
            mono2Item = repeat.itemAt(3)
            if (!socketContainer.monoSprayM1M2Active || !socketsVisibleForOverlay()) {
                visible = false
                return
            }

            var p1 = mono1Item.mapToItem(socketContainer, 0, 0)
            var p2 = mono2Item.mapToItem(socketContainer, 0, 0)
            var coagLeft = Math.round(mono1Item.width / 2) + 1
            x = p1.x + coagLeft
            y = p1.y
            width = Math.max(0, mono1Item.width - coagLeft)
            height = Math.max(0, (p2.y + mono2Item.height) - p1.y)
            visible = width > 0 && height > 0
            bgCanvas.requestPaint()
        }

        Canvas {
            id: bgCanvas
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var radius = sprayM1M2Overlay.cornerRadius
                var w = width
                var h = height
                ctx.beginPath()
                ctx.moveTo(0, 0)
                ctx.lineTo(w - radius, 0)
                ctx.arcTo(w, 0, w, radius, radius)
                ctx.lineTo(w, h - radius)
                ctx.arcTo(w, h, w - radius, h, radius)
                ctx.lineTo(0, h)
                ctx.closePath()
                ctx.fillStyle = sprayM1M2Overlay.coagBlue
                ctx.fill()
            }
        }

        Item {
            id: mono1Zone
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: sprayM1M2Overlay.mono1Item ? sprayM1M2Overlay.mono1Item.height : parent.height / 2

            Image {
                id: overlayInstrImage
                visible: sprayM1M2Overlay.hasInstrImage
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                width: sprayM1M2Overlay.instrBaseSize
                height: sprayM1M2Overlay.instrBaseSize
                source: sprayM1M2Overlay.hasInstrImage
                        ? ("image://instruments/coaginstr" + sprayM1M2Overlay.instrumNum)
                        : ""
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                anchors.left: parent.left
                anchors.leftMargin: sprayM1M2Overlay.coagImageLeftInset
            }

            Label {
                id: overlayModeLabel
                text: sprayM1M2Overlay.modeName
                color: "white"
                font.pixelSize: 42
                font.bold: true
                wrapMode: Text.Wrap
                lineHeight: 0.82
                lineHeightMode: Text.ProportionalHeight
                maximumLineCount: 2
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignTop
                anchors.top: parent.top
                anchors.topMargin: 8
                anchors.left: sprayM1M2Overlay.hasInstrImage ? overlayInstrImage.right : parent.left
                anchors.leftMargin: sprayM1M2Overlay.hasInstrImage
                                    ? -sprayM1M2Overlay.coagLabelImageOverlap
                                    : sprayM1M2Overlay.coagImageLeftInset
                anchors.right: parent.right
                anchors.rightMargin: sprayM1M2Overlay.coagLabelRightInset
            }
        }

        Item {
            id: mono2Zone
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: sprayM1M2Overlay.mono2Item ? sprayM1M2Overlay.mono2Item.height : parent.height / 2

            Label {
                text: sprayM1M2Overlay.modePower
                color: "white"
                font.pixelSize: 60
                font.bold: true
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignBottom
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 5
                anchors.left: parent.left
                anchors.leftMargin: sprayM1M2Overlay.coagImageLeftInset
                anchors.right: parent.right
                anchors.rightMargin: sprayM1M2Overlay.coagLabelRightInset
            }
        }

        Rectangle {
            color: "darkgray"
            anchors.left: parent.left
//            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 150
            height: 8
        }

        Rectangle {
            color: "gray"
            anchors.right: parent.right
//            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 220
            height: 60
        }

        Label {
            text: "М1+М2"
            color: "white"
            font.pixelSize: 48
            font.bold: true
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                fullSocketEditor.socId = 2
                periphHandle.enableActivation = false
                fullSocketEditor.open()
            }
        }

        onWidthChanged: bgCanvas.requestPaint()
        onHeightChanged: bgCanvas.requestPaint()
    }

    Connections {
        target: periphHandle
        function onAutoModeChanged(socketId, mode) {
            socketContainer.monoSprayM1M2Active = (periphHandle.autoMode(2) === 3)
            Qt.callLater(sprayM1M2Overlay.syncGeometry)
        }
    }

    Connections {
        target: theModel
        function onDataChanged(topLeft, bottomRight, roles) {
            if (!socketContainer.monoSprayM1M2Active)
                return
            if (bottomRight.row < 2 || topLeft.row > 3)
                return
            Qt.callLater(sprayM1M2Overlay.syncGeometry)
        }
        function onSubProgIdxChanged() {
            Qt.callLater(sprayM1M2Overlay.syncGeometry)
        }
        function onEndoProgramViewChanged() {
            Qt.callLater(sprayM1M2Overlay.syncGeometry)
        }
    }

    Connections {
        target: layout
        function onWidthChanged() { Qt.callLater(sprayM1M2Overlay.syncGeometry) }
        function onHeightChanged() { Qt.callLater(sprayM1M2Overlay.syncGeometry) }
    }

    Connections {
        target: repeat
        function onCountChanged() { Qt.callLater(sprayM1M2Overlay.syncGeometry) }
        function onContainerHeightChanged() { Qt.callLater(sprayM1M2Overlay.syncGeometry) }
    }

    onWidthChanged: Qt.callLater(sprayM1M2Overlay.syncGeometry)
    onHeightChanged: Qt.callLater(sprayM1M2Overlay.syncGeometry)
    onMonoSprayM1M2ActiveChanged: {
        Qt.callLater(sprayM1M2Overlay.syncGeometry)
        if (monoSprayM1M2Active)
            sprayOverlayLayoutTimer.restart()
    }

    Timer {
        id: sprayOverlayLayoutTimer
        interval: 50
        repeat: false
        onTriggered: sprayM1M2Overlay.syncGeometry()
    }

    Component.onCompleted: {
        monoSprayM1M2Active = (periphHandle.autoMode(2) === 3)
        sprayOverlayLayoutTimer.start()
    }

    SocketEditor {
        id: socketEditor
    }

    ModeEditor {
        id: modePickerPopup
        deferCommit: true
        onClosed: fullSocketEditor.handleSubEditorClosed(dialogAccepted)
    }

    InstrumEditor {
        id: instrPickerPopup
        deferCommit: true
        onClosed: fullSocketEditor.handleSubEditorClosed(dialogAccepted)
    }

    FullSocketEditor {
        id: fullSocketEditor
        modePicker: modePickerPopup
        instrPicker: instrPickerPopup
    }
    ProgAdditionPop {
        id: progSelector
        width: socketContainer.width + 200
        height: socketContainer.height/2
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
            socketContainer.requestActivationEnable()
        }
    }
    Connections {
        target: socketEditor
        function onClosed() {
            socketContainer.requestActivationEnable()
        }
    }
    Connections {
        target: fullSocketEditor
        function onClosed() {
            socketContainer.requestActivationEnable()
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

