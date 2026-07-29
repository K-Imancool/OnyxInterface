import QtQuick 2.15
Rectangle {
    id: pedContainer

    property var innerModel
    signal pedMenuRequest(int socketId)
    readonly property int pedalSideMargin: 8
    /** Как в SocketContainerV2: anchors.margins у ColumnLayout */
    readonly property int layoutMargins: 5
    /** Как в SocketContainerV2: spacing между progPage и SocketRepeater */
    readonly property int columnSpacing: 10
    /** Высота progPage в не-эндо (эндо — 0, блок скрыт) */
    readonly property int progPageHeight: (innerModel && innerModel.endoProgramView) ? 0 : 50
    /**
     * Верх первой строки сокетов (как у ColumnLayout в SocketContainerV2).
     * В эндо progPage с visible: false не участвует в разметке — между колонкой и сокетами
     * нет spacing 10 px, только верхний margin 5.
     */
    readonly property int socketTopOffset: (innerModel && innerModel.endoProgramView)
            ? layoutMargins
            : (layoutMargins + progPageHeight + columnSpacing)
    readonly property int socketSpacing: 10
    color: "#2c2c2c"
    clip: true

    function displaySocketName(name) {
        if (!name) {
            return ""
        }

        return String(name).replace(/\s+/g, "")
    }

    function uniformSocketHeight() {
        var socketCount = innerModel ? innerModel.rowCount() : 0
        if (socketCount <= 0) {
            return 0
        }
        // Как SocketContainerV2 → SocketRepeater:
        // layout.height = высота ColumnLayout (у панели сокетов минус верхний anchor 5).
        // containerHeight = layout.height - layout.spacing - progPage.height
        // В SocketRepeater: containerMargins = layout.anchors.margins — при раздельных
        // top/bottom margin это обычно 0, тогда в calculateCollapsedHeight не вычитается 2*margins.
        var layoutInnerHeight = height - layoutMargins
        var containerHeight = layoutInnerHeight - columnSpacing - progPageHeight
        return (containerHeight - socketCount * socketSpacing) / socketCount
    }

    function socketHeight(socketId) {
        var socketCount = innerModel ? innerModel.rowCount() : 0
        if (!innerModel || socketId < 0 || socketId >= socketCount) {
            return 0
        }
        return uniformSocketHeight()
    }

    function socketTop(socketId) {
        var top = socketTopOffset
        for (var i = 0; i < socketId; ++i) {
            top += socketHeight(i) + socketSpacing
        }
        return top
    }

    Repeater {
        id: repeat
        model: innerModel

        delegate: Item {
            id: socketPedalSlot
            readonly property bool endoDimmed: {
                if (!theModel || !theModel.endoProgramView) {
                    return false
                }
                if (theModel.subProgIdx === 0) {
                    return index < 2
                }
                if (theModel.subProgIdx === 1) {
                    return index >= 2
                }
                return false
            }
            visible: pedContainer.socketHeight(index) > 0 && !endoDimmed
            x: 0
            y: pedContainer.socketTop(index)
            width: pedContainer.width
            height: pedContainer.socketHeight(index)

            Rectangle {
                width: parent.width
                height: 2
                color: "lightgray"
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: -3
                anchors.leftMargin: 8
                anchors.rightMargin: 8
            }

            Text {
                id: socketNameLabel
                width: Math.min(parent.width - 8, pedIcon.width + 60)
                height: Math.min(implicitHeight, Math.max(0, pedIcon.y - 6))
                anchors.top: parent.top
                anchors.topMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                color: "white"
                font.pixelSize: 22
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                wrapMode: Text.WordWrap
                text: pedContainer.displaySocketName(model.socketname)
                clip: true
            }

            Pedal {
                id: pedIcon
                width: Math.max(0, Math.min(parent.width - pedContainer.pedalSideMargin * 2, parent.height))
                height: width
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
                pedalStateIdx: model.socketpedal
                socketId: index
            }

            Text {
                id: emptyPedalLabel
                text: qsTr("выбор педали")
                anchors.fill: pedIcon
                color: "white"
                font.pixelSize: 18
//                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                visible: pedIcon.state === "empty"
            }

            Connections {
                target: pedIcon
                function onPedalMenuRequest() {
                    pedContainer.pedMenuRequest(index)
                }
            }
        }
    }
}
