import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Popup {
    property int socId: -1
    property int modeIndex: -1
    property bool isCoag: false
    property bool deferCommit: false
    property bool dialogAccepted: false
    readonly property var modeEditor: Editor
    property string imageNameTemplate
    
    id: root
    modal: true
    parent: Overlay.overlay
    width: parent.width
    height: parent.height
    x: 0
    y: 0
    
    property var itemIdArr: []
    property var itemNameArr: []
    property var itemNumArr: []
    ListModel {
        id: combinedModel
    }
    
    //т.к. при прямом присвоение ломается бандинг,
    // то нужна функция, перезадающая модель
    function updateModel() {
        combinedModel.clear()
        
        if (itemIdArr.length !== itemNameArr.length || itemIdArr.length !== itemNumArr.length) {
            console.warn("Lists from C++ have different lengths!")
            return
        }
        for (var i = 0; i < itemIdArr.length; i++) {
            combinedModel.append({
                itemId: itemNumArr[i],  // Используем Num вместо ID для изображений
                itemName: itemNameArr[i],
                // rowIndex: i
            })
        }
        
        instrumListView.innerModel = combinedModel
    }

    onAboutToShow: dialogAccepted = false

    onOpened: {
        // Запрещаем активацию при открытии popup
        // control.enableActivation = false
        
        modeEditor.initialize(socId, modeIndex, isCoag)

        itemNameArr = modeEditor.instrList
        itemIdArr = modeEditor.instrListIds()
        itemNumArr = modeEditor.instrListNums()
        updateModel()
        //кринж, но т.к. вызывается переназначение свойств
        //и триггерятся сигналы
        var bla = modeEditor.currentInstrIndex
        modeEditor.currentInstrIndex = bla
    }
    
    Rectangle {
        id: back
        anchors.fill: parent
        color: "transparent"

        GradientBack {
            id: gradientBack
            anchors.fill: parent
            startColor: isCoag ? "#000066" : "#443300"
            stopColor: isCoag ? "#0000aa" : "#665500"
            beamColor: isCoag ? "#5078FF" : "#B4963C"
        }

        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 100
            color: "transparent"

            Label {
                id: titleLable
                text: qsTr("Выбор инструмента для выхода %1")
                        .arg(modeEditor.socketName)
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 28
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                height: parent.height / 2
                width: parent.width * 0.8
                color: "white"
            }
            Rectangle {
                id: titleLowerRect
                color: isCoag ? "blue" : "yellow"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                height: parent.height / 2
                width: titleLableLower.contentWidth + 50
            }

            Label {
                id: titleLableLower
                text: qsTr("РЕЖИМ: %1")
                        .arg(modeEditor.currentMode.name)
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 28
                font.bold: true
                color: isCoag ? "white" : "black"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                height: parent.height / 2
                width: parent.width * 0.8
            }

            Button {
                id: cancelButton
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                    left: titleLable.right
                }
                background: Rectangle {
                    color: "transparent"
                    radius: 8
                }
                Text {
                    id: cancelText
                    text: qsTr("X")
                    font.pixelSize: 34
                    font.bold: true
                    anchors.fill: parent
                    horizontalAlignment: Qt.AlignHCenter
                    verticalAlignment: Qt.AlignVCenter
                    color: "white"
                }
                onClicked: {
                    root.dialogAccepted = false
                    if (!root.deferCommit)
                        modeEditor.rollBack()
                    root.close()
                }
            }
        }

        Rectangle {
            id: instrumList
            color: "transparent"
            anchors {
                left: parent.left
                bottom: parent.bottom
                top: header.bottom
            }
            width: .3 * parent.width
            ItemList {
                id: instrumListView
                curIndex: modeEditor.currentInstrIndex
                anchors {
                    top: parent.top
                    bottom: footer.top
                    left: parent.left
                    right: parent.right
                }
                imageSourceTemplate: "image://instruments/minstr%1"
            }
            Rectangle {
                id: footer
                height: 100
                color: "transparent"
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                }
                Button {
                    id: downButton
                    width: instrumList.width * .4
                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        left: parent.left
                    }
                    background: Rectangle {
                        color: "transparent"
                        radius: 8
                    }
                    Text {
                        id: downText
                        text: qsTr("▼")
                        font.pixelSize: 34
                        font.bold: true
                        anchors.fill: parent
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        color: "white"
                    }
                    onClicked: {
                        instrumListView.scrollDown()
                    }
                }
                Button {
                    id: upButton
                    width: downButton.width
                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        left: downButton.right
                        leftMargin: 20
                    }
                    background: Rectangle {
                        color: "transparent"
                        radius: 8
                    }
                    Text {
                        id: upText
                        text: qsTr("▲")
                        font.pixelSize: 34
                        font.bold: true
                        anchors.fill: parent
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        color: "white"
                    }
                    onClicked: {
                        instrumListView.scrollUp()
                    }
                }
            }
        }

        DialogActionButton {
            id: declineButton
            width: parent.width * .2
            height: parent.height * .1
            anchors {
                bottom: parent.bottom
                bottomMargin: 10
                left: instrumList.right
                leftMargin: 20
            }
            text: qsTr("ОТМЕНА")
            secondaryColor: "transparent"
            secondaryBorderWidth: 3
            secondaryBorderColor: "white"
            cornerRadius: 8
            labelPixelSize: 34
            labelColor: "white"
            onClicked: {
                root.dialogAccepted = false
                if (!root.deferCommit)
                    modeEditor.rollBack()
                root.close()
            }
        }

        DialogActionButton {
            id: acceptButton
            width: parent.width * .2
            height: parent.height * .1

            //именно эдитор знает дейсвительно ли есть изменения
            //именно эдитор занимается их обработкой и внесением в модель
            visible: modeEditor.hasChanges
            anchors {
                bottom: parent.bottom
                bottomMargin: 10
                left: declineButton.right
                leftMargin: 80
            }
            text: qsTr("ПРИНЯТЬ")
            primary: true
            primaryEnabledColor: "transparent"
            primaryDisabledColor: "transparent"
            primaryBorderWidth: 3
            primaryBorderColor: "lightgreen"
            cornerRadius: 8
            labelPixelSize: 34
            labelColor: "lightgreen"
            onClicked: {
                root.dialogAccepted = true
                if (!root.deferCommit)
                    modeEditor.commitChanges()
                root.close()
            }
        }

        Rectangle {
            id: instrumPreview
            anchors {
                top: header.bottom
                bottom: acceptButton.top
                right: parent.right
                left: instrumList.right
            }
            color: "transparent"
            Image {
                id: previewImage
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                source: {
                    if (modeEditor.currentInstrIndex >= 0 && modeEditor.currentInstrIndex < itemNumArr.length) {
                        return ("image://instruments/instrum%1").arg(itemNumArr[modeEditor.currentInstrIndex])
                    }
                    return ""
                }
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: instrBriefText.top
                    top: parent.top
                    margins: 10
                }
            }
            
            Text {
                id: instrBriefText
                text: modeEditor.instrBrief
                color: "white"
                font.pixelSize: 30
                font.bold: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: buttonRow.top
                    margins: 10
                }
            }
            
            Rectangle {
                id: buttonRow
                height: 100
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: recommendText.top
                }
                color: "transparent"
                RowLayout {
                    id:lay
                    anchors.fill: parent
                    spacing: 5
                    property int pwr

                    PowerRect {
                        id: but1
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        Layout.margins: 10
                        borderColor: "darkcyan"
                        power: modeEditor.lowPowerBound
                        selected: (modeEditor.currentPower === power)
                        isEndo: modeEditor.isEndo
                    }
                    PowerRect {
                        id: but2
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        Layout.margins: 10
                        borderColor: "lightgreen"
                        power: modeEditor.midPowerBound
                        selected: (modeEditor.currentPower === power)
                        isEndo: modeEditor.isEndo
                    }
                    PowerRect {
                        id: but3
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        Layout.margins: 10
                        borderColor: "gold"
                        power: modeEditor.highPowerBound
                        selected: (modeEditor.currentPower === power)
                        isEndo: modeEditor.isEndo
                    }
                }
            }

            Text {
                id: recommendText
                text: qsTr("Выберите рекомендуемую мощность")
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: 10
                font.pixelSize: 20
                font.bold: true
                color: "white"
            }
        }
    }

    Connections {
        target: but3
        function onPowerChosen() {
            modeEditor.updateParameter("currentpower", but3.power)
        }
    }
    Connections {
        target: but2
        function onPowerChosen() {
            modeEditor.updateParameter("currentpower", but2.power)
        }
        //мощность на кнопке меняется при инициализации
        //и мы по умолчанию устанавливаем в диалоге среднюю мощность
        function onPowerChanged() {
            modeEditor.updateParameter("currentpower", but2.power)
        }
    }
    Connections {
        target: but1
        function onPowerChosen() {
            modeEditor.updateParameter("currentpower", but1.power)
        }
        //мощность на кнопке меняется при инициализации
        //если средней мощности нет, по умолчанию устанавливаем в диалоге минимальную
        function onPowerChanged() {
            if (modeEditor.midPowerBound === 0) {
                modeEditor.updateParameter("currentpower", but1.power)
            }
        }
    }

    Connections {
        target: instrumListView
        function onNewIndexSelected(index) {
            modeEditor.currentInstrIndex = index
        }
    }
}
