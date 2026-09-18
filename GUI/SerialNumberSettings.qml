import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import StratifyLabs.UI 2.0

ServiceFrame {
    id: serialRoot
    title: qsTr("СЕРИЙНЫЙ НОМЕР И ТИП")
    backButtonVisible: !serialRoot.commentsEditing
    titleClickEnabled: serialRoot.commentsEditing
    onReturnButtonPressed: {
        serialInput.focus = false
        serialRoot.finishCommentsEditing()
    }
    onTitleClicked: serialRoot.finishCommentsEditing()

    property string serialNumber: ""
    property string deviceType: ""
    property string featureNotes: ""
    readonly property var deviceTypeOptions: ["ONYX-M", "ONYX-AM"]
    readonly property int minSerialNumber: 260000
    readonly property int maxSerialNumber: 1000000
    property bool serialValid: false
    property string serialSaveStatus: ""

    readonly property int rowHeight: 90
    readonly property int labelWidth: 280
    readonly property int labelFontSize: 36
    readonly property int valueFontSize: 48
    readonly property int statusFontSize: 28
    readonly property int notesFontSize: 32
    readonly property bool commentsEditing: featuresInput.activeFocus
    readonly property int bottomBarReserve: Math.max(retButton.height, saveButton.height) + 27
    property bool notesUpdating: false
    property int notesIdleHeight: 0
    property string lastFittingNotes: ""
    readonly property int keyboardInset: {
        if (!commentsEditing)
            return 0
        var fallback = Math.round(height * 0.42)
        var rect = Qt.inputMethod.keyboardRectangle
        if (Qt.inputMethod.visible && rect.height > 0)
            return Math.max(0, Math.round(height - rect.y))
        return fallback
    }

    function finishCommentsEditing() {
        featuresInput.focus = false
        Qt.inputMethod.hide()
    }

    function notesMaxInnerHeight() {
        var fieldHeight = notesIdleHeight > 0 ? notesIdleHeight : featuresInput.height
        return Math.max(0, Math.floor(fieldHeight - featuresInput.topPadding - featuresInput.bottomPadding))
    }

    function notesContentWidth() {
        return Math.max(1, Math.floor(featuresInput.width - featuresInput.leftPadding - featuresInput.rightPadding))
    }

    function notesTextFits(value) {
        var maxH = notesMaxInnerHeight()
        var maxW = notesContentWidth()
        if (maxH <= 0 || featuresInput.width <= 0)
            return true
        notesProbe.width = maxW
        notesProbe.text = value
        return notesProbe.contentHeight <= maxH + 1
    }

    function clipNotesToField(value) {
        if (!value || notesTextFits(value))
            return value
        var lo = 0
        var hi = value.length
        while (lo < hi) {
            var mid = Math.ceil((lo + hi) / 2)
            if (notesTextFits(value.substring(0, mid)))
                lo = mid
            else
                hi = mid - 1
        }
        return value.substring(0, lo)
    }

    function limitNotesText(value) {
        if (notesTextFits(value))
            return value
        if (lastFittingNotes.length > 0 && value.indexOf(lastFittingNotes) === 0)
            return clipNotesToField(value)
        if (lastFittingNotes.length > 0 && notesTextFits(lastFittingNotes))
            return lastFittingNotes
        return clipNotesToField(value)
    }

    function applyNotesLimit() {
        if (notesUpdating || featuresInput.width <= 0 || featuresInput.height <= 0)
            return
        var limited = limitNotesText(featuresInput.text)
        if (limited === featuresInput.text) {
            lastFittingNotes = limited
            featureNotes = limited
            return
        }
        var pos = Math.min(featuresInput.cursorPosition, limited.length)
        notesUpdating = true
        featuresInput.text = limited
        featuresInput.cursorPosition = pos
        notesUpdating = false
        lastFittingNotes = limited
        featureNotes = limited
    }

    function saveSettings() {
        if (!serialRoot.serialValid) {
            serialRoot.serialSaveStatus = qsTr("Введите серийный номер в диапазоне 260 000 - 1 000 000")
            return
        }

        if (typeof savedJson !== "undefined" && savedJson) {
            savedJson.saveString("serialNumber", serialRoot.serialNumber)
            savedJson.saveString("deviceType", serialRoot.deviceType)
            savedJson.saveString("featureNotes", serialRoot.featureNotes)
        }

        if (typeof remoteUpdater !== "undefined" && remoteUpdater) {
            remoteUpdater.serialNumber = serialRoot.serialNumber
        }

        if (typeof appControl !== "undefined" && appControl) {
            appControl.applyStoredDeviceType()
        }

        serialRoot.serialSaveStatus = qsTr("Сохранено")
    }

    Component.onCompleted: {
        if (typeof savedJson !== "undefined" && savedJson) {
            serialRoot.serialNumber = savedJson.readString("serialNumber", "")
            serialRoot.deviceType = savedJson.readString("deviceType", "")
            serialRoot.featureNotes = savedJson.readString("featureNotes", "")
        }
        if (deviceTypeOptions.indexOf(serialRoot.deviceType) < 0) {
            serialRoot.deviceType = deviceTypeOptions[1]
        }
        serialRoot.serialValid = isSerialValid(serialRoot.serialNumber)
        serialRoot.serialSaveStatus = serialRoot.serialValid
                                   ? qsTr("Сохранено")
                                   : qsTr("Введите серийный номер в диапазоне 260 000 - 1 000 000")
    }

    Component.onDestruction: {
        Qt.inputMethod.hide()
    }

    function isSerialValid(value) {
        if (!/^\d+$/.test(value)) {
            return false
        }
        var numeric = parseInt(value, 10)
        return numeric >= minSerialNumber && numeric <= maxSerialNumber
    }

    function setDeviceType(value) {
        serialRoot.deviceType = value
        if (serialRoot.serialValid) {
            serialRoot.serialSaveStatus = qsTr("Не сохранено")
        }
    }

    Connections {
        target: Qt.inputMethod
        function onVisibleChanged() {
            if (!Qt.inputMethod.visible && featuresInput.activeFocus)
                serialRoot.finishCommentsEditing()
        }
    }

    ColumnLayout {
        anchors {
            top: screenTitle.bottom
            topMargin: 28
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: serialRoot.commentsEditing
                          ? serialRoot.keyboardInset + 8
                          : serialRoot.bottomBarReserve
            leftMargin: 40
            rightMargin: 40
        }
        spacing: 22

        GridLayout {
            visible: !serialRoot.commentsEditing
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 22
            rowSpacing: 18

            Text {
                text: qsTr("Серийный номер")
                Layout.preferredWidth: serialRoot.labelWidth
                Layout.maximumWidth: serialRoot.labelWidth
                Layout.fillWidth: false
                Layout.preferredHeight: serialRoot.rowHeight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                color: "white"
                font.pixelSize: serialRoot.labelFontSize
                font.bold: true
            }

            TextField {
                id: serialInput
                Layout.fillWidth: true
                Layout.preferredHeight: serialRoot.rowHeight
                placeholderText: qsTr("Введите серийный номер")
                placeholderTextColor: "#8aa0b3"
                text: serialRoot.serialNumber
                font.pixelSize: serialRoot.valueFontSize
                font.bold: true
                color: "white"
                leftPadding: 20
                rightPadding: 20
                verticalAlignment: Text.AlignVCenter
                selectByMouse: true
                validator: IntValidator {
                    bottom: serialRoot.minSerialNumber
                    top: serialRoot.maxSerialNumber
                }
                inputMethodHints: Qt.ImhDigitsOnly
                onTextChanged: {
                    serialRoot.serialNumber = text
                    serialRoot.serialValid = serialRoot.isSerialValid(text)
                    if (serialRoot.serialValid) {
                        serialRoot.serialSaveStatus = qsTr("Не сохранено")
                    } else {
                        serialRoot.serialSaveStatus = qsTr("Введите серийный номер в диапазоне 260 000 - 1 000 000")
                    }
                }
                background: Rectangle {
                    color: "#1a2a3a"
                    border.color: serialRoot.serialValid
                                  ? "#4ade80"
                                  : (serialInput.activeFocus ? "#f87171" : "#3a4a5a")
                    border.width: 2
                    radius: 8
                }
            }

            Item {
                Layout.preferredWidth: serialRoot.labelWidth
            }

            Text {
                Layout.fillWidth: true
                text: serialRoot.serialSaveStatus
                color: serialRoot.serialValid ? "#4ade80" : "#fbbf24"
                font.pixelSize: serialRoot.statusFontSize
                wrapMode: Text.WordWrap
            }

            Text {
                text: qsTr("Тип аппарата")
                Layout.preferredWidth: serialRoot.labelWidth
                Layout.preferredHeight: serialRoot.rowHeight
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                color: "white"
                font.pixelSize: serialRoot.labelFontSize
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: serialRoot.rowHeight
                spacing: 22

                Repeater {
                    model: serialRoot.deviceTypeOptions
                    delegate: SButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: serialRoot.rowHeight
                        style: serialRoot.deviceType === modelData ? "btn-primary lg" : "btn-secondary lg"
                        text: modelData
                        onPressed: serialRoot.setDeviceType(modelData)
                    }
                }
            }
        }

        Text {
            text: qsTr("Комментарии")
            Layout.fillWidth: true
            color: "white"
            font.pixelSize: serialRoot.labelFontSize
            font.bold: true
        }

        TextArea {
            id: featuresInput
            Layout.fillWidth: true
            Layout.fillHeight: true
            placeholderText: qsTr("Дата производства, особенности")
            placeholderTextColor: "#8aa0b3"
            text: serialRoot.featureNotes
            wrapMode: Text.Wrap
            clip: true
            font.pixelSize: {
                var inner = Math.max(1, height - topPadding - bottomPadding)
                return Math.max(18, Math.min(serialRoot.notesFontSize, Math.floor(inner / 6)))
            }
            color: "white"
            leftPadding: 20
            rightPadding: 20
            topPadding: 16
            bottomPadding: 16
            selectByMouse: true
            inputMethodHints: Qt.ImhNoPredictiveText
            onActiveFocusChanged: {
                if (activeFocus)
                    Qt.inputMethod.show()
            }
            onWidthChanged: Qt.callLater(serialRoot.applyNotesLimit)
            onHeightChanged: {
                if (!serialRoot.commentsEditing && height > 1)
                    serialRoot.notesIdleHeight = height
                Qt.callLater(serialRoot.applyNotesLimit)
            }
            onTextChanged: {
                if (serialRoot.notesUpdating)
                    return
                var limited = serialRoot.limitNotesText(text)
                if (limited !== text) {
                    var pos = Math.min(cursorPosition, limited.length)
                    serialRoot.notesUpdating = true
                    text = limited
                    cursorPosition = pos
                    serialRoot.notesUpdating = false
                }
                serialRoot.lastFittingNotes = text
                serialRoot.featureNotes = text
                if (serialRoot.serialValid) {
                    serialRoot.serialSaveStatus = qsTr("Не сохранено")
                }
            }
            background: Rectangle {
                color: "#1a2a3a"
                border.color: featuresInput.activeFocus ? "#4a9eff" : "#3a4a5a"
                border.width: 2
                radius: 8
            }
        }
    }

    SButton {
        id: saveButton
        visible: !serialRoot.commentsEditing
        style: "btn-primary"
        text: qsTr("Сохранить")
        enabled: serialRoot.serialValid
        onPressed: {
            serialRoot.saveSettings()
        }
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 15
        }
    }

    TextEdit {
        id: notesProbe
        visible: false
        readOnly: true
        wrapMode: TextEdit.Wrap
        textFormat: TextEdit.PlainText
        font: featuresInput.font
    }
}
