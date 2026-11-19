import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0
import Ui 1.0

Dialog {
    id: root

    signal overwriteConfirmationRequested()

    readonly property string progName: progNameInput.text
    readonly property string scopeName: isNewScope ? newScopeNameInput.text : scopeNameBox.currentText

    property string originalProgName: ""
    property string originalScopeName: ""
    property bool currentProgramIsUser: false
    property bool isNewScope: false
    property bool keyboardChromeHeld: false

    modal: true
    padding: 0
    width: parent ? parent.width : 1280
    height: parent ? parent.height : 800
    x: 0
    y: 0

    function ensureKeyboard(field) {
        if (!field.activeFocus)
            field.forceActiveFocus()
        Qt.inputMethod.show()
    }

    function restoreInputFocus() {
        Qt.callLater(function() {
            progNameInput.forceActiveFocus()
            Qt.inputMethod.show()
        })
    }

    function clearTextFieldFocus() {
        if (progNameInput.activeFocus)
            progNameInput.focus = false
        if (newScopeNameInput.activeFocus)
            newScopeNameInput.focus = false
    }

    function holdKeyboardChrome() {
        if (keyboardChromeHeld)
            return
        KeyboardCoordinator.acquire(KeyboardCoordinator.actionBarHeight)
        keyboardChromeHeld = true
    }

    function releaseKeyboardChrome() {
        clearTextFieldFocus()
        Qt.inputMethod.hide()
        if (!keyboardChromeHeld)
            return
        KeyboardCoordinator.release()
        keyboardChromeHeld = false
    }

    onOpened: {
        console.log("openSaveDia")
        holdKeyboardChrome()
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

    onClosed: releaseKeyboardChrome()

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
        color: UiPalette.accent

        Label {
            anchors.fill: parent
            text: qsTr("Сохранение программы")
            font.bold: true
            font.pixelSize: 36
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            color: UiPalette.accentInk
        }
    }

    background: Rectangle {
        color: UiPalette.background
    }

    contentItem: Rectangle {
        color: "transparent"

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 32
            anchors.rightMargin: 32
            anchors.topMargin: 16
            anchors.bottomMargin: KeyboardCoordinator.keyboardGap(root.height) + 8
            spacing: 18

            Label {
                Layout.fillWidth: true
                text: qsTr("Выберите папку или создайте новую")
                horizontalAlignment: Qt.AlignHCenter
                font.pixelSize: 26
                font.bold: true
                wrapMode: Text.WordWrap
                color: UiPalette.textSecondary
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
                            font.pixelSize: 26
                            color: UiPalette.text
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
                    placeholderText: qsTr("Название папки")
                    color: UiPalette.text
                    font.pixelSize: 28
                    selectByMouse: true
                    activeFocusOnPress: true
                    inputMethodHints: Qt.ImhNoPredictiveText
                    background: Rectangle {
                        color: UiPalette.background
                        border.color: newScopeNameInput.activeFocus ? UiPalette.accent : UiPalette.border
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
                color: UiPalette.textSecondary
            }

            TextField {
                id: progNameInput
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                placeholderText: qsTr("Название")
                color: UiPalette.text
                font.pixelSize: 28
                selectByMouse: true
                activeFocusOnPress: true
                inputMethodHints: Qt.ImhNoPredictiveText
                background: Rectangle {
                    color: UiPalette.background
                    border.color: progNameInput.activeFocus ? UiPalette.accent : UiPalette.border
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

            Item { Layout.fillHeight: true }
        }
    }

    footer: Rectangle {
        color: UiPalette.background
        implicitHeight: KeyboardCoordinator.actionBarHeight

        DialogActionButton {
            anchors.left: parent.left
            anchors.leftMargin: KeyboardCoordinator.screenMargin
            anchors.verticalCenter: parent.verticalCenter
            width: KeyboardCoordinator.actionButtonWidth
            height: KeyboardCoordinator.actionButtonHeight
            text: qsTr("ОТМЕНА")
            labelPixelSize: 30
            secondaryColor: UiPalette.textSecondary
            onPressed: reject()
        }

        DialogActionButton {
            anchors.right: parent.right
            anchors.rightMargin: KeyboardCoordinator.screenMargin
            anchors.verticalCenter: parent.verticalCenter
            width: KeyboardCoordinator.actionButtonWidth
            height: KeyboardCoordinator.actionButtonHeight
            text: qsTr("ПРИНЯТЬ")
            labelPixelSize: 30
            primary: true
            primaryEnabledColor: "#264093"
            primaryDisabledColor: "#26409370"
            labelColor: "white"
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
