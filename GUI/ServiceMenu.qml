import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import StratifyLabs.UI 2.0

Item {
    id: serviceMenuRoot
    signal returnButtonPressed()
    signal serialNumberButtonPressed()
    signal softwareUpdateButtonPressed()
    signal wifiFileReceiveButtonPressed()
    signal wifiSettingsButtonPressed()
    signal touchScreenTestButtonPressed()
    signal aboutButtonPressed()
    signal logUpdateButtonPressed()
    signal specialCommandsButtonPressed()
    signal secretKeysButtonPressed()
    signal featureOptionsButtonPressed()
    signal isnCorrectionButtonPressed()
    signal deleteAllUserProgsRequested()

    property string accessLevel: "full"
    readonly property bool isLimitedAccess: accessLevel === "limited"
    readonly property int menuButtonWidth: 550
    readonly property int menuButtonHeight: 85
    readonly property int menuColumnsSpacing: 24

    function applySettingsReset() {
        if (typeof savedJson !== "undefined" && savedJson) {
            savedJson.saveString("serviceMenuNoPassword", "0")
            savedJson.saveString("fullscreenErrors", "1")
            savedJson.saveString("wifiAlwaysEnabled", "0")
        }
        if (typeof appControl !== "undefined" && appControl) {
            appControl.uartRate = 50
            appControl.debugUartEnabled = false
            appControl.cpuMonitorVisible = false
        }
        if (typeof periphHandle !== "undefined" && periphHandle) {
            periphHandle.fullscreenErrorsEnabled = true
        }
        deleteAllUserProgsRequested()
    }

    Rectangle {
        id: background
        anchors.fill: parent
        color: "darkslategray"
    }
    
    SLabel {
        id: screenTitle
        style: "label-primary lg"
        text: qsTr("Сервисное меню")
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }
    
    GridLayout {
        anchors {
            top: screenTitle.bottom
            topMargin: 24
            horizontalCenter: parent.horizontalCenter
        }
        columns: 2
        columnSpacing: serviceMenuRoot.menuColumnsSpacing
        rowSpacing: 14
        width: serviceMenuRoot.menuButtonWidth * 2 + serviceMenuRoot.menuColumnsSpacing

        SButton {
            id: specialCommandsButton
            visible: !serviceMenuRoot.isLimitedAccess
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("СПЕЦ КОМАНДЫ")
            onPressed: serviceMenuRoot.specialCommandsButtonPressed()
        }

        SButton {
            id: serialNumberButton
            visible: !serviceMenuRoot.isLimitedAccess
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Серийный номер")
            onPressed: serviceMenuRoot.serialNumberButtonPressed()
        }

        SButton {
            id: softwareUpdateButton
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Обновление ПО")
            onPressed: serviceMenuRoot.softwareUpdateButtonPressed()
        }

        SButton {
            id: wifiFileReceiveButton
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Приём файлов обновления")
            onPressed: serviceMenuRoot.wifiFileReceiveButtonPressed()
        }

        SButton {
            id: wifiSettingsButton
            visible: !serviceMenuRoot.isLimitedAccess
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Настройки WiFi")
            onPressed: serviceMenuRoot.wifiSettingsButtonPressed()
        }

        SButton {
            id: aboutButton
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Об аппарате")
            onPressed: serviceMenuRoot.aboutButtonPressed()
        }

        SButton {
            id: touchScreenTestButton
            visible: !serviceMenuRoot.isLimitedAccess
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Проверка тач-скрина")
            onPressed: serviceMenuRoot.touchScreenTestButtonPressed()
        }

        SButton {
            id: logUpdateButton
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Лог обновлений")
            onPressed: serviceMenuRoot.logUpdateButtonPressed()
        }

        SButton {
            id: secretKeysButton
            visible: !serviceMenuRoot.isLimitedAccess
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Секретные ключи")
            onPressed: serviceMenuRoot.secretKeysButtonPressed()
        }

        SButton {
            id: featureOptionsButton
            visible: !serviceMenuRoot.isLimitedAccess
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Управление опциями")
            onPressed: serviceMenuRoot.featureOptionsButtonPressed()
        }

        SButton {
            id: isnCorrectionButton
            visible: !serviceMenuRoot.isLimitedAccess
            style: "btn-primary lg"
            Layout.preferredWidth: serviceMenuRoot.menuButtonWidth
            Layout.preferredHeight: serviceMenuRoot.menuButtonHeight
            text: qsTr("Управление ИСН")
            onPressed: serviceMenuRoot.isnCorrectionButtonPressed()
        }
    }

    SButton {
        id: retButton
        style: "btn-secondary"
        text: qsTr("Назад")
        onPressed: returnButtonPressed()
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 15
        }
    }

    SButton {
        id: resetSettingsButton
        visible: !serviceMenuRoot.isLimitedAccess
        style: "btn-warning"
        text: qsTr("СБРОС НАСТРОЕК")
        onPressed: confirmResetDialog.open()
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 15
        }
    }

    Dialog {
        id: confirmResetDialog
        title: qsTr("Подтверждение сброса")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent

        Label {
            text: qsTr("Сбросить настройки?\nВход по паролю — включить, UART 50 мс,\nвывод UART и ЦП — отключить, ошибки на весь экран,\nWiFi всегда включен — отключить.\nПользовательские программы будут удалены.\nЭто действие необратимо.")
            font.pixelSize: 20
        }

        onAccepted: serviceMenuRoot.applySettingsReset()
    }
}
