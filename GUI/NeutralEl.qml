import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: neutralEl
    radius: 8
    color: "green"
    border {
        color: "black"
        width: 1
    }

    // Свойства компонента
    property int neutralSize: 0      // 0 = Small, 1 = Medium, 2 = Large
    property int neutralType: 0      // 0 = Type A, 1 = Type B
    property bool neutralConnected: false  // Передается снаружи
    property bool showControls: false      // Показывать ли кнопки управления

    // Контейнер для изображения
    Rectangle {
        id: neutralImage
        anchors.centerIn: parent
        width: getImageWidth()
        height: getImageHeight()
        color: "transparent"

        // Type A - один прямоугольник в центре
        Rectangle {
            id: typeAIndicator
            anchors.centerIn: parent
            color: neutralConnected ? "green" : "red"
            width: getTypeASize()
            height: getTypeASize()
            visible: neutralType === 0
        }

        // Type B - два прямоугольника по бокам
        Rectangle {
            id: typeBLeft
            anchors {
                left: parent.left
                leftMargin: 5
                verticalCenter: parent.verticalCenter
            }
            color: neutralConnected ? "green" : "red"
            width: 20
            height: getTypeBSize()
            visible: neutralType === 1
        }

        Rectangle {
            id: typeBRight
            anchors {
                right: parent.right
                rightMargin: 5
                verticalCenter: parent.verticalCenter
            }
            color: neutralConnected ? "green" : "red"
            width: 20
            height: getTypeBSize()
            visible: neutralType === 1
        }
    }

    // Fallback текст
    Text {
        anchors.centerIn: parent
        text: getNeutralText()
        font.pixelSize: getTextSize()
        color: "white"
        visible: false  // Скрываем, так как используем графические индикаторы
    }

    // Кнопки управления (только когда showControls = true)
    Column {
        anchors {
            top: parent.top
            right: parent.right
            topMargin: 5
            rightMargin: 5
        }
        spacing: 5
        visible: showControls

        // Кнопка выбора типа
        Button {
            text: "Type " + (neutralType === 0 ? "A" : "B")
            width: 60
            height: 25
            font.pixelSize: 10
            onClicked: neutralType = (neutralType + 1) % 2
        }

        // Кнопка выбора размера
        Button {
            text: "Size " + (neutralSize === 0 ? "S" : (neutralSize === 1 ? "M" : "L"))
            width: 60
            height: 25
            font.pixelSize: 10
            onClicked: neutralSize = (neutralSize + 1) % 3
        }
    }

    // Индикатор подключения
    Rectangle {
        width: 12
        height: 12
        radius: 6
        color: neutralConnected ? "green" : "red"
        anchors {
            top: parent.top
            left: parent.left
            topMargin: 5
            leftMargin: 5
        }
        border {
            color: "white"
            width: 1
        }
    }

    // Функции
    function getImageWidth() {
        return showControls ? 80 : 25
    }

    function getImageHeight() {
        return showControls ? 80 : 25
    }

    function getTextSize() {
        return showControls ? 16 : 12
    }

    function getTypeASize() {
        switch(neutralSize) {
            case 0: return 40  // Small
            case 1: return 30  // Medium
            case 2: return 20  // Large
            default: return 30
        }
    }

    function getTypeBSize() {
        switch(neutralSize) {
            case 0: return 40  // Small
            case 1: return 30  // Medium
            case 2: return 20  // Large
            default: return 30
        }
    }

    function getNeutralText() {
        var typeText = neutralType === 0 ? "A" : "B"
        var sizeText = neutralSize === 0 ? "S" : (neutralSize === 1 ? "M" : "L")
        var connText = neutralConnected ? "C" : "D"
        return typeText + sizeText + connText
    }

    // Обработчики изменений
    onNeutralTypeChanged: {
        console.log("Neutral type changed to:", neutralType)
    }

    onNeutralSizeChanged: {
        console.log("Neutral size changed to:", neutralSize)
    }
}
