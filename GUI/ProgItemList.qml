import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Rectangle {
    id: recProgs
    // property alias innerModel: repeatRoot.model
    signal clickedButton(int idx)
    signal returnButtonPressed()
    color: "black"

    property var itemIdArr: []
    property var itemNameArr: []
    property bool loadClear: true


    ListModel {
        id: scopeModel
    }

    ListModel {
        id: progsModel
    }

    function updateModel() {
        progsModel.clear()
        itemNameArr = recomHandle.progNameList
        itemIdArr = recomHandle.progIdList
        if (itemIdArr.length !== itemNameArr.length) {
            console.warn("Lists from C++ have different lengths!")
            return
        }
        for (var i = 0; i < itemIdArr.length; i++) {
            progsModel.append({
                itemId: itemIdArr[i],
                itemName: itemNameArr[i],
                rowIndex: i
            })
        }
        // console.log(itemNameArr)
        progList.innerModel = progsModel
        progList.curIndex = -1
    }

    // onOpened: {
    // }
    Component.onCompleted: {

        itemNameArr = recomHandle.scopeNameList
        itemIdArr = recomHandle.scopeIdList
        if (itemIdArr.length !== itemNameArr.length) {
            console.warn("Lists from C++ have different lengths!")
            return
        }
        for (var i = 0; i < itemIdArr.length; i++) {
            scopeModel.append({
                itemId: itemIdArr[i],
                itemName: itemNameArr[i],
                rowIndex: i
            })
        }
        // console.log(itemIdArr.length)
        // console.log(itemNameArr)
        scopeList.innerModel = scopeModel
        // updateModel()
        recomHandle.scopeIdx = 0
    }

    Rectangle {
        id: headerRect
        height: 60
        color: "darkgray"
        anchors {
            left: parent.left
            top: parent.top
            right: parent.right
        }
        Text {
            anchors.fill: parent
            text: qsTr("РЕКОМЕНДОВАННЫЕ ПРОГРАММЫ")
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            font.pixelSize: 30
        }
    }

    Rectangle {
        id: scopeRect
        width: parent.width * 0.42
        anchors {
            left: parent.left
            top: headerRect.bottom
            bottom: retButton.top
        }
        ItemList {
            id: scopeList
            anchors.fill: parent
            curIndex: recomHandle.scopeIdx
            noImage: false
            imageSourceTemplate: "image://instrums/Scope%1"
        }

    }
    Rectangle {
        id: progRect
        anchors {
            left: border.right
            top: headerRect.bottom
            bottom: retButton.top
            right: parent.right
            leftMargin: 2
        }

        ItemList {
            anchors.fill: parent
            id: progList
            noImage: true
            // imageSourceTemplate: "image://instrums/%1"
        }
    }
    Rectangle {
        id: border
        width: 2
        anchors {
            left: scopeRect.right
            top: headerRect.bottom
            bottom: retButton.top
            leftMargin: 2

        }
        color: "darkgray"
    }

    Button {
        id: retButton

        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: 200
        height: 50
        background: Rectangle {
            radius: 8
            color: "black"
            border.color: "darkgray"
            border.width: 2
        }

        text: qsTr("Назад")

        onClicked: recProgs.returnButtonPressed()
    }

    Connections {
        target: scopeList
        function onNewIndexSelected(index) {
            recomHandle.scopeIdx = index
            recProgs.updateModel()
        }
    }
    Connections {
        target: progList
        function onNewIndexSelected(index) {
            recomHandle.loadRecommendedProg(index, loadClear)
            recProgs.clickedButton(-1);
        }
    }
}

