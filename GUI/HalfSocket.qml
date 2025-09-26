import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: halfSocketRoot

    property bool isCoag
    property string modeName
    property int modePower
    property int modeId
    property int maxPower
    property int socketId
    property int instrumId
    property string instrumName: qsTr("не выбран")

    signal modeEditDialogRequest()
    signal instrumEditDialogRequest()
    signal newPower(int power)

    color: "transparent"

    InstrumRect {
        id: instrumRect
        isCoag: halfSocketRoot.isCoag
        instrumName: halfSocketRoot.instrumName
        instrumId: halfSocketRoot.instrumId
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }
    ModePowerRect {
        id: modePower
        state: halfSocketRoot.state
        isCoag: halfSocketRoot.isCoag
        modeName: halfSocketRoot.modeName
        modePower: halfSocketRoot.modePower
        modeId: halfSocketRoot.modeId
        maxPower: halfSocketRoot.maxPower
        anchors {
            top: instrumRect.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
    }

    Connections {
        target: instrumRect
        function onInstrumEditDialogRequest() {
            halfSocketRoot.instrumEditDialogRequest()
        }
    }
    Connections {
        target: modePower
        function onModeEditDialogRequest() {
            halfSocketRoot.modeEditDialogRequest()
        }
    }
    Connections {
        target: modePower
        function onNewPower(pwr) {
            halfSocketRoot.newPower(pwr)
        }
    }

    states: [
        State {
            name: "collapsed"
            PropertyChanges {
                target: instrumRect;
                visible: false
                height: 0
            }
        },
        State {
            name: "expanded"
            PropertyChanges {
                target: instrumRect;
                visible: (halfSocketRoot.modeId != 1000)
                height: halfSocketRoot.height * .4
            }
        }
    ]
    transitions: [
//        Transition {
//            from: "collapsed"
//            to: "expanded"
//            SequentialAnimation {
//                // Плавное появление InstrumRect
//                PropertyAnimation {
//                    target: instrumRect
//                    properties: "visible"
//                    duration: 0
//                }
//                // Плавное изменение высоты InstrumRect
//                NumberAnimation {
//                    target: instrumRect
//                    properties: "height"
//                    duration: 400
//                    easing.type: Easing.OutQuart
//                }
//            }
//        },
//        Transition {
//            from: "expanded"
//            to: "collapsed"
//            SequentialAnimation {
//                // Плавное изменение высоты InstrumRect
//                NumberAnimation {
//                    target: instrumRect
//                    properties: "height"
//                    duration: 350
//                    easing.type: Easing.InQuart
//                }
//                // Скрытие InstrumRect
//                PropertyAnimation {
//                    target: instrumRect
//                    properties: "visible"
//                    duration: 0
//                }
//            }
//        }
    ]
}
