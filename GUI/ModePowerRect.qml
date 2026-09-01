import QtQuick 2.15
import QtQuick.Controls 2.15
import BackEnd 1.0

Canvas {
    id: modePowerRect

    property bool isCoag
    property string modeName
    property int modeId
    property int modePower
    property int maxPower
    property int instrumNum: 1000
    property bool isEndo: false

    signal modeEditDialogRequest()
    signal newPower(int pwr)

    function colorFromId() {
        if (modePowerRect.modeId === ESHF.NO_MODE)
            return "darkgray"
        return isCoag ? "blue" : "yellow"
    }

    function usesUnitPowerStep() {
        return modePowerRect.modeId === ESHF.BI_COAG_MICRO
    }

    function changePower(direction) {
        return changePowerFromValue(modePower, direction)
    }

    // Функция изменения мощности на основе переданного значения (для автоповтора)
    function changePowerFromValue(currentValue, direction) {
        var changedPower = currentValue
        var unitStep = usesUnitPowerStep()
        if (direction === "up") {
            if (unitStep)
                changedPower += 1
            else if (currentValue < 20) changedPower += 1
            else if (currentValue < 50) changedPower += 2
            else if (currentValue < 100) changedPower += 5
            else if (currentValue < 200) changedPower += 10
            else if (currentValue < 400) changedPower += 25
            if (changedPower > maxPower) changedPower = maxPower
        }
        if (direction === "down") {
            if (unitStep)
                changedPower = Math.max(currentValue - 1, 1)
            else if (currentValue <= 1) changedPower = 1
            else if (currentValue <= 20) changedPower -= 1
            else if (currentValue <= 50) changedPower -= 2
            else if (currentValue <= 100) changedPower -= 5
            else if (currentValue <= 200) changedPower -= 10
            else if (currentValue <= 400) changedPower -= 25
            if (changedPower < 1) changedPower = 1
        }
        return changedPower
    }

    function changePowerEndoCut(direction) {
        var changedPower = modePower
        if (direction === "up") {
            if (modePower < 30) changedPower += 10
        }
        if (direction === "down") {
            if (modePower >= 20) changedPower -= 10
        }
        return changedPower
    }

    function changePowerEndoCoag(direction) {
        var changedPower = modePower
        if (direction === "up") {
            if ((modePower % 10) < 3) changedPower += 1
        }
        if (direction === "down") {
            if ((modePower % 10) >= 2) changedPower -= 1
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

    Connections {
        target: modePowerRect
        function onModeIdChanged() {
            modePowerRect.requestPaint()
        }
    }

    Connections {
        target: modePowerRect
        function onIsEndoChanged() {
            modePowerRect.requestPaint()
        }
    }

    Item {
        id: collapsedSummary
        anchors.fill: parent
        visible: modePowerRect.state === "collapsed"
        z: 20

        readonly property bool hasInstrImage: modePowerRect.instrumNum > 0
                                             && modePowerRect.instrumNum !== 1000
        readonly property int imageBlockWidth: hasInstrImage ? 62 : 0

        Image {
            id: collapsedInstrImage
            visible: collapsedSummary.hasInstrImage
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            width: 54
            height: 54
            source: "image://instruments/"
                    + (modePowerRect.isCoag ? "coaginstr" : "cutinstr")
                    + modePowerRect.instrumNum
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: modePowerRect.isCoag ? parent.left : undefined
            anchors.leftMargin: modePowerRect.isCoag ? 4 : undefined
            anchors.right: modePowerRect.isCoag ? undefined : parent.right
            anchors.rightMargin: modePowerRect.isCoag ? undefined : 4
        }

        Column {
            id: collapsedTextColumn
            spacing: 2
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.left: modePowerRect.isCoag ? collapsedInstrImage.right : parent.left
            anchors.leftMargin: modePowerRect.isCoag ? 6 : 8
            anchors.right: modePowerRect.isCoag ? parent.right : collapsedInstrImage.left
            anchors.rightMargin: modePowerRect.isCoag ? 8 : 6

            Label {
                id: collapsedModeLabel
                text: modePowerRect.modeName
                color: modePowerRect.isCoag ? "white" : "black"
                font.pixelSize: 20
                font.bold: true
                wrapMode: Text.Wrap
                lineHeight: 0.75
                lineHeightMode: Text.ProportionalHeight
                maximumLineCount: 2
                elide: Text.ElideRight
                width: parent.width
                horizontalAlignment: modePowerRect.isCoag ? Text.AlignRight : Text.AlignLeft
                verticalAlignment: Text.AlignTop
            }

            Label {
                id: collapsedPowerLabel
                text: modePowerRect.modePower
                visible: modePowerRect.modeId !== ESHF.NO_MODE
                color: modePowerRect.isCoag ? "white" : "black"
                font.pixelSize: 46
                font.bold: true
                width: parent.width
                horizontalAlignment: modePowerRect.isCoag ? Text.AlignRight : Text.AlignLeft
            }
        }
    }

    Rectangle {
        id: mode
        color: "transparent"
        z: 10  // Поднимаем над powerControls для обработки кликов
        // Явно задаём анкоры по умолчанию для expanded state
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: 90  // Базовая высота для области выбора режима
        Label {
            id: modeLabel
            text: modeName
            font.pixelSize: 22
            font.bold: true
            wrapMode: Text.Wrap
            lineHeight: 0.9
            lineHeightMode: Text.ProportionalHeight
            height: 60
            color: isCoag ? "white" : "black"
            // verticalAlignment: Qt.AlignVCenter
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


//        // Визуальная подсветка границ MouseArea
//        Rectangle {
//            anchors {
//                fill: parent
//                topMargin: 5
//                leftMargin: 10
//                rightMargin: 10
//                bottomMargin: 30
//            }
//            color: "red"
//            opacity: 0.2
//            border.color: "yellow"
//            border.width: 2
//            z: 9999
//        }
        
        MouseArea {
            id: modeSelectButton
            anchors {
                fill: parent
                topMargin: 5
                leftMargin: 60
                rightMargin: 60
                bottomMargin: 5  // ИСПРАВЛЕНО: было 90, что давало отрицательную высоту
            }
            z: 10  // Поднимаем над другими элементами
            onClicked: modePowerRect.modeEditDialogRequest()
        }

    }

    Item {
        id: powerControls
        anchors.fill: parent
        visible: !isEndo

        Rectangle {
            id: power
            color: "transparent"
            height: powerPlusButton.height
            anchors.verticalCenter: powerPlusButton.verticalCenter
            Label {
                id: powerLabel
                text: modePower
                visible: (modeId !== ESHF.NO_MODE)
                width: fontMetrics.advanceWidth("999")
                font.pixelSize: 50
                font.bold: true
                color: isCoag ? "white" : "black"
                anchors {
                    left: parent.left
                    leftMargin: 10
                    right: parent.right
                    rightMargin: 10
                    verticalCenter: parent.verticalCenter
                }
                verticalAlignment: Qt.AlignVCenter
                horizontalAlignment: Qt.AlignHCenter
//                // Поведенческая анимация для размера шрифта
//                Behavior on font.pixelSize {
//                    NumberAnimation {
//                        duration: 300
//                        easing.type: Easing.OutCubic
//                    }
//                }
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
            radius: 8
            border {
                color: modePowerRect.isCoag ? "white" : "color"
                width: 2
            }
            anchors.margins: 95
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            
            // Локальные состояния для автоповтора
            property bool held: false

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
                onPressed: {
//                    console.log("PLUS PRESSED", mouse.x, mouse.y, "modePower =", modePower)
                    powerPlusButton.color = "#40ffffff"
                    powerPlusButton.held = true
                    // первый шаг
                    var newValue = changePowerFromValue(modePower, "up")
                    modePowerRect.newPower(newValue)
                    // запускаем таймеры автоповтора и страховки
                    autoRepeatPlusTimer.start()
                    plusSafetyTimer.restart()
                }
                onReleased: {
//                    console.log("PLUS RELEASED", mouse.x, mouse.y, "modePower =", modePower)
                    powerPlusButton.color = "transparent"
                    powerPlusButton.held = false
                    autoRepeatPlusTimer.stop()
                    plusSafetyTimer.stop()
                }
                onCanceled: {
//                    console.log("PLUS CANCELED")
                    powerPlusButton.color = "transparent"
                    powerPlusButton.held = false
                    autoRepeatPlusTimer.stop()
                    plusSafetyTimer.stop()
                }
            }

            // Таймер автоповтора при удержании
            Timer {
                id: autoRepeatPlusTimer
                interval: 200  // мс между шагами автоинкремента
                repeat: true
                onTriggered: {
                    if (!powerPlusButton.held) {
                        stop()
                        return
                    }
//                    console.log("PLUS AUTOREPEAT step, modePower =", modePower)
                    var newValue = changePowerFromValue(modePower, "up")
                    modePowerRect.newPower(newValue)
                }
            }

            // Страховка: если release потерялся, через тайм-аут считаем, что кнопку отпустили
            Timer {
                id: plusSafetyTimer
                interval: 2000
                repeat: false
                onTriggered: {
                    if (powerPlusButton.held) {
                        console.log("PLUS SAFETY TIMEOUT, force release, modePower =", modePower)
                        powerPlusButton.held = false
                        autoRepeatPlusTimer.stop()
                        powerPlusButton.color = "transparent"
                    }
                }
            }
        }

        Rectangle {
            id: powerMinusButton
            width: 60
            height: 60
            color: "transparent"
            radius: 8
            border {
                color: modePowerRect.isCoag ? "white" : "color"
                width: 2
            }
            anchors.margins: 95
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            
            // Локальные состояния для автоповтора
            property bool held: false

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
                onPressed: {
//                    console.log("MINUS PRESSED", mouse.x, mouse.y, "modePower =", modePower)
                    powerMinusButton.color = "#40ffffff"
                    powerMinusButton.held = true
                    // первый шаг
                    var newValue = changePowerFromValue(modePower, "down")
                    modePowerRect.newPower(newValue)
                    // запускаем таймеры автоповтора и страховки
                    autoRepeatMinusTimer.start()
                    minusSafetyTimer.restart()
                }
                onReleased: {
//                    console.log("MINUS RELEASED", mouse.x, mouse.y, "modePower =", modePower)
                    powerMinusButton.color = "transparent"
                    powerMinusButton.held = false
                    autoRepeatMinusTimer.stop()
                    minusSafetyTimer.stop()
                }
                onCanceled: {
//                    console.log("MINUS CANCELED")
                    powerMinusButton.color = "transparent"
                    powerMinusButton.held = false
                    autoRepeatMinusTimer.stop()
                    minusSafetyTimer.stop()
                }
           }

           // Таймер автоповтора при удержании
           Timer {
               id: autoRepeatMinusTimer
               interval: 200  // мс между шагами автоинкремента
               repeat: true
               onTriggered: {
                   if (!powerMinusButton.held) {
                       stop()
                       return
                   }
//                   console.log("MINUS AUTOREPEAT step, modePower =", modePower)
                   var newValue = changePowerFromValue(modePower, "down")
                   modePowerRect.newPower(newValue)
               }
           }

           // Страховка: если release потерялся, через тайм-аут считаем, что кнопку отпустили
           Timer {
               id: minusSafetyTimer
               interval: 2000
               repeat: false
               onTriggered: {
                   if (powerMinusButton.held) {
                       console.log("MINUS SAFETY TIMEOUT, force release, modePower =", modePower)
                       powerMinusButton.held = false
                       autoRepeatMinusTimer.stop()
                       powerMinusButton.color = "transparent"
                   }
               }
           }
        }

        Slider {
            id: powerSlider
            property real _sliderValue: modePower
            value: modePowerRect.isEndo ? _sliderValue : modePower
            enabled: !modePowerRect.isEndo
            opacity: 1
            anchors.bottomMargin: 15
            anchors.leftMargin: 25
            anchors.rightMargin:  25
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            from: 1
            to: maxPower
            onValueChanged: {
                // Не обрабатываем изменения слайдера в эндоскопическом режиме
                if (modePowerRect.isEndo) {
                    _sliderValue = value
                    return
                }
                var roundedValue = roundToStep(value)
                if (roundedValue !== modePower) {
                    modePowerRect.newPower(roundedValue)
                }
            }
        }
    }

    Item {
        id: powerEndoControls
        anchors.fill: parent
        visible: isEndo

        Row {
            id: endoRow
            anchors.centerIn: parent
            spacing: 30

            // Минимальный отступ слева
            Item {
                width: 5
            }

            Rectangle {
                id: powerMinusEndoCut
                width: 60
                height: 60
                color: "transparent"
                radius: 8
                border {
                    color: modePowerRect.isCoag ? "white" : "black"
                    width: 2
                }
                Label {
                    anchors {
                        margins: 10
                        fill: parent
                    }
                    color: modePowerRect.isCoag ? "white" : "black"
                    font.pixelSize: 30
                    font.bold: true
                    text: "-"
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: modePowerRect.newPower(changePowerEndoCut("down"));
                }
            }

            Item {
                width: fontMetricsEndo.advanceWidth("9")
                height: 60
                clip: false
                
                anchors.verticalCenter: powerMinusEndoCut.verticalCenter
                
                Label {
                    id: cutEffect
                    text: qsTr("эффект резания")
                    font.pixelSize: 14
                    color: isCoag ? "white" : "black"
                    width: parent.width
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: powerEndoCutLabel.top
                    anchors.bottomMargin: 10
                    horizontalAlignment: Qt.AlignHCenter
                }
                
                Label {
                    id: powerEndoCutLabel
                    text: Math.floor(modePower / 10)
                    visible: (modeId !== ESHF.NO_MODE)
                    width: parent.width
                    height: 60
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 50
                    font.bold: true
                    color: isCoag ? "white" : "black"
                    verticalAlignment: Qt.AlignVCenter
                    horizontalAlignment: Qt.AlignHCenter
                }
            }

            Rectangle {
                id: powerPlusEndoCut
                width: 60
                height: 60
                color: "transparent"
                radius: 8
                border {
                    color: modePowerRect.isCoag ? "white" : "black"
                    width: 2
                }
                Label {
                    anchors {
                        margins: 10
                        fill: parent
                    }
                    color: modePowerRect.isCoag ? "white" : "black"
                    font.pixelSize: 30
                    font.bold: true
                    text: "+"
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: modePowerRect.newPower(changePowerEndoCut("up"));
                }
            }

            // Увеличенный отступ между Cut и Coag группами
            Rectangle {
                id: dummyRect
                width: 5
                height: 60
                color: "transparent"
            }

            Rectangle {
                id: powerMinusEndoCoag
                width: 60
                height: 60
                color: "transparent"
                radius: 8
                border {
                    color: modePowerRect.isCoag ? "white" : "black"
                    width: 2
                }
                Label {
                    anchors {
                        margins: 10
                        fill: parent
                    }
                    color: modePowerRect.isCoag ? "white" : "black"
                    font.pixelSize: 30
                    font.bold: true
                    text: "-"
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var newValue = changePowerEndoCoag("down");
                        if (newValue !== modePower) {
                            modePowerRect.newPower(newValue);
                        }
                    }
                }
            }

            Item {
                width: fontMetricsEndo.advanceWidth("9")
                height: 60
                clip: false
                
                anchors.verticalCenter: powerMinusEndoCoag.verticalCenter
                
                Label {
                    id: coagEffect
                    text: qsTr("эффект коагуляции")
                    font.pixelSize: 14
                    color: isCoag ? "white" : "black"
                    width: parent.width
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: powerEndoCoagLabel.top
                    anchors.bottomMargin: 10
                    horizontalAlignment: Qt.AlignHCenter
                }
                
                Label {
                    id: powerEndoCoagLabel
                    text: modePower % 10
                    visible: (modeId !== ESHF.NO_MODE)
                    width: parent.width
                    height: 60
                    anchors.verticalCenter: parent.verticalCenter
                    font.pixelSize: 50
                    font.bold: true
                    color: isCoag ? "white" : "black"
                    verticalAlignment: Qt.AlignVCenter
                    horizontalAlignment: Qt.AlignHCenter
                }
            }

            Rectangle {
                id: powerPlusEndoCoag
                width: 60
                height: 60
                color: "transparent"
                radius: 8
                border {
                    color: modePowerRect.isCoag ? "white" : "black"
                    width: 2
                }
                Label {
                    anchors {
                        margins: 10
                        fill: parent
                    }
                    color: modePowerRect.isCoag ? "white" : "black"
                    font.pixelSize: 30
                    font.bold: true
                    text: "+"
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var newValue = changePowerEndoCoag("up");
                        if (newValue !== modePower) {
                            modePowerRect.newPower(newValue);
                        }
                    }
                }
            }

            // Минимальный отступ справа
            Item {
                width: 5
            }
        }

        FontMetrics {
            id: fontMetricsEndo
            font: powerLabel.font
        }
    }

    Rectangle {
        id: powerEndo
        color: "transparent"
        visible: isEndo
        anchors.left: {modePowerRect.isCoag ? powerControls.left : undefined}
        anchors.right: {modePowerRect.isCoag ? undefined : powerControls.right}
        anchors.top: powerControls.top
        anchors.bottom: powerControls.bottom
        Label {
            id: powerLabelEndoCollapse
            text: Math.floor(modePower/10) + "-" + (modePower%10)
            visible: (modeId !== ESHF.NO_MODE)
            width: fontMetrics2.advanceWidth("999")
            height: 31
            font.pixelSize: 50
            font.bold: true
            color: isCoag ? "white" : "black"
            // verticalAlignment: Qt.AlignVCenter
            anchors {
                margins: 10
                fill: parent
            }
            horizontalAlignment: isCoag ? Text.AlignLeft : Text.AlignRight
            verticalAlignment: Text.AlignVCenter

//            // Поведенческая анимация для размера шрифта
//            Behavior on font.pixelSize {
//                NumberAnimation {
//                    duration: 300
//                    easing.type: Easing.OutCubic
//                }
//            }
        }
        FontMetrics {
            id: fontMetrics2
            font: powerLabel.font
        }
    }

    states: [
        State {
            name: "collapsed"
            PropertyChanges { target: mode; visible: false }
            PropertyChanges { target: power; visible: false }
            PropertyChanges { target: powerPlusButton;  visible: false }
            PropertyChanges { target: powerMinusButton; visible: false }
            PropertyChanges { target: powerSlider;      visible: false }
            PropertyChanges { target: powerEndoControls; visible: false }
//            PropertyChanges { target: powerPlusEndoCut;  visible: false }
//            PropertyChanges { target: powerMinusEndoCut; visible: false }
//            PropertyChanges { target: powerPlusEndoCoag;  visible: false }
//            PropertyChanges { target: powerMinusEndoCoag; visible: false }
//            PropertyChanges { target: cutEffect; visible: false }
//            PropertyChanges { target: coagEffect; visible: false }
            PropertyChanges {
                target: powerLabel;
                horizontalAlignment: isCoag ? Text.AlignLeft : Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                anchors.bottomMargin: 10
                anchors.topMargin: 10
//                font.pixelSize: 50
            }
            PropertyChanges {
                target: modeLabel;
                horizontalAlignment: isCoag ? Text.AlignRight : Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
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
                anchors.left: {modePowerRect.isCoag ? powerControls.left : undefined}
                anchors.right: {modePowerRect.isCoag ? undefined : powerControls.right}
//                anchors.top: powerControls.top
//                anchors.bottom: powerControls.bottom
            }
            PropertyChanges {
                target: mode;
                height: undefined  // В collapsed высота определяется анкорами
            }
            AnchorChanges {
                target: mode
                anchors.left: {modePowerRect.isCoag ? powerControls.right : modePowerRect.left}
                anchors.right: {modePowerRect.isCoag ? modePowerRect.right : powerControls.left}
                anchors.top: powerControls.top
                anchors.bottom: powerControls.bottom
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
            PropertyChanges { target: mode; visible: true }
            PropertyChanges { target: power; visible: true }
            PropertyChanges { target: powerPlusButton;  visible: (modeId !== ESHF.NO_MODE) }
            PropertyChanges { target: powerMinusButton; visible: (modeId !== ESHF.NO_MODE) }
            PropertyChanges { target: powerSlider;      visible: (modeId !== ESHF.NO_MODE) }
            PropertyChanges { target: powerEndo;  visible: false }
            PropertyChanges {
                target: powerLabel;
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
                anchors.bottomMargin: 40
                anchors.topMargin: -20
//                font.pixelSize: 50
            }
            PropertyChanges {
                target: modeLabel;
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignTop
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
            PropertyChanges {
                target: mode;
                height: 100
            }
//            AnchorChanges {
//                target: powerPlusButton
//                anchors.right: modePowerRect.right
//                anchors.bottom: power.bottom
//            }
//            AnchorChanges {
//                target: powerMinusButton
//                anchors.left: modePowerRect.left
//                anchors.bottom: power.bottom
//            }
//            AnchorChanges {
//                target: powerSlider
//                anchors.bottom: parent.bottom
//            }
            PropertyChanges {
                target: mode;
                height: 90  // Достаточная высота для клика
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
                anchors.left: powerControls.left
                anchors.right: powerControls.right
//                anchors.top: powerControls.top
//                anchors.left: undefined
//                anchors.right: undefined
//                anchors.top: undefined
//                anchors.bottom: undefined
//                anchors.verticalCenter: powerPlusButton.verticalCenter
            }
        }
    ]
//    transitions: [
//        Transition {
//            from: "collapsed"
//            to: "expanded"
//            NumberAnimation {  duration: 100; easing.type: Easing.InQuad }
//        },
//        Transition {
//            from: "expanded"
//            to: "collapsed"
//            NumberAnimation { duration: 100; easing.type: Easing.InQuad }
//        }
//    ]
}
