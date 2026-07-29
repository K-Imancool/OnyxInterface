import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import StratifyLabs.UI 2.0

Item {
    id: specialCommandsRoot

    signal returnButtonPressed()
    signal deleteAllUserProgsRequested()

    property bool serviceMenuNoPassword: false

    readonly property int menuButtonWidth: 550
    readonly property int menuButtonHeight: 112
    readonly property int menuColumnsSpacing: 24

    function readServiceMenuNoPassword() {
        return typeof savedJson !== "undefined"
                && savedJson
                && savedJson.readString("serviceMenuNoPassword", "0") === "1"
    }

    function saveServiceMenuNoPassword(enabled) {
        serviceMenuNoPassword = enabled
        if (typeof savedJson !== "undefined" && savedJson) {
            savedJson.saveString("serviceMenuNoPassword", enabled ? "1" : "0")
        }
    }

    Component.onCompleted: {
        serviceMenuNoPassword = readServiceMenuNoPassword()
    }

    Rectangle {
        anchors.fill: parent
        color: "darkslategray"
    }

    SLabel {
        id: screenTitle
        style: "label-primary lg"
        text: qsTr("СПЕЦ КОМАНДЫ")
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    GridLayout {
        anchors {
            top: screenTitle.bottom
            topMargin: 36
            horizontalCenter: parent.horizontalCenter
        }
        columns: 2
        columnSpacing: specialCommandsRoot.menuColumnsSpacing
        rowSpacing: 20
        width: specialCommandsRoot.menuButtonWidth * 2 + specialCommandsRoot.menuColumnsSpacing

        SButton {
            id: noPasswordButton
            style: specialCommandsRoot.serviceMenuNoPassword ? "btn-danger lg" : "btn-primary lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: qsTr("Вход без пароля")
            onPressed: specialCommandsRoot.saveServiceMenuNoPassword(!specialCommandsRoot.serviceMenuNoPassword)
        }

        SButton {
            id: debugButton
            style: (typeof appControl !== "undefined" && appControl && appControl.debugUartEnabled)
                   ? "btn-danger lg" : "btn-primary lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: (typeof appControl !== "undefined" && appControl && appControl.debugUartEnabled)
                  ? qsTr("UART: ВКЛ") : qsTr("Вывод UART")
            onPressed: {
                if (typeof appControl === "undefined" || !appControl)
                    return
                appControl.debugUartEnabled = !appControl.debugUartEnabled
            }
        }

        SButton {
            id: uartRateButton
            style: (typeof appControl !== "undefined" && appControl && appControl.uartRate === 500)
                   ? "btn-outline-success lg" : "btn-primary lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: (typeof appControl !== "undefined" && appControl && appControl.uartRate === 500)
                  ? qsTr("UART 500 мс") : qsTr("UART 50 мс")
            onPressed: {
                if (typeof appControl === "undefined" || !appControl)
                    return
                appControl.uartRate = appControl.uartRate === 50 ? 500 : 50
            }
        }

        SButton {
            id: monitorButton
            style: (typeof appControl !== "undefined" && appControl && appControl.cpuMonitorVisible)
                   ? "btn-danger lg" : "btn-primary lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: (typeof appControl !== "undefined" && appControl && appControl.cpuMonitorVisible)
                  ? qsTr("ЦП: ВКЛ") : qsTr("Вывод загрузки ЦП")
            onPressed: {
                if (typeof appControl === "undefined" || !appControl)
                    return
                appControl.cpuMonitorVisible = !appControl.cpuMonitorVisible
            }
        }

        SButton {
            id: poweroffButton
            style: "btn-danger lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: qsTr("ВЫКЛ одноплатника")
            onPressed: {
                if (typeof appControl === "undefined" || !appControl)
                    return
                appControl.shutdownSystemFromUi()
            }
        }

        SButton {
            id: resetButton
            style: "btn-danger lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: qsTr("RESET одноплатника")
            onPressed: {
                if (typeof appControl === "undefined" || !appControl)
                    return
                appControl.resetSystemFromUi()
            }
        }

        SButton {
            id: exitButton
            style: "btn-danger lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: qsTr("Выход в Linux")
            onPressed: {
                if (typeof remoteUpdater === "undefined" || !remoteUpdater)
                    return
                remoteUpdater.stopDemo1UserServiceAndQuit()
            }
        }

        SButton {
            id: deleteAllUserProgsButton
            style: "btn-danger lg"
            Layout.preferredWidth: specialCommandsRoot.menuButtonWidth
            Layout.preferredHeight: specialCommandsRoot.menuButtonHeight
            text: qsTr("Удалить программы\nпользователя")
            onPressed: confirmDeleteDialog.open()
        }
    }

    SButton {
        id: returnButton
        style: "btn-secondary"
        text: qsTr("Назад")
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 15
        }
        onPressed: specialCommandsRoot.returnButtonPressed()
    }

    Dialog {
        id: confirmDeleteDialog
        title: qsTr("Подтверждение удаления")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent

        Label {
            text: qsTr("Удалить все пользовательские программы и папки?\nЭто действие необратимо.")
            font.pixelSize: 20
        }

        onAccepted: specialCommandsRoot.deleteAllUserProgsRequested()
    }
}
