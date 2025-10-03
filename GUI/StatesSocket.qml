import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: socketRoot
    color: "transparent"
    // property color middleColor: "black"

    property string title

    property int socketId

    property string cutModeName
    property int cutModePower
    property int cutModeId
    property int cutMaxPower
    property int cutInstrumId
    property string cutInstrumName: qsTr("не выбран")

    property string coagModeName
    property int coagModePower
    property int coagModeId
    property int coagMaxPower
    property int coagInstrumId
    property string coagInstrumName: qsTr("не выбран")

    signal modeEditDialogRequest(int socketId, bool isCoag)
    signal instrumEditDialogRequest(int socketId, bool isCoag)
    signal newPower(int socketId, int pwr, bool isCoag)
    signal socketExpandRequest()
    signal socketCollapseRequest()
    signal absolutePositionChanged(int socketId, real absoluteY)  // Новый сигнал

    state: model.socketdisplaymode
    
    // Принудительно обновляем state при изменении модели
    property string currentModelState: model.socketdisplaymode
    onCurrentModelStateChanged: {
        if (state !== currentModelState) {
            state = currentModelState
        }
    }

    // Логирование абсолютного положения по высоте при изменении позиции
    onYChanged: {
        var absY = mapToItem(null, 0, 0).y
        absolutePositionChanged(socketId, absY)
    }
    
    onHeightChanged: {
        var absY = mapToItem(null, 0, 0).y
        absolutePositionChanged(socketId, absY)
    }
    
    onStateChanged: {
        var absY = mapToItem(null, 0, 0).y
        absolutePositionChanged(socketId, absY)
    }
    
    Component.onCompleted: {
        var absY = mapToItem(null, 0, 0).y
        absolutePositionChanged(socketId, absY)
    }

    // MouseArea для всего сокета - переход в expanded
   MouseArea {
       id: socketMouseArea
       anchors.fill: parent
       onClicked: {
           if (socketRoot.state === "collapsed") {
               socketRoot.state = "expanded"
               socketRoot.socketExpandRequest()
           }
       }
       // Не перехватываем события от дочерних элементов
       propagateComposedEvents: true
       // Не перехватываем события, если сокет уже развернут
       enabled: socketRoot.state === "collapsed"
   }

    HalfSocket {
        id: leftRect
        isCoag: false
        state: socketRoot.state
        modeName:   socketRoot.cutModeName
        modePower:  socketRoot.cutModePower
        modeId:     socketRoot.cutModeId
        maxPower:   socketRoot.cutMaxPower
        instrumId:  socketRoot.cutInstrumId
        instrumName: socketRoot.cutInstrumName

        // Перехватываем события от HalfSocket только для разворачивания
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (socketRoot.state === "collapsed") {
                    socketRoot.state = "expanded"
                    socketRoot.socketExpandRequest()
                }
                mouse.accepted = true // Останавливаем распространение события
            }
            // Не перехватываем события, если сокет уже развернут
            enabled: socketRoot.state === "collapsed"
        }
    }
    HalfSocket {
        id: rightRect
        isCoag: true
        state: socketRoot.state
        modeName:   socketRoot.coagModeName
        modePower:  socketRoot.coagModePower
        modeId:     socketRoot.coagModeId
        maxPower:   socketRoot.coagMaxPower
        instrumId:  socketRoot.coagInstrumId
        instrumName: socketRoot.coagInstrumName

        // Перехватываем события от HalfSocket только для разворачивания
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (socketRoot.state === "collapsed") {
                    socketRoot.state = "expanded"
                    socketRoot.socketExpandRequest()
                }
                mouse.accepted = true // Останавливаем распространение события
            }
            // Не перехватываем события, если сокет уже развернут
            enabled: socketRoot.state === "collapsed"
        }
    }
    Rectangle {
        id: middleRect
        color: "black"
        width: fontMetrics.advanceWidth("MONO 22")
        Label {
            id: socketNameLabel
            anchors.fill: parent
            anchors.margins: 10
            text: title
            font.pixelSize: 24
            font.bold: true
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignTop
        }
        Pedal {
            id: pedalRect
            width: fontMetrics.advanceWidth("MONO 22")
            height: parent.height > width ? width : parent.height
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
        }

        FontMetrics {
            id: fontMetrics
            font: socketNameLabel.font
        }
        // MouseArea для middleRect - переход в collapsed
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (socketRoot.state === "expanded") {
                    socketRoot.state = "collapsed"
                    socketRoot.socketCollapseRequest()
                }
                mouse.accepted = true // Останавливаем распространение события
            }
        }
    }
    Connections {
        target: rightRect
        function onNewPower(pwr) {
            socketRoot.newPower(socketRoot.socketId, pwr, true)
        }
    }
    Connections {
        target: rightRect
        function onModeEditDialogRequest() {
            socketRoot.modeEditDialogRequest(socketRoot.socketId, true)
        }
    }
    Connections {
        target: rightRect
        function onInstrumEditDialogRequest() {
            socketRoot.instrumEditDialogRequest(socketRoot.socketId, true)
        }
    }
    Connections {
        target: leftRect
        function onNewPower(pwr) {
            socketRoot.newPower(socketRoot.socketId, pwr, false)
        }
    }
    Connections {
        target: leftRect
        function onModeEditDialogRequest() {
            socketRoot.modeEditDialogRequest(socketRoot.socketId, false)
        }
    }
    Connections {
        target: leftRect
        function onInstrumEditDialogRequest() {
            socketRoot.instrumEditDialogRequest(socketRoot.socketId, false)
        }
    }

    states: [
        // Свернутое состояние
        State {
            name: "collapsed"
            AnchorChanges {
                target: middleRect
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.left: undefined
                anchors.right: undefined
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }
            PropertyChanges {
                target: middleRect
                color: "black"
                width: fontMetrics.advanceWidth("MONO 22")
            }
            AnchorChanges {
                target: leftRect
                anchors.left: parent.left
                anchors.right: middleRect.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }
            AnchorChanges {
                target: rightRect
                anchors.left: middleRect.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }
        },
        // Развернутое состояние
        State {
            name: "expanded"
            AnchorChanges {
                target: middleRect
                anchors.horizontalCenter: undefined
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: undefined
            }
            PropertyChanges {
                target: middleRect
                color: "transparent"
                height: fontMetrics.height + socketNameLabel.anchors.margins
            }
            AnchorChanges {
                target: leftRect
                anchors.left: parent.left
                anchors.right: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }
            AnchorChanges {
                target: rightRect
                anchors.left: parent.horizontalCenter
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }
        }
    ]
    // Переходы между состояниями (опционально)
    transitions: [
//        Transition {
//            from: "collapsed"
//            to: "expanded"
//            ParallelAnimation {
//                // Основная анимация высоты с более плавным easing
//                NumberAnimation {
//                    properties: "height"
//                    duration: 400
//                    easing.type: Easing.OutQuart
//                }
//                // Плавная анимация позиционирования
//                AnchorAnimation {
//                    duration: 400
//                    easing.type: Easing.OutQuart
//                }
//                // Плавная анимация свойств
//                PropertyAnimation {
//                    properties: "width,color"
//                    duration: 400
//                    easing.type: Easing.OutQuart
//                }
//            }
//        },
//        Transition {
//            from: "expanded"
//            to: "collapsed"
//            ParallelAnimation {
//                NumberAnimation {
//                    properties: "height"
//                    duration: 350
//                    easing.type: Easing.InQuart
//                }
//                AnchorAnimation {
//                    duration: 350
//                    easing.type: Easing.InQuart
//                }
//                PropertyAnimation {
//                    properties: "width,color"
//                    duration: 350
//                    easing.type: Easing.InQuart
//                }
//            }
//        }
    ]
}
