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

    property string accessLevel: "full"
    readonly property bool isLimitedAccess: accessLevel === "limited"
    readonly property int menuButtonWidth: 550
    readonly property int menuButtonHeight: 85
    readonly property int menuColumnsSpacing: 24
    
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
//        contentItem: Text {
//            text: retButton.text
//            font: retButton.font
//            opacity: enabled ? 1.0 : 0.3
//            color: "white"
//            horizontalAlignment: Text.AlignHCenter
//            verticalAlignment: Text.AlignVCenter
//            elide: Text.ElideRight
//        }
    }
}
