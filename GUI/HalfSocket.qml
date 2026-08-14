import QtQuick 2.15
import QtQuick.Controls 2.15
import BackEnd 1.0

Rectangle {
    id: halfSocketRoot
    color: "transparent"

    property bool isCoag
    property string modeName
    property int modePower
    property int modeId
    property int maxPower
    property int socketId
    property string socketName: ""
    property int instrumNum
    property string instrumName: qsTr("Другой инструмент")
    property bool isEndo: false
    property bool hasAvailableModes: true

    readonly property color socketColor: {
        if (modeId === ESHF.NO_MODE) {
            return "darkgray"
        }
        return isCoag ? "blue" : "yellow"
    }
    readonly property bool hasInstrImage: modeId !== ESHF.NO_MODE
                                          && instrumNum > 0
                                          && instrumNum !== 1000
    readonly property int endoCutEffect: Math.floor(modePower / 10)
    readonly property int endoCoagEffect: modePower % 10
    readonly property int normalModeNameFont: 36
    readonly property int singleLineModeNameFont: 42
    readonly property int collapsedInstrBaseSize: 150
    readonly property int collapsedInstrMinSize: 56
    readonly property int cutLabelLeftInset: 8
    readonly property int cutLabelToImageGap: 6
    readonly property int instrImageRightInset: 8
    readonly property int coagLabelToImageGap: 6
    readonly property int coagLabelImageOverlap: 20
    readonly property int coagLabelRightInset: 8
    readonly property int coagImageLeftInset: 8
    // Реальная ширина подписи режима (см. anchors у modeLabel и collapsedInstrImage)
    readonly property int cutModeLabelMarginTotal: cutLabelLeftInset + cutLabelToImageGap + instrImageRightInset
    readonly property int coagModeLabelMarginTotal: coagImageLeftInset + coagLabelToImageGap + coagLabelRightInset
    readonly property int modeNameMeasurePadding: 8
    readonly property int cutModeNameWidth: Math.ceil(largeModeNameMetrics.width) + modeNameMeasurePadding
    readonly property int coagModeNameWidthLarge: Math.ceil(largeModeNameMetrics.width) + modeNameMeasurePadding
    readonly property int coagModeNameWidthNormal: Math.ceil(normalModeNameMetrics.width) + modeNameMeasurePadding
    readonly property int coagModeLabelAreaWidth: Math.max(0, halfSocketRoot.width
            - (halfSocketRoot.hasInstrImage ? collapsedInstrBaseSize : 0)
            - coagModeLabelMarginTotal)

    // Резание: уменьшаем только картинку, шрифт режима фиксированный (42)
    readonly property int collapsedInstrSize: {
        if (halfSocketRoot.isCoag || halfSocketRoot.modeId === ESHF.NO_MODE)
            return halfSocketRoot.hasInstrImage ? collapsedInstrBaseSize : 0
        if (!halfSocketRoot.hasInstrImage)
            return 0
        var w = halfSocketRoot.width
        if (w <= 0)
            return collapsedInstrBaseSize
        var availAtBase = w - collapsedInstrBaseSize - cutModeLabelMarginTotal
        if (cutModeNameWidth <= availAtBase)
            return collapsedInstrBaseSize
        var required = w - cutModeLabelMarginTotal - cutModeNameWidth
        return Math.max(collapsedInstrMinSize, Math.min(collapsedInstrBaseSize, required))
    }

    readonly property int modeNameFontSize: {
        if (modeId === ESHF.NO_MODE)
            return 24
        if (!isCoag)
            return singleLineModeNameFont
        return coagModeNameWidthLarge <= coagModeLabelAreaWidth
                ? singleLineModeNameFont
                : normalModeNameFont
    }

    readonly property bool cutModeNameSingleLine: !isCoag && modeId !== ESHF.NO_MODE
    readonly property string modeLabelText: {
        if (modeId !== ESHF.NO_MODE) {
            return modeName
        }
        var role = isCoag ? qsTr("коагуляция") : qsTr("резание")
        return socketName !== "" ? socketName + ": " + role : role
    }

    Canvas {
        id: socketBackground
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var radius = 20
            var x = 0
            var y = 0
            var w = width
            var h = height

            ctx.beginPath()

            if (halfSocketRoot.isCoag) {
                // Для коагуляции скругляем правые углы.
                ctx.moveTo(x, y)
                ctx.lineTo(w - radius, y)
                ctx.arcTo(w, y, w, radius, radius)
                ctx.lineTo(w, h - radius)
                ctx.arcTo(w, h, w - radius, h, radius)
                ctx.lineTo(x, h)
                ctx.lineTo(x, y)
            } else {
                // Для резания скругляем левые углы.
                ctx.moveTo(x + radius, y)
                ctx.lineTo(w, y)
                ctx.lineTo(w, h)
                ctx.lineTo(x + radius, h)
                ctx.arcTo(x, h, x, h - radius, radius)
                ctx.lineTo(x, radius)
                ctx.arcTo(x, y, x + radius, y, radius)
            }

            ctx.closePath()
            ctx.fillStyle = halfSocketRoot.socketColor
            ctx.fill()
        }
    }

    Connections {
        target: halfSocketRoot
        function onModeIdChanged() { socketBackground.requestPaint() }
        function onIsCoagChanged() { socketBackground.requestPaint() }
        function onWidthChanged() { socketBackground.requestPaint() }
        function onHeightChanged() { socketBackground.requestPaint() }
    }

    Image {
        id: collapsedInstrImage
        visible: halfSocketRoot.hasInstrImage
        asynchronous: true
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        width: collapsedInstrSize
        height: collapsedInstrSize
        source: "image://instruments/"
                + (halfSocketRoot.isCoag ? "coaginstr" : "cutinstr")
                + halfSocketRoot.instrumNum
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.left: halfSocketRoot.isCoag ? parent.left : undefined
        anchors.leftMargin: halfSocketRoot.isCoag ? coagImageLeftInset : undefined
        anchors.right: halfSocketRoot.isCoag ? undefined : parent.right
        anchors.rightMargin: halfSocketRoot.isCoag ? undefined : instrImageRightInset
    }

    Label {
        id: modeLabel
        visible: halfSocketRoot.hasAvailableModes
        text: halfSocketRoot.modeLabelText
        color: (!halfSocketRoot.isCoag || modeId === ESHF.NO_MODE) ? "black" : "white"
        font.pixelSize: halfSocketRoot.modeNameFontSize
        font.bold: true
        wrapMode: halfSocketRoot.cutModeNameSingleLine ? Text.NoWrap : Text.Wrap
        lineHeight: 0.82
        lineHeightMode: Text.ProportionalHeight
        maximumLineCount: halfSocketRoot.cutModeNameSingleLine ? 1 : 2
        elide: Text.ElideNone
        horizontalAlignment: halfSocketRoot.isCoag ? Text.AlignRight : Text.AlignLeft
        verticalAlignment: Text.AlignTop
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.left: halfSocketRoot.isCoag
                     ? (halfSocketRoot.hasInstrImage ? collapsedInstrImage.right : parent.left)
                     : parent.left
        anchors.leftMargin: halfSocketRoot.isCoag
                            ? (halfSocketRoot.hasInstrImage ? -coagLabelImageOverlap : coagImageLeftInset)
                            : cutLabelLeftInset
        anchors.right: halfSocketRoot.isCoag
                      ? parent.right
                      : (halfSocketRoot.hasInstrImage ? collapsedInstrImage.left : parent.right)
        anchors.rightMargin: halfSocketRoot.isCoag
                             ? coagLabelRightInset
                             : (halfSocketRoot.hasInstrImage ? cutLabelToImageGap : cutLabelLeftInset)
    }

    TextMetrics {
        id: largeModeNameMetrics
        text: halfSocketRoot.modeLabelText
        font.pixelSize: halfSocketRoot.singleLineModeNameFont
        font.bold: true
    }

    TextMetrics {
        id: normalModeNameMetrics
        text: halfSocketRoot.modeLabelText
        font.pixelSize: halfSocketRoot.normalModeNameFont
        font.bold: true
    }

    Label {
        id: powerLabel
        text: halfSocketRoot.modePower
        visible: !halfSocketRoot.isEndo && halfSocketRoot.modeId !== ESHF.NO_MODE
        color: halfSocketRoot.isCoag ? "white" : "black"
        font.pixelSize: 60
        font.bold: true
        horizontalAlignment: halfSocketRoot.isCoag ? Text.AlignRight : Text.AlignLeft
        verticalAlignment: Text.AlignBottom
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.left: halfSocketRoot.isCoag
                     ? (halfSocketRoot.hasInstrImage ? collapsedInstrImage.right : parent.left)
                     : parent.left
        anchors.leftMargin: halfSocketRoot.isCoag
                            ? (halfSocketRoot.hasInstrImage ? coagLabelToImageGap : coagImageLeftInset)
                            : cutLabelLeftInset
        anchors.right: halfSocketRoot.isCoag
                      ? parent.right
                      : (halfSocketRoot.hasInstrImage ? collapsedInstrImage.left : parent.right)
        anchors.rightMargin: halfSocketRoot.isCoag
                             ? coagLabelRightInset
                             : (halfSocketRoot.hasInstrImage ? cutLabelToImageGap : cutLabelLeftInset)
    }

    Row {
        id: powerEndoRow
        visible: halfSocketRoot.isEndo && halfSocketRoot.modeId !== ESHF.NO_MODE
        spacing: 18
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.left: halfSocketRoot.isCoag
                     ? (halfSocketRoot.hasInstrImage ? collapsedInstrImage.right : parent.left)
                     : parent.left
        anchors.leftMargin: halfSocketRoot.isCoag
                            ? (halfSocketRoot.hasInstrImage ? coagLabelToImageGap : coagImageLeftInset)
                            : cutLabelLeftInset
        anchors.right: halfSocketRoot.isCoag
                      ? parent.right
                      : (halfSocketRoot.hasInstrImage ? collapsedInstrImage.left : parent.right)
        anchors.rightMargin: halfSocketRoot.isCoag
                             ? coagLabelRightInset
                             : (halfSocketRoot.hasInstrImage ? cutLabelToImageGap : cutLabelLeftInset)

        Row {
            spacing: 6

            Text {
                text: qsTr("эфф.\nрез.")
                color: halfSocketRoot.isCoag ? "white" : "black"
                font.pixelSize: 24
                font.bold: true
                width: implicitWidth
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                text: halfSocketRoot.endoCutEffect
                color: halfSocketRoot.isCoag ? "white" : "black"
                font.pixelSize: 60
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }
        }

        Row {
            spacing: 6

            Text {
                text: qsTr("эфф.\nкоаг.")
                color: halfSocketRoot.isCoag ? "white" : "black"
                font.pixelSize: 24
                font.bold: true
                width: implicitWidth
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                text: halfSocketRoot.endoCoagEffect
                color: halfSocketRoot.isCoag ? "white" : "black"
                font.pixelSize: 60
                font.bold: true
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
