import QtQuick 2.15
import QtQuick.Controls 2.15
import StratifyLabs.UI 2.0
import BackEnd 1.0

Dialog {
    readonly property string progName: progNameInput.text
    readonly property string scopeName: contRect.isNewScope ? newScopeNameInput.text : scopeNameBox.currentText
    title: qsTr("Сохранение программы")
    standardButtons: Dialog.Ok | Dialog.Cancel
    modal: true
    
    onOpened: {
        recomHandle.isRecomProgs = false
        progNameInput.text = container.currentProgName || "Программа 1"
        progNameInput.selectAll()
    }
    
    onClosed: {
        progNameInput.focus = false
        Qt.inputMethod.hide()
    }

    contentItem: Rectangle {
        id: contRect
        property bool isNewScope: false
        color: "transparent"

        SPanel {
            id: scopePanel
            style: "panel-primary";
            heading: "Выберите или добавьте категорию программы";
            width: 0.9 * parent.width
            anchors.top : contRect.top
            anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter

            SRow {
                SDropdown {
                    id: scopeNameBox
                    span: 8
                    model: ["ha","hahaha", "bu", "bububu"]
                    visible: !contRect.isNewScope
                }
                SInput {
                    id: newScopeNameInput
                    span: 8
                    placeholder: "Категория"
                    visible: contRect.isNewScope
                }

                SButton{
                    id: addScopeButton
                    span: 4
                    iconString: contRect.isNewScope ? Fa.Icon.chevron_left : Fa.Icon.plus_square;
                    onClicked: {
                        contRect.isNewScope = !contRect.isNewScope
                    }
                }
            }
        }

        SPanel {
            id: inputPanel
            heading: "Укажите название программы";
            width: 0.9 * parent.width
            anchors.top : scopePanel.bottom
            anchors.topMargin: 10
            anchors.horizontalCenter: parent.horizontalCenter

            SInput {
                id: progNameInput
                placeholder: "Название";
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width * .9
            }
        }

    }
}
