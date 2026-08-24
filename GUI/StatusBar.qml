import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: statusRoot
    signal drawerCalled()
    signal saveCalled()
    signal programTitlePressed()
    property alias text: mainText.text
    property alias versionText: appVersionText.text
    property bool saveHighlighted: false
    property int saveButtonWidth: 100
    readonly property int sideSpacing: 12
    readonly property int leftOccupiedWidth: drawerButton.width + sideSpacing
    readonly property int rightOccupiedWidth: saveButton.width
                                            + (appVersionText.visible ? appVersionText.width + sideSpacing : 0)
                                            + sideSpacing
    readonly property int titleSideMargin: Math.max(leftOccupiedWidth, rightOccupiedWidth)
    readonly property int titleLargeFontSize: 50
    readonly property int titleCompactFontSize: 36
    readonly property bool useCompactTitle: mainTextLargeMetrics.width > titleViewport.width

    border {
        width: 1
        color: "black"
    }

    Button {
        id: drawerButton
        flat: true
        height: Math.round(parent.height * 1.30)
        width: height
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        scale: 1.12
        contentItem: Text {
            text: "☰"
            font.pixelSize: Math.round(drawerButton.height * 0.45)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: "#111111"
        }
        background: Item {}
        onClicked: statusRoot.drawerCalled()
    }
    Flickable {
        id: titleViewport
        height: parent.height
        anchors.left: parent.left
        anchors.leftMargin: titleSideMargin
        anchors.right: parent.right
        anchors.rightMargin: titleSideMargin
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: needScroll ? mainText.paintedWidth : width
        contentHeight: height

        readonly property real metricsTextWidth: Math.ceil(titleScrollMetrics.width)
        readonly property real textRenderWidth: needScroll ? Math.ceil(contentWidth) : metricsTextWidth
        readonly property real overflowWidth: Math.max(0, textRenderWidth - width)
        readonly property real endRevealPadding: 0
        readonly property real maxScrollX: Math.max(0, overflowWidth + endRevealPadding)
        readonly property real scrollDistance: maxScrollX
        readonly property bool needScroll: metricsTextWidth > width + 1 && mainText.text !== ""

        onWidthChanged: contentX = 0
        onNeedScrollChanged: contentX = 0
        onContentXChanged: {
            if (contentX < 0) {
                contentX = 0
            } else if (contentX > maxScrollX) {
                contentX = maxScrollX
            }
        }

        Text {
            id: mainText
            y: Math.round((titleViewport.height - height) / 2)
            x: 0
            width: titleViewport.needScroll ? titleViewport.metricsTextWidth : titleViewport.width
            horizontalAlignment: titleViewport.needScroll ? Text.AlignLeft : Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: statusRoot.useCompactTitle
                            ? statusRoot.titleCompactFontSize
                            : statusRoot.titleLargeFontSize
            wrapMode: Text.NoWrap
            maximumLineCount: 1
            elide: Text.ElideNone

            onTextChanged: {
                titleViewport.contentX = 0
            }
            onFontChanged: titleViewport.contentX = 0

        }

        SequentialAnimation on contentX {
            running: titleViewport.needScroll
            loops: Animation.Infinite

            PauseAnimation { duration: 2000 }
            NumberAnimation {
                from: 0
                to: titleViewport.scrollDistance
                duration: Math.max(1200, titleViewport.scrollDistance * 8)
                easing.type: Easing.Linear
            }
            PauseAnimation { duration: 800 }
            PropertyAction { target: titleViewport; property: "contentX"; value: 0 }
        }
    }

    Text {
        id: mainTextMeasure
        visible: false
        text: mainText.text
        font.pixelSize: statusRoot.useCompactTitle
                        ? statusRoot.titleCompactFontSize
                        : statusRoot.titleLargeFontSize
    }
    MouseArea {
        anchors.fill: titleViewport
        onClicked: statusRoot.programTitlePressed()
    }
    TextMetrics {
        id: mainTextLargeMetrics
        text: mainTextMeasure.text
        font.pixelSize: statusRoot.titleLargeFontSize
    }
    TextMetrics {
        id: titleScrollMetrics
        text: mainText.text
        font.pixelSize: statusRoot.useCompactTitle
                        ? statusRoot.titleCompactFontSize
                        : statusRoot.titleLargeFontSize
    }
    Text {
        id: appVersionText
        height: parent.height
        width: visible ? Math.min(implicitWidth, statusRoot.width * 0.25) : 0
        anchors.right: saveButton.left
        anchors.rightMargin: sideSpacing
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        text: ""
        visible: text !== ""
    }
    Button {
        id: saveButton
        flat: true
        height: Math.round(parent.height * 1.50)
        width: statusRoot.saveButtonWidth
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        scale: 1.12
        contentItem: Text {
            text: "💾"
            font.pixelSize: Math.round(saveButton.height * 0.4)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Item {}
        onClicked: statusRoot.saveCalled()

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 10
            color: "#70F870"
            opacity: statusRoot.saveHighlighted ? 0.95 : 0.0
            z: -1
        }
    }

}
