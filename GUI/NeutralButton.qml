import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: neutButRoot
    property color borderColor
    property int borderWidth
    property color neutColor
    property color theColor
    property color innerTextColor
    property alias innerText: optionalText.text
    property int innerTextFontSize: 14  // Размер шрифта для innerText
    property bool divided
    property bool button: true
    property bool pressed: mouseArea.pressed  // Для совместимости с Button API

    // Используем единый масштаб для сохранения пропорций (эскиз designW × designH)
    readonly property real designW: 24
    readonly property real designH: 33
    readonly property int bottomMargin: 10  // Отступ снизу
    // unitScale, не scale: у Item уже есть свойство scale (трансформация)
    // Масштаб надо делать целочисленным, иначе всё поедет
    // Значения задаются в updateLayoutMetrics() по размеру Canvas (после layout)
    property int unitScale: 1
    property int scaleX: 0
    property int scaleY: 0
    // Вычисляем смещения для центрирования по горизонтали и привязки к низу
    property int offsetX: 0
    property int offsetY: 0

    readonly property int topReservedHeight: optionalText.visible && optionalText.text.length > 0
            ? Math.ceil(optionalText.font.pixelSize * 2.6) + optionalText.anchors.topMargin + 8
            : 8

    function updateLayoutMetrics(canvasWidth, canvasHeight) {
        var layoutW = canvasWidth > 0 ? canvasWidth : width
        var layoutH = canvasHeight > 0 ? canvasHeight : height
        var drawableH = Math.max(0, layoutH - topReservedHeight - bottomMargin)

        var sx = layoutW / designW
        var sy = drawableH / designH
        scaleX = Math.floor(sx)
        scaleY = Math.floor(sy)

        // round: при ширине панели ~150 px (140 с margins) даёт 6, floor — только 5
        var s = (layoutW <= 0 || drawableH <= 0)
                ? 1
                : Math.max(1, Math.round(Math.min(sx, sy)))
        unitScale = s
        offsetX = Math.floor((layoutW - designW * s) / 2)
        offsetY = layoutH - Math.floor(designH * s) - bottomMargin
    }

    // Метрики нельзя писать из onPaint: смена unitScale/offset вызывает
    // requestPaint() посреди отрисовки, и Qt сбрасывает уже нарисованную
    // цветную подложку (зелёная/красная индикация подключения).
    onWidthChanged: updateLayoutMetrics(width, height)
    onHeightChanged: updateLayoutMetrics(width, height)
    onTopReservedHeightChanged: updateLayoutMetrics(width, height)
    Component.onCompleted: updateLayoutMetrics(width, height)

    signal clicked()

    color: theColor
    radius: 10
    border {
        color: borderColor
        width: borderWidth
    }

    Rectangle {
        id: contentRect
        color: "transparent"
        anchors {
            fill: parent
        }
        Canvas {
            id: neutralBack
            anchors.fill: parent

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.fillStyle = neutColor
                ctx.beginPath()

                var s = neutButRoot.unitScale
                var ox = neutButRoot.offsetX
                var oy = neutButRoot.offsetY

                // Начинаем с левой стороны
                ctx.moveTo(ox + s, oy + 2 * s)

                // Закругление в верхнем левом углу
                ctx.arcTo(ox + s, oy + s, ox + 2 * s, oy + s, s)
                ctx.lineTo(ox + 6 * s, oy + s)
                ctx.lineTo(ox + 6 * s, oy + 5 * s)
                ctx.arcTo(ox + 6 * s, oy + 6 * s, ox + 7 * s, oy + 6 * s, s)
                ctx.lineTo(ox + 17 * s, oy + 6 * s)
                ctx.arcTo(ox + 18 * s, oy + 6 * s, ox + 18 * s, oy + 5 * s, s)
                ctx.lineTo(ox + 18 * s, oy + s)
                ctx.lineTo(ox + 22 * s, oy + s)
                ctx.arcTo(ox + 23 * s, oy + s, ox + 23 * s, oy + 2 * s, s)
                ctx.lineTo(ox + 23 * s, oy + 31 * s)
                ctx.arcTo(ox + 23 * s, oy + 32 * s, ox + 22 * s, oy + 32 * s, s)
                ctx.lineTo(ox + 2 * s, oy + 32 * s)
                ctx.arcTo(ox + s, oy + 32 * s, ox + s, oy + 31 * s, s)

                // Замыкаем путь
                ctx.closePath()
                ctx.fill()
            }

            // Перерисовываем при изменении цвета или размеров
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        // Перерисовываем neutralBack при изменении цвета и раскладки
        Connections {
            target: neutButRoot
            function onNeutColorChanged() {
                neutralBack.requestPaint()
            }
            function onUnitScaleChanged() {
                neutralBack.requestPaint()
            }
            function onOffsetXChanged() {
                neutralBack.requestPaint()
            }
            function onOffsetYChanged() {
                neutralBack.requestPaint()
            }
            function onTopReservedHeightChanged() {
                neutralBack.requestPaint()
            }
        }

        Canvas {
            id: neutralBody
            anchors.fill: parent

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                var s = neutButRoot.unitScale
                var ox = neutButRoot.offsetX
                var oy = neutButRoot.offsetY

                // Создаём градиент, имитирующий серебро (вертикальный)
                var gradient = ctx.createLinearGradient(
                    ox + 12 * s, oy,
                    ox + 12 * s, oy + 31 * s
                )
                gradient.addColorStop(0, "#808080")    // Тёмно-серый
                gradient.addColorStop(0.15, "#c0c0c0")    // Светло-серый
                gradient.addColorStop(0.35, "#f0f0f0") // Почти белый (блик)
                gradient.addColorStop(0.6, "#a8a8a8")  // Средне-серый
                gradient.addColorStop(0.8, "#d0d0d0") // Серебристый
                gradient.addColorStop(1, "#b0b0b0")    // Тёмно-серый

                ctx.fillStyle = gradient
                ctx.beginPath()

                if (divided) {
                    // Левая половина
                    ctx.moveTo(ox + 8 * s, oy)
                    ctx.lineTo(ox + 11 * s, oy)
                    ctx.lineTo(ox + 11 * s, oy + 31 * s)
                    ctx.lineTo(ox + 3 * s, oy + 31 * s)
                    ctx.arcTo(ox + 2 * s, oy + 31 * s, ox + 2 * s, oy + 30 * s, s)
                    ctx.lineTo(ox + 2 * s, oy + 8 * s)
                    ctx.arcTo(ox + 2 * s, oy + 7 * s, ox + 3 * s, oy + 7 * s, s)
                    ctx.lineTo(ox + 7 * s, oy + 7 * s)
                    ctx.arcTo(ox + 8 * s, oy + 7 * s, ox + 8 * s, oy + 6 * s, s)

                    // Замыкаем путь
                    ctx.closePath()
                    ctx.fill()

                    // Правая половина
                    ctx.beginPath()
                    ctx.moveTo(ox + 13 * s, oy)
                    ctx.lineTo(ox + 16 * s, oy)
                    ctx.lineTo(ox + 16 * s, oy + 6 * s)
                    ctx.arcTo(ox + 16 * s, oy + 7 * s, ox + 17 * s, oy + 7 * s, s)
                    ctx.lineTo(ox + 21 * s, oy + 7 * s)
                    ctx.arcTo(ox + 22 * s, oy + 7 * s, ox + 22 * s, oy + 8 * s, s)
                    ctx.lineTo(ox + 22 * s, oy + 30 * s)
                    ctx.arcTo(ox + 22 * s, oy + 31 * s, ox + 21 * s, oy + 31 * s, s)
                    ctx.lineTo(ox + 13 * s, oy + 31 * s)
                } else {
                    // Неразделённый нейтральник
                    ctx.moveTo(ox + 10 * s, oy)
                    ctx.lineTo(ox + 14 * s, oy)
                    ctx.lineTo(ox + 14 * s, oy + 6 * s)
                    ctx.arcTo(ox + 14 * s, oy + 7 * s, ox + 15 * s, oy + 7 * s, s)
                    ctx.lineTo(ox + 21 * s, oy + 7 * s)
                    ctx.arcTo(ox + 22 * s, oy + 7 * s, ox + 22 * s, oy + 8 * s, s)
                    ctx.lineTo(ox + 22 * s, oy + 30 * s)
                    ctx.arcTo(ox + 22 * s, oy + 31 * s, ox + 21 * s, oy + 31 * s, s)
                    ctx.lineTo(ox + 3 * s, oy + 31 * s)
                    ctx.arcTo(ox + 2 * s, oy + 31 * s, ox + 2 * s, oy + 30 * s, s)
                    ctx.lineTo(ox + 2 * s, oy + 8 * s)
                    ctx.arcTo(ox + 2 * s, oy + 7 * s, ox + 3 * s, oy + 7 * s, s)
                    ctx.lineTo(ox + 9 * s, oy + 7 * s)
                    ctx.arcTo(ox + 10 * s, oy + 7 * s, ox + 10 * s, oy + 6 * s, s)
                }

                // Замыкаем путь
                ctx.closePath()
                ctx.fill()
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        // Перерисовываем Canvas при изменении типа электрода и раскладки
        Connections {
            target: neutButRoot
            function onDividedChanged() {
                neutralBody.requestPaint()
            }
            function onUnitScaleChanged() {
                neutralBody.requestPaint()
            }
            function onOffsetXChanged() {
                neutralBody.requestPaint()
            }
            function onOffsetYChanged() {
                neutralBody.requestPaint()
            }
            function onTopReservedHeightChanged() {
                neutralBody.requestPaint()
            }
        }

        Label {
            id: optionalText
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignTop
            visible: text.length != 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 7
            color: innerTextColor
            font.pixelSize: innerTextFontSize
            font.bold: true
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onPressed: neutButRoot.clicked()
    }
}
