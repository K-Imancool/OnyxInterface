import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: neutralEl
    color: "transparent"

    // Маркер для NeutralDrawer: интерактивные элементы получают события напрямую
    property bool hasInteractiveContent: true

    // Свойства компонента
    property int neutralSize: periphHandle ? periphHandle.neutralSize : 2      // 0 = Small, 1 = Medium, 2 = Large
    // property bool neutralDivided: true  // НЭ разделённый или нет
    property bool neutralDivided: periphHandle ? periphHandle.neutralElDivided : true  // НЭ разделённый или нет - привязка к ControlCenter

    // property bool neutralConnected: false  // Передается снаружи
    property bool neutralConnected: periphHandle.neutralElConnected  // Передается снаружи
    property bool showControls: false      // Показывать ли кнопки управления

    readonly property string neIconsBasePath: AppPaths.neIconsBaseUrl

    // Сигналы для синхронизации с PeriphHandler
    // Используем другие имена, чтобы не конфликтовать с автоматическими сигналами свойств
    signal neutralDividedToggled(bool divided)
    signal neutralSizeSelected(int size)

    component MassSelectionBut: Rectangle {
        id: rootCustomBut
        required property int type
        property string line1Text: ""
        property string powerValueText: ""
        property string iconSource: ""
        property bool pressed: mouseArea.pressed

        readonly property real labelFontSize: Math.min(height / 4, width / 12)

        signal clicked()

        height: parent.height * .30
        width: parent.width * .7  // Уменьшена ширина, чтобы не перекрывать кнопки типа слева
        radius: 10

        color: neutralSize === type ? fotekOrange : "lightgray"
        border {
            color: neutralSize === type ? fotekBlue : "transparent"
            width: neutralSize === type ? 5 : 0
        }

        Rectangle {
            id: darker
            anchors.fill: parent
            color: "black"
            opacity: rootCustomBut.pressed ? 0.2 : 0
            radius: rootCustomBut.radius
        }

        Image {
            id: iconImage
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
                rightMargin: 8
                topMargin: 6
                bottomMargin: 6
            }
            source: rootCustomBut.iconSource
            fillMode: Image.PreserveAspectFit
            visible: rootCustomBut.iconSource !== ""
        }

//        Column {
//            id: textColumn
//            anchors {
//                left: parent.left
//                right: iconImage.left
//                verticalCenter: parent.verticalCenter
//                leftMargin: 8
//                rightMargin: 8
//            }
//            spacing: 0

            Text {
//                width: textColumn.width
                text: qsTr("Пациент:")
                textFormat: Text.StyledText
                font.pixelSize: rootCustomBut.labelFontSize
                color: "#2c2c2c"
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.0
                lineHeightMode: Text.ProportionalHeight
                anchors {
                    left: parent.left
                    top: parent.top
                    margins: 8
                }
            }

            Text {
//                width: textColumn.width
                text: "<b>" + rootCustomBut.line1Text + "</b>"
                textFormat: Text.StyledText
                font.pixelSize: rootCustomBut.labelFontSize
                color: "#2c2c2c"
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.2
                lineHeightMode: Text.ProportionalHeight
                anchors {
                    right: parent.right
                    top: parent.top
                    topMargin: 8
                    rightMargin: 70
                }
            }

            Text {
//                width: textColumn.width
                text: qsTr("Максимальная\nмощность:")
                font.pixelSize: rootCustomBut.labelFontSize
                color: "#2c2c2c"
                horizontalAlignment: Text.AlignLeft
                lineHeight: 1.0
                lineHeightMode: Text.ProportionalHeight
                visible: rootCustomBut.type != 2
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    margins: 8
                }
            }

            Text {
//                width: textColumn.width
                text: "<b>" + rootCustomBut.powerValueText + "</b>"
                textFormat: Text.StyledText
                font.pixelSize: rootCustomBut.labelFontSize
                color: "#2c2c2c"
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.0
                lineHeightMode: Text.ProportionalHeight
                visible: rootCustomBut.type != 2
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    bottomMargin: 8
                    rightMargin: 70
                }
            }
//        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            onClicked: {
                rootCustomBut.clicked()
                if (neutralSize !== type) {
                    neutralSize = type
                    neutralSizeSelected(type)
                }
            }
        }
    }

    Connections {
        target: buttonDivided
        function onClicked() {
            if (neutralDivided !== true) {
                neutralDivided = true
                neutralDividedToggled(true)
            }
        }
    }
    Connections {
        target: buttonNotDivided
        function onClicked() {
            if (neutralDivided !== false) {
                neutralDivided = false
                neutralDividedToggled(false)
            }
        }
    }

    NeutralButton {
        id: neutralImage
        visible: !showControls
        borderColor: fotekBlue
        borderWidth: 3
        divided: neutralDivided
        neutColor: neutralConnected ? "green" : "red"
        theColor: neutralConnected ? "lightgray" : "white"

        anchors.fill: parent
        button: false
        innerTextFontSize: 24
//        innerTextFontSize: showControls ? 18 : 14  // Меньший шрифт в компактном режиме (PeripheryPanel)
        innerText: {
            if (neutralSize === 0)
                qsTr("< 5кг\nМакс.50")
            else if (neutralSize === 1)
                qsTr("5-15кг\nМакс.75")
            else if (neutralSize === 2)
                qsTr("> 15кг\nМакс.400")
        }
    }


    // Контейнер для кнопок управления (только когда showControls = true)
    Rectangle {
        id: neutralControlContainer
        anchors {
            left: parent.left
            bottom: parent.bottom
            right: parent.right
            leftMargin: 5
            rightMargin: 5
        }
        height: parent.height
        color: "transparent"
        radius: 10
        border.color: fotekBlue
        border.width: 2
        visible: showControls

        Canvas {
            id: selectionConnector
            anchors.fill: parent
            z: 0

            readonly property int connectorLineWidth: 5
            readonly property Item typeButton: neutralDivided ? buttonDivided : buttonNotDivided
            readonly property Item massButton: neutralSize === 0 ? smallNeutralSize
                                                   : neutralSize === 1 ? mediumNeutralSize
                                                   : largeNeutralSize
            readonly property real geometryRevision: typeButton.x + typeButton.y + typeButton.width + typeButton.height
                                                   + massButton.x + massButton.y + massButton.width + massButton.height
                                                   + neutralSize + (neutralDivided ? 1 : 0)

            onGeometryRevisionChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            Component.onCompleted: requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                var start = mapFromItem(typeButton, typeButton.width, typeButton.height / 2)
                var end = mapFromItem(massButton, 0, massButton.height / 2)
                var cornerX = start.x + (end.x - start.x) / 2

                ctx.strokeStyle = fotekBlue
                ctx.lineWidth = connectorLineWidth
                ctx.lineCap = "butt"
                ctx.lineJoin = "miter"

                ctx.beginPath()
                ctx.moveTo(start.x, start.y)
                ctx.lineTo(cornerX, start.y)
                ctx.lineTo(cornerX, end.y)
                ctx.lineTo(end.x, end.y)
                ctx.stroke()
            }
        }

        NeutralButton {
            id: buttonDivided
            height: parent.height * .45
            width: parent.width * .24
            borderColor: neutralDivided ? fotekBlue : "transparent"
            borderWidth: neutralDivided ? 5 : 0
            divided: true
            neutColor: "green"
            theColor: !neutralDivided ? "lightgray" : fotekOrange
            anchors {
                top: parent.top
                left: parent.left
                leftMargin: 10
                topMargin: 10
            }
        }
        NeutralButton {
            id: buttonNotDivided
            height: parent.height * .45
            width: parent.width * .24
            borderColor: !neutralDivided ? fotekBlue : "transparent"
            borderWidth: !neutralDivided ? 5 : 0
            divided: false
            neutColor: "green"
            theColor: neutralDivided ? "lightgray" : fotekOrange
            anchors {
                bottom: parent.bottom
                left: parent.left
                leftMargin: 10
                bottomMargin: 10
            }
        }
        MassSelectionBut {
            id: largeNeutralSize
            type: 2
            line1Text: qsTr("&gt; 15 кг")
//            line1Text: qsTr("<b>Взрослый</b>: &gt; 15 кг")
            powerValueText: "400"
            iconSource: neutralEl.neIconsBasePath + "NE_adult.png"
            anchors {
                top: parent.top
                right: parent.right
                rightMargin: 10
                topMargin: 10
            }
        }
        MassSelectionBut {
            id: mediumNeutralSize
            type: 1
            line1Text: qsTr("5-15 кг")
//            line1Text: qsTr("<b>Ребёнок</b>: 5-15 кг")
            powerValueText: "75"
            iconSource: neutralEl.neIconsBasePath + "NE_kid.png"
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: 10
            }
        }
        MassSelectionBut {
            id: smallNeutralSize
            type: 0
            line1Text: qsTr("&lt; 5 кг")
//            line1Text: qsTr("<b>Младенец</b>: &lt; 5 кг")
            powerValueText: "50"
            iconSource: neutralEl.neIconsBasePath + "NE_baby.png"
            anchors {
                bottom: parent.bottom
                right: parent.right
                rightMargin: 10
                bottomMargin: 10
            }
        }
    }
}
