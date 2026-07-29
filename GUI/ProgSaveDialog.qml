import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Dialog {
    id: root

    signal overwriteConfirmationRequested()

    readonly property string progName: progNameInput.text
    readonly property string scopeName: isNewScope ? newScopeNameInput.text : scopeNameBox.currentText

    property string originalProgName: ""
    property string originalScopeName: ""
    property bool currentProgramIsUser: false
    property bool isNewScope: false

    modal: true

    function ensureKeyboard(field) {
        if (!field.activeFocus)
            field.forceActiveFocus()
        Qt.inputMethod.show()
    }

    function clearTextFieldFocus() {
        if (progNameInput.activeFocus)
            progNameInput.focus = false
        if (newScopeNameInput.activeFocus)
            newScopeNameInput.focus = false
    }

    onOpened: {
        console.log("openSaveDia")
        recomHandle.isRecomProgs = false
        scopeNameBox.model = recomHandle.scopeNameList
        var scopeIndex = originalScopeName.length > 0
                ? scopeNameBox.find(originalScopeName)
                : -1
        scopeNameBox.currentIndex = scopeIndex >= 0 ? scopeIndex : 0
        progNameInput.text = originalProgName
        isNewScope = false
        Qt.callLater(function() {
            progNameInput.forceActiveFocus()
            Qt.inputMethod.show()
        })
    }

    Connections {
        target: Qt.inputMethod
        enabled: root.visible
        function onVisibleChanged() {
            if (!Qt.inputMethod.visible)
                root.clearTextFieldFocus()
        }
    }

    header: Rectangle {
        height: 56
        color: fotekGreen

        Label {
            anchors.fill: parent
            text: qsTr("Сохранение программы")
            font.bold: true
            font.pixelSize: 36
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            color: "black"
        }
    }

    background: Rectangle {
        color: "#f5f5f5"
    }

    contentItem: Rectangle {
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 32
            anchors.rightMargin: 32
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            spacing: 18

            Label {
                Layout.fillWidth: true
                text: qsTr("Выберите папку или создайте новую")
                horizontalAlignment: Qt.AlignHCenter
                font.pixelSize: 26
                font.bold: true
                wrapMode: Text.WordWrap
                color:  "#5A6478"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ComboBox {
                    id: scopeNameBox
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    visible: !root.isNewScope
                    font.pixelSize: 28
                    model: []

                    delegate: ItemDelegate {
                        width: scopeNameBox.width
                        height: 64
                        contentItem: Text {
                            text: modelData
                            font.pixelSize: 26  // пункты в выпадающем списке
                            color: "black"
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                        highlighted: scopeNameBox.highlightedIndex === index
                    }
                }

                TextField {
                    id: newScopeNameInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    visible: root.isNewScope
                    placeholderText: qsTr("Категория")
                    color: "black"
                    font.pixelSize: 28
                    selectByMouse: true
                    activeFocusOnPress: true
                    inputMethodHints: Qt.ImhNoPredictiveText
                    background: Rectangle {
                        color: "#ffffff"
                        border.color: newScopeNameInput.activeFocus ? "#4a9eff" : "#7a7a7a"
                        border.width: 2
                        radius: 6
                    }
                    onActiveFocusChanged: {
                        if (activeFocus)
                            Qt.inputMethod.show()
                    }

                    MouseArea {
                        anchors.fill: parent
                        propagateComposedEvents: true
                        onPressed: {
                            root.ensureKeyboard(newScopeNameInput)
                            mouse.accepted = false
                        }
                    }
                }

                DialogActionButton {
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 72
                    text: root.isNewScope ? "←" : "+"
                    labelPixelSize: 34
                    primary: true
                    onPressed: {
                        root.isNewScope = !root.isNewScope
                        Qt.callLater(function() {
                            if (root.isNewScope)
                                root.ensureKeyboard(newScopeNameInput)
                            else
                                root.clearTextFieldFocus()
                        })
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 12
                text: qsTr("Укажите название программы")
                horizontalAlignment: Qt.AlignHCenter
                font.pixelSize: 26
                font.bold: true
                wrapMode: Text.WordWrap
                color:  "#5A6478"
            }

            TextField {
                id: progNameInput
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                placeholderText: qsTr("Название")
                color: "black"
                font.pixelSize: 28
                selectByMouse: true
                activeFocusOnPress: true
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle {
                    color: "#ffffff"
                    border.color: progNameInput.activeFocus ? "#4a9eff" : "#7a7a7a"
                    border.width: 2
                    radius: 6
                }
                onActiveFocusChanged: {
                    if (activeFocus)
                        Qt.inputMethod.show()
                }

                MouseArea {
                    anchors.fill: parent
                    propagateComposedEvents: true
                    onPressed: {
                        root.ensureKeyboard(progNameInput)
                        mouse.accepted = false
                    }
                }
            }
        }
    }

    footer: Rectangle {
        color: "transparent"
        implicitHeight: 120

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            spacing: 18

            DialogActionButton {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                text: qsTr("ОТМЕНА")
                labelPixelSize: 34
                onPressed: reject()
            }

            Item { Layout.fillWidth: true }

            DialogActionButton {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                text: qsTr("ПРИНЯТЬ")
                labelPixelSize: 34
                primary: true
                enabled: progName.length > 0 && scopeName.length > 0
                onPressed: {
                    if (progName.length > 0
                            && scopeName.length > 0
                            && recomHandle.userProgExists(scopeName, progName)) {
                        overwriteConfirmationRequested()
                        return
                    }
                    accept()
                }
            }
        }
    }
}
