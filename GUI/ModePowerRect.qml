import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

Canvas {
    id: modePowerRect

    property bool isCoag
    property string modeName
    property int modeId
    property int modePower
    property int maxPower

    signal modeEditDialogRequest()
    signal newPower(int pwr)

    function colorFromId() {
        if (modePowerRect.modeId == 1000)
            return "darkgray"
        return isCoag ? "blue" : "yellow"
    }

    function changePower(direction) {
        var changedPower = modePower
        if (direction === "up") {
            if (modePower < 20) changedPower += 1
            else if (modePower < 50) changedPower +=2
            else if (modePower < 100) changedPower +=5
            else if (modePower < 200) changedPower +=10
            else if (modePower < 400) changedPower +=25
        }
        if (direction === "down") {
            if (modePower <= 1) changedPower = 1
            else if (modePower <= 20) changedPower -= 1
            else if (modePower <= 50) changedPower -=2
            else if (modePower <= 100) changedPower -=5
            else if (modePower <= 200) changedPower -=10
            else if (modePower <= 400) changedPower -=25
        }
        return changedPower
    }

    // Функция для округления значения до ближайшего допустимого шага
    function roundToStep(value) {
        if (value <= 0) return 1
        
        if (value <= 20) {
            return Math.round(value) // шаг 1
        } else if (value <= 50) {
            return Math.round(value / 2) * 2 // шаг 2
        } else if (value <= 100) {
            return Math.round(value / 5) * 5 // шаг 5
        } else if (value <= 200) {
            return Math.round(value / 10) * 10 // шаг 10
        } else if (value <= 400) {
            return Math.round(value / 25) * 25 // шаг 25
        } else {
            return 1 // шаг 25
        }
    }

    onPaint: {
        var ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);

        var radius = 20;
        var x = 0;
        var y = 0;
        var w = width;
        var h = height;

        ctx.beginPath();

        if (isCoag) {
            if (parent.parent.state === "expanded") {
                // Правый сокет расширен - закруглен только нижний правый угол
                ctx.moveTo(x, y);
                ctx.lineTo(w, y);
                ctx.lineTo(w, h - radius);
                ctx.arcTo(w, h, w - radius, h, radius);
                ctx.lineTo(x, h);
                ctx.lineTo(x, y);
            }
            else {
                // Правый сокет сжат - закруглены правые углы
                ctx.moveTo(x, y);
                ctx.lineTo(w - radius, y);
                ctx.arcTo(w, y, w, radius, radius);
                ctx.lineTo(w, h - radius);
                ctx.arcTo(w, h, w - radius, h, radius);
                ctx.lineTo(x, h);
                ctx.lineTo(x, y);
            }
        } else {
            if (parent.parent.state === "expanded") {
                // Левый сокет расширен - закруглен только нижний левый угол
                ctx.moveTo(x, y);
                ctx.lineTo(w, y);
                ctx.lineTo(w, h);
                ctx.lineTo(x + radius, h);
                ctx.arcTo(x, h, x, h - radius, radius);
            }
            else {
                // Левый сокет сжат - закруглены левые углы
                ctx.moveTo(x + radius, y);
                ctx.lineTo(w, y);
                ctx.lineTo(w, h);
                ctx.lineTo(x + radius, h);
                ctx.arcTo(x, h, x, h - radius, radius);
                ctx.lineTo(x, radius);
                ctx.arcTo(x, y, x + radius, y, radius);
            }
        }

        ctx.closePath();
        ctx.fillStyle = colorFromId();
        ctx.fill();
    }

//    // Поведенческие анимации для плавных переходов
//    Behavior on height {
//        NumberAnimation {
//            duration: 400
//            easing.type: Easing.OutQuart
//        }
//    }

//    Behavior on width {
//        NumberAnimation {
//            duration: 400
//            easing.type: Easing.OutQuart
//        }
//    }

    Connections {
        target: modePowerRect
        function onModeIdChanged() {
            modePowerRect.color = modePowerRect.colorFromId()
            console.log(isCoag, modePowerRect.color, modePowerRect.modeId)
        }
    }

    Rectangle {
        id: mode
        color: "transparent"
        Label {
            id: modeLabel
            text: modeName
            font.pixelSize: 33
            font.bold: true
            wrapMode: Text.Wrap
            height: 60
            color: isCoag ? "white" : "black"
            verticalAlignment: Qt.AlignVCenter
            anchors {
                margins: 10
                fill: parent
            }

            Behavior on font.pixelSize {
                NumberAnimation {
                    duration: 400
                    easing.type: Easing.OutQuart
                }
            }
        }
        MouseArea {
            id: modeSelectButton
            anchors.fill: parent
            onClicked: modePowerRect.modeEditDialogRequest()
        }
    }

    Rectangle {
        id: power
        color: "transparent"
        Label {
            id: powerLabel
            text: modePower
            visible: (modeId != 1000)
            width: fontMetrics.advanceWidth("999")
            height: 31
            font.pixelSize: 40
            font.bold: true
            color: isCoag ? "white" : "black"
            verticalAlignment: Qt.AlignVCenter
            anchors {
                margins: 10
                fill: parent
            }
            // Поведенческая анимация для размера шрифта
            Behavior on font.pixelSize {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }

//            DropShadow {
//                anchors.fill: powerLabel
//                horizontalOffset: 2
//                verticalOffset: 2
//                radius: 4
//                samples: 9
//                color: "#80000000"  // Полупрозрачный черный
//                source: powerLabel
//            }

        }
        FontMetrics {
            id: fontMetrics
            font: powerLabel.font
        }
    }
    Rectangle {
        id: powerPlusButton
        width: 60
        height: 60
        color: "transparent"
        radius: 10
        opacity: 1
        border {
            color: modePowerRect.isCoag ? "white" : "color"
            width: 2
        }
        anchors.margins: 15
        Label {
            anchors {
                margins: 10
                fill: parent
            }
            color: modePowerRect.isCoag ? "white" : "color"
            font.pixelSize: 30
            font.bold: true
            text: "+"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }
        MouseArea {
            anchors.fill: parent
            onClicked: modePowerRect.newPower(changePower("up"));
        }
    }
    Rectangle {
        id: powerMinusButton
        width: 60
        height: 60
        color: "transparent"
        radius: 10
        opacity: 1
        border {
            color: modePowerRect.isCoag ? "white" : "color"
            width: 2
        }
        anchors.margins: 15
        Label {
            anchors {
                margins: 10
                fill: parent
            }
            color: modePowerRect.isCoag ? "white" : "color"
            font.pixelSize: 30
            font.bold: true
            text: "-"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
        }
        MouseArea {
            anchors.fill: parent
            onClicked: modePowerRect.newPower(changePower("down"));
        }
    }
    Slider {
        id: powerSlider
        value: modePower
        opacity: 1
        from: 1
        to: maxPower
        onValueChanged: {
            var roundedValue = roundToStep(value)
            if (roundedValue !== modePower) {
                modePowerRect.newPower(roundedValue)
            }
        }
    }
    states: [
        State {
            name: "collapsed"
            PropertyChanges { target: powerPlusButton;  visible: false }
            PropertyChanges { target: powerMinusButton; visible: false }
            PropertyChanges { target: powerSlider;      visible: false }
            PropertyChanges {
                target: powerLabel;
                horizontalAlignment: isCoag ? Text.AlignLeft : Text.AlignRight
                font.pixelSize: 50
            }
            PropertyChanges {
                target: modeLabel;
                horizontalAlignment: isCoag ? Text.AlignRight : Text.AlignLeft
                font.pixelSize: 25
            }
            PropertyChanges {
                target: power;
                anchors.topMargin: undefined
            }
            PropertyChanges {
                target: modeSelectButton;
                enabled: false
            }
            AnchorChanges {
                target: power
                anchors.left: {modePowerRect.isCoag ? modePowerRect.left : undefined}
                anchors.right: {modePowerRect.isCoag ? undefined : modePowerRect.right}
                anchors.top: modePowerRect.top
                anchors.bottom: modePowerRect.bottom
            }
            AnchorChanges {
                target: mode
                anchors.left: {modePowerRect.isCoag ? power.right : modePowerRect.left}
                anchors.right: {modePowerRect.isCoag ? modePowerRect.right : power.left}
                anchors.top: modePowerRect.top
                anchors.bottom: modePowerRect.bottom
            }
            AnchorChanges {
                target: powerPlusButton
                anchors.right: undefined
                anchors.bottom: undefined
            }
            AnchorChanges {
                target: powerMinusButton
                anchors.left: undefined
                anchors.bottom: undefined
            }
            AnchorChanges {
                target: powerSlider
                anchors.left: undefined
                anchors.right: undefined
                anchors.verticalCenter: undefined
            }
        },
        State {
            name: "expanded"
            PropertyChanges { target: powerPlusButton;  visible: (modeId != 1000) }
            PropertyChanges { target: powerMinusButton; visible: (modeId != 1000) }
            PropertyChanges { target: powerSlider;      visible: (modeId != 1000) }
            PropertyChanges {
                target: powerLabel;
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 60
            }
            PropertyChanges {
                target: modeLabel;
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 28
            }
            PropertyChanges {
                target: power;
                anchors.topMargin: 100
            }
            PropertyChanges {
                target: modeSelectButton;
                enabled: true
            }
            AnchorChanges {
                target: powerPlusButton
                anchors.right: modePowerRect.right
                anchors.verticalCenter: power.verticalCenter
            }
            AnchorChanges {
                target: powerMinusButton
                anchors.left: modePowerRect.left
                anchors.verticalCenter: power.verticalCenter
            }
            AnchorChanges {
                target: powerSlider
                anchors.left: powerMinusButton.right
                anchors.right: powerPlusButton.left
                anchors.bottom: modePowerRect.bottom
            }
            AnchorChanges {
                target: mode
                anchors.left: modePowerRect.left
                anchors.right: modePowerRect.right
                anchors.top: modePowerRect.top
                anchors.bottom: undefined
            }
            AnchorChanges {
                target: power
                anchors.left: modePowerRect.left
                anchors.right: modePowerRect.right
                anchors.top: modePowerRect.top
                anchors.bottom: undefined
            }
        }
    ]
//    transitions: [
//        Transition {
//            from: "collapsed"
//            to: "expanded"
//            SequentialAnimation {
//                // Этап 1: Плавное изменение размеров шрифта
//                ParallelAnimation {
//                    NumberAnimation {
//                        target: powerLabel
//                        properties: "font.pixelSize"
//                        duration: 300
//                        easing.type: Easing.OutQuart
//                    }
//                    NumberAnimation {
//                        target: modeLabel
//                        properties: "font.pixelSize"
//                        duration: 300
//                        easing.type: Easing.OutQuart
//                    }
//                    NumberAnimation {
//                        target: power
//                        properties: "anchors.topMargin"
//                        duration: 300
//                        easing.type: Easing.OutQuart
//                    }
//                }
//                // Этап 2: Плавное появление кнопок
//                ParallelAnimation {
//                    PropertyAnimation {
//                        target: powerPlusButton
//                        properties: "visible,opacity"
//                        duration: 200
//                        easing.type: Easing.OutQuad
//                    }
//                    PropertyAnimation {
//                        target: powerMinusButton
//                        properties: "visible,opacity"
//                        duration: 200
//                        easing.type: Easing.OutQuad
//                    }
//                    PropertyAnimation {
//                        target: powerSlider
//                        properties: "visible,opacity"
//                        duration: 200
//                        easing.type: Easing.OutQuad
//                    }
//                }
//                // Этап 3: Плавное позиционирование
//                AnchorAnimation {
//                    duration: 250
//                    easing.type: Easing.OutQuart
//                }
//            }
//        },
//        Transition {
//            from: "expanded"
//            to: "collapsed"
//            SequentialAnimation {
//                // Этап 1: Плавное скрытие кнопок
//                ParallelAnimation {
//                    PropertyAnimation {
//                        target: powerPlusButton
//                        properties: "opacity"
//                        from: 1
//                        to: 0
//                        duration: 150
//                        easing.type: Easing.InQuad
//                    }
//                    PropertyAnimation {
//                        target: powerMinusButton
//                        properties: "opacity"
//                        from: 1
//                        to: 0
//                        duration: 150
//                        easing.type: Easing.InQuad
//                    }
//                    PropertyAnimation {
//                        target: powerSlider
//                        properties: "opacity"
//                        from: 1
//                        to: 0
//                        duration: 150
//                        easing.type: Easing.InQuad
//                    }
//                }
//                // Этап 2: Скрытие элементов
//                PropertyAnimation {
//                    targets: [powerPlusButton, powerMinusButton, powerSlider]
//                    properties: "visible"
//                    duration: 0
//                }
//                // Этап 3: Плавное изменение размеров
//                ParallelAnimation {
//                    NumberAnimation {
//                        target: powerLabel
//                        properties: "font.pixelSize"
//                        duration: 350
//                        easing.type: Easing.InQuart
//                    }
//                    NumberAnimation {
//                        target: modeLabel
//                        properties: "font.pixelSize"
//                        duration: 350
//                        easing.type: Easing.InQuart
//                    }
//                    NumberAnimation {
//                        target: power
//                        properties: "anchors.topMargin"
//                        duration: 350
//                        easing.type: Easing.InQuart
//                    }
//                    AnchorAnimation {
//                        duration: 350
//                        easing.type: Easing.InQuart
//                    }
//                }
//            }
//            }
//    ]
}
