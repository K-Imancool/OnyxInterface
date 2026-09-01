import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import BackEnd 1.0

Drawer {
    id: repeatRoot

    property int socketId: 0
    property var innerModel
    property int pedalUiRev: 0
    readonly property real titleHeight: Math.max(18, Math.min(40, height * 0.22))
    readonly property color drawerGray: "#6a6a6a"

    modal: false
    closePolicy: Popup.NoAutoClose

    onOpened: pedalUiRev++
    onSocketIdChanged: pedalUiRev++

    Connections {
        enabled: repeatRoot.opened && innerModel
        target: innerModel
        function onDataChanged(topLeft, bottomRight, roles) {
            if (socketId < topLeft.row || socketId > bottomRight.row)
                return
            pedalUiRev++
        }
        function onModelReset() {
            pedalUiRev++
        }
        function onSubProgIdxChanged() {
            pedalUiRev++
        }
    }

    function currentShownPedals() {
        if (!innerModel || socketId < 0) {
            return []
        }

        var socketIndex = innerModel.index(socketId, 0)
        return socketIndex.valid ? innerModel.data(socketIndex, SocketModel.SocketAllowedPedal) || [] : []
    }

    function currentSelectedPed() {
        if (!innerModel || socketId < 0) {
            return 0
        }

        var socketIndex = innerModel.index(socketId, 0)
        return socketIndex.valid ? innerModel.data(socketIndex, SocketModel.SocketPedal) || 0 : 0
    }

    background: Rectangle {
        color: repeatRoot.drawerGray
    }

    interactive: false

    function socketName() {
        if (!innerModel || socketId < 0) {
            return ""
        }

        var socketIndex = innerModel.index(socketId, 0)
        if (!socketIndex.valid) {
            return ""
        }

        var rawName = innerModel.data(socketIndex, SocketModel.SocketName) || ""
        return String(rawName).replace(/\s+/g, "")
    }

    Rectangle {
        anchors.fill: parent
        color: repeatRoot.drawerGray
        z: -1
        
        MouseArea {
            anchors.fill: parent
            onPressed: mouse.accepted = true
            onReleased: mouse.accepted = true
            onPositionChanged: mouse.accepted = true
            onDoubleClicked: mouse.accepted = true
        }
    }
    
    Rectangle {
        id: titleItem
        height: repeatRoot.titleHeight
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        color: "transparent"
        Label {
            anchors.fill: parent
            text: repeatRoot.socketName() + ":    " + qsTr("ВЫБЕРИТЕ УСТРОЙСТВО АКТИВАЦИИ")
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            wrapMode: Text.WordWrap
            font.bold: true
            font.pixelSize: Math.max(14, Math.min(20, titleItem.height * 0.55))
            color: "white"
        }
    }

    Flickable {
        id: editorViewport
        anchors.top: titleItem.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        clip: true
        contentWidth: pedEditor.width
        contentHeight: pedEditor.height

        PedalEditor {
            id: pedEditor
            socketNumber: repeatRoot.socketId
            width: Math.max(editorViewport.width, 560)
            height: Math.max(editorViewport.height, 100)
            shownPedalsArray: {
                var _ = repeatRoot.pedalUiRev
                return repeatRoot.currentShownPedals()
            }
            selectedPed: {
                var _ = repeatRoot.pedalUiRev
                return repeatRoot.currentSelectedPed()
            }
        }

        Connections {
            target: pedEditor
            function onPedSelected(pedal) {
                innerModel.qmlSetData(socketId, pedal, "socketpedal")
                recomHandle.saveCurrentState()
                repeatRoot.close()
            }
        }
    }
    
    MouseArea {
        id: swipeCloseArea
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(48, parent.width * 0.16)
        z: 1000
        enabled: repeatRoot.opened
        
        property real startX: 0
        property real startY: 0
        property bool isSwipeGesture: false
        property real minSwipeDistance: 50
        
        onPressed: {
            startX = mouse.x
            startY = mouse.y
            isSwipeGesture = false
            mouse.accepted = true
        }
        
        onPositionChanged: {
            if (pressed) {
                var deltaX = mouse.x - startX
                var deltaY = Math.abs(mouse.y - startY)
                
                if (deltaX > 50 && deltaX > deltaY * 2 && !isSwipeGesture) {
                    isSwipeGesture = true
                }
            }
        }
        
        onReleased: {
            var deltaX = mouse.x - startX
            var deltaY = Math.abs(mouse.y - startY)
            
            if (isSwipeGesture && deltaX > minSwipeDistance) {
                repeatRoot.close()
            }
            isSwipeGesture = false
        }
    }
}
