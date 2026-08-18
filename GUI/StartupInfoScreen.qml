import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: startupInfo

    signal returnButtonPressed()

    property color fotekBlue: "#264093"
    property color fotekOrange: "#faa731"
    readonly property string deviceTypeText: {
        var value = String(savedJson.readString("deviceType", "")).trim()
        return value === "" ? qsTr("Не указан") : value
    }
    readonly property string serialNumberText: {
        var value = String(savedJson.readString("serialNumber", "")).trim()
        return value === "" ? qsTr("Не указан") : value
    }
    readonly property string softwareVersionsText: buildSoftwareVersionsText()

    function normalizedVersion(value) {
        var text = (value === undefined || value === null) ? "" : String(value).trim()
        return text.length > 0 && text !== "—" ? text : "0.0"
    }

    function moduleAt(index) {
        if (typeof mcFirmware === "undefined" || !mcFirmware
                || !mcFirmware.modules || mcFirmware.modules.length <= index) {
            return null
        }
        return mcFirmware.modules[index]
    }

    function formatModuleAppVersion(mod, compact) {
        if (!mod) {
            return compact ? "0.0" : "—"
        }
        if (mod.appStatus === "corrupted" || mod.appCorrupted) {
            return compact ? qsTr("повр") : qsTr("повреждена")
        }
        if (mod.appStatus === "missing") {
            return compact ? qsTr("нет") : qsTr("нет")
        }
        if (mod.appStatus === "unknown") {
            return compact ? "0.0" : "—"
        }
        var text = String(mod.appMain) + "." + String(mod.appSub)
        return compact ? normalizedVersion(text) : text
    }

    function moduleAppVersion(index) {
        return formatModuleAppVersion(moduleAt(index), true)
    }

    function moduleBootAndAppVersion(index) {
        var mod = moduleAt(index)
        if (!mod) {
            return "0.0/0.0"
        }
        return normalizedVersion(mod.bootMain + "." + mod.bootSub)
                + "/" + formatModuleAppVersion(mod, true)
    }

    function buildSoftwareVersionsText() {
        var interfaceVersion = normalizedVersion(typeof appVersion !== "undefined" ? appVersion : "")
        var mediaVersion = normalizedVersion(typeof httpUpload !== "undefined" && httpUpload
                                             ? httpUpload.currentMediaVersion : "")

        return "IF-" + interfaceVersion
                + ":MF-" + mediaVersion
                + ":S-" + moduleBootAndAppVersion(0)
                + ":G-" + moduleBootAndAppVersion(2)
                + ":A-" + moduleBootAndAppVersion(1)
                + ":R-" + moduleAppVersion(3)
                + ":N-" + moduleAppVersion(4)
    }

    Rectangle {
        anchors.fill: parent
        color: "#F4F6FA"
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 28
        radius: 28
        color: "white"
        border.width: 2
        border.color: startupInfo.fotekBlue

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 34
            spacing: 22

            Label {
                Layout.fillWidth: true
                text: qsTr("Информация об аппарате")
                color: startupInfo.fotekBlue
                font.pixelSize: 34
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: startupInfo.fotekOrange
            }

            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 920)
                    spacing: 14

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 130
                        radius: 22
                        color: "#F7F9FD"
                        border.width: 1
                        border.color: "#D3D9E6"

                        Column {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            Label {
                                width: parent.width
                                text: qsTr("Тип аппарата")
                                color: "#616161"
                                font.pixelSize: 22
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: startupInfo.deviceTypeText
                                color: "black"
                                font.pixelSize: 34
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 130
                        radius: 22
                        color: "#F7F9FD"
                        border.width: 1
                        border.color: "#D3D9E6"

                        Column {
                            anchors.fill: parent
                            anchors.margins: 24
                            spacing: 12

                            Label {
                                width: parent.width
                                text: qsTr("Серийный номер")
                                color: "#616161"
                                font.pixelSize: 22
                                font.bold: true
                            }

                            Label {
                                width: parent.width
                                text: startupInfo.serialNumberText
                                color: "black"
                                font.pixelSize: 34
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        radius: 22
                        color: "#F7F9FD"
                        border.width: 1
                        border.color: "#D3D9E6"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 10

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Версии ПО")
                                color: "#616161"
                                font.pixelSize: 22
                                font.bold: true
                            }

                            Label {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: startupInfo.softwareVersionsText
                                color: "black"
                                font.pixelSize: 26
                                font.bold: true
                                wrapMode: Text.WrapAnywhere
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            DialogActionButton {
                Layout.alignment: Qt.AlignLeft
                Layout.preferredWidth: 180
                Layout.preferredHeight: 66
                text: qsTr("НАЗАД")
                secondaryColor: startupInfo.fotekBlue
                secondaryBorderWidth: 1
                secondaryBorderColor: "#1C2F6B"
                onPressed: startupInfo.returnButtonPressed()
            }
        }
    }
}
