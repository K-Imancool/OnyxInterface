import QtQuick 2.15
import QtQuick.Controls 2.15

Popup {
    id: activationPopup
    
    // Привязка к данным из ControlCenter
    property string socketName /*control.activeSocketName || "Неизвестный сокет"*/
    property string modeName /*control.activeModeName || "Режим не выбран"*/
    property int power: 0
    property bool isCoag: false
    property bool isEndo: false
    property int activeSocketId: -1
    
    onPowerChanged: {
//        console.log("Activation.qml: power changed to", power)
    }
    
    onOpened: {
//        console.log("Activation.qml opened: socketName=", socketName, "modeName=", modeName, "power=", power, "isCoag=", isCoag, "isEndo=", isEndo)
    }
    
    // Настройки popup
    modal: true  // Модальный - блокируем касания
    closePolicy: Popup.NoAutoClose  // Закрывается только программно

    onClosed: {
        activeSocketId = -1
    }

    // Перехватываем все события мыши
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: false
        preventStealing: true
    }
    
    // Фон в зависимости от режима
    background: Rectangle {
        id: backgroundRect
        color: isCoag ? "#0f58fa" : "#ffd900"  // Синий для коагуляции, жёлтый для резания
        radius: 20
        border.color: "white"
        border.width: 3
        opacity: 1.0
    }
    
    contentItem: Item {
        // Главный контейнер с информацией
        Column {
            anchors.centerIn: parent
            spacing: 32
            
            // Заголовок "АКТИВАЦИЯ"
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("АКТИВАЦИЯ ") + socketName
                font.pixelSize: 64
                font.bold: true
                color: isCoag ? "white" : "black"
                style: Text.Outline
                styleColor: isCoag ? "black" : "white"
            }
            
            // Разделитель
            Rectangle {
                width: 560
                height: 4
                color: isCoag ? "white" : "black"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
            // Название режима
            Label {
                id: modeNameLabel
                anchors.horizontalCenter: parent.horizontalCenter
                text: modeName
                font.pixelSize: 96
                color: isCoag ? "white" : "black"
                style: Text.Outline
                styleColor: isCoag ? "black" : "white"
            }

            // Мощность
            Rectangle {
                id: powerInfoRect
                anchors.horizontalCenter: parent.horizontalCenter
                width: 680
                height: 280
                color: "transparent"
                
                Column {
                    visible: !activationPopup.isEndo
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.width
                    spacing: 16
                    Label {
                        width: parent.width
                        font.pixelSize: 56
                        font.bold: true
                        color: activationPopup.isCoag ? "white" : "black"
                        style: Text.Outline
                        styleColor: activationPopup.isCoag ? "black" : "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Мощность")
                    }
                    Label {
                        width: parent.width
                        height: parent.height - parent.children[0].height - parent.spacing
                        font.pixelSize: 140
                        font.bold: true
                        color: activationPopup.isCoag ? "white" : "black"
                        style: Text.Outline
                        styleColor: activationPopup.isCoag ? "black" : "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: activationPopup.power
                    }
                }

                Column {
                    id: endoCutColumn
                    visible: activationPopup.isEndo
                    anchors.left: parent.left
                    anchors.right: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    spacing: 16
                    Label {
                        width: parent.width
                        font.pixelSize: 56
                        font.bold: true
                        color: activationPopup.isCoag ? "white" : "black"
                        style: Text.Outline
                        styleColor: activationPopup.isCoag ? "black" : "white"
                        text: qsTr("Эффект\nрезания")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    Label {
                        width: parent.width
                        height: parent.height - parent.children[0].height - parent.spacing
                        font.pixelSize: 140
                        font.bold: true
                        color: activationPopup.isCoag ? "white" : "black"
                        style: Text.Outline
                        styleColor: activationPopup.isCoag ? "black" : "white"
                        text: {
                            var cutValue = Math.floor(activationPopup.power / 10)
//                            console.log("Endo cut value:", cutValue, "from power:", activationPopup.power)
                            return cutValue
                        }
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                Column {
                    id: endoCoagColumn
                    visible: activationPopup.isEndo
                    anchors.left: parent.horizontalCenter
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    spacing: 16
                    Label {
                        width: parent.width
                        font.pixelSize: 56
                        font.bold: true
                        color: activationPopup.isCoag ? "white" : "black"
                        style: Text.Outline
                        styleColor: activationPopup.isCoag ? "black" : "white"
                        text: qsTr("Эффект\nкоагуляции")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    Label {
                        width: parent.width
                        height: parent.height - parent.children[0].height - parent.spacing
                        font.pixelSize: 140
                        font.bold: true
                        color: activationPopup.isCoag ? "white" : "black"
                style: Text.Outline
                        styleColor: activationPopup.isCoag ? "black" : "white"
                        text: {
                            var coagValue = activationPopup.power % 10
//                            console.log("Endo coag value:", coagValue, "from power:", activationPopup.power)
                            return coagValue
                        }
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        
        // Анимированная рамка по краям
        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            color: "transparent"
            border.color: "white"
            border.width: 3
            radius: 15
            opacity: 0.7
            
            SequentialAnimation on border.width {
                running: activationPopup.visible
                loops: Animation.Infinite
                
                NumberAnimation {
                    from: 3
                    to: 6
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 6
                    to: 3
                    duration: 800
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
    
    // Открытие/закрытие с анимацией
    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: 50
        }
        NumberAnimation {
            property: "scale"
            from: 0.9
            to: 1.0
            duration: 50
            easing.type: Easing.OutQuad
        }
    }
    
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: 50
        }
        NumberAnimation {
            property: "scale"
            from: 1.0
            to: 0.9
            duration: 50
            easing.type: Easing.InQuad
        }
    }
}

