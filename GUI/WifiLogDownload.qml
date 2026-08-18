import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0
import StratifyLabs.UI 2.0

Item {
    id: root
    signal returnButtonPressed()

    component ArchivePrepSpinner: Item {
        id: spinnerRoot
        implicitWidth: 64
        implicitHeight: 64

        Item {
            id: spinnerWheel
            anchors.fill: parent

            RotationAnimator {
                target: spinnerWheel
                from: 0
                to: 360
                duration: 1100
                loops: Animation.Infinite
                running: true
            }

            Repeater {
                id: spinnerDots
                model: 8
                Rectangle {
                    width: Math.max(8, spinnerRoot.width * 0.16)
                    height: width
                    radius: width / 2
                    color: "#42a5f5"
                    opacity: 0.25 + (index / spinnerDots.count) * 0.75
                    x: spinnerRoot.width / 2 - width / 2
                    y: spinnerRoot.height / 2 - height / 2
                    transform: [
                        Translate {
                            y: -Math.min(spinnerRoot.width, spinnerRoot.height) * 0.38
                        },
                        Rotation {
                            angle: index / spinnerDots.count * 360
                            origin.x: width / 2
                            origin.y: height / 2
                        }
                    ]
                }
            }
        }
    }

    function savedJsonFieldOrDash(key) {
        if (typeof savedJson === "undefined") {
            return "—"
        }
        var v = String(savedJson.readString(key, "")).trim()
        return v.length > 0 ? v : "—"
    }

    readonly property string deviceSerialDisplay: savedJsonFieldOrDash("serialNumber")
    readonly property string deviceTypeDisplay: savedJsonFieldOrDash("deviceType")
    readonly property int fontTitle: 36
    readonly property int fontBody: 26
    readonly property int fontCaption: 24
    readonly property int fontSmall: 22
    readonly property int wizardStep: {
        if (httpUpload.logDownloadCompleted)
            return 3
        if (httpUpload.logArchiveError.length > 0 && !httpUpload.logArchiveReady)
            return -1
        if (httpUpload.lastError.length > 0 && !httpUpload.active)
            return -1
        if (!httpUpload.logArchiveReady || !httpUpload.active)
            return 0
        if (!httpUpload.accessPointClientConnected)
            return 1
        return 2
    }

    Component.onCompleted: {
        Qt.callLater(function() {
            if (typeof httpUpload !== "undefined") {
                httpUpload.startLogDownloadSession()
            }
        })
    }

    Component.onDestruction: {
        if (typeof httpUpload !== "undefined" && httpUpload.active) {
            httpUpload.stopSession()
        }
    }

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "darkslategray"
    }

    SLabel {
        id: screenTitle
        style: "label-primary lg"
        text: qsTr("СКАЧИВАНИЕ ЛОГ-ФАЙЛА")
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    ScrollView {
        id: scrollView
        anchors {
            top: screenTitle.bottom
            left: parent.left
            right: parent.right
            bottom: returnButton.top
            margins: 15
        }
        clip: true
        contentWidth: width
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.width
            spacing: 14

            Item {
                Layout.fillWidth: true
                implicitHeight: receivePanelRow.implicitHeight + 28

                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: "#122323"
                    border.width: 1
                    border.color: root.wizardStep === 3
                                  ? "#66bb6a"
                                  : (httpUpload.active && httpUpload.accessPointClientConnected
                                     ? "#66bb6a"
                                     : (root.wizardStep >= 1 ? "#ffca28" : "#42a5f5"))
                }

                RowLayout {
                    id: receivePanelRow
                    x: 14
                    y: 14
                    width: parent.width - 28
                    spacing: 18

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 12

                        Text {
                            color: root.wizardStep === 3
                                   ? "#a5d6a7"
                                   : (httpUpload.active && httpUpload.accessPointClientConnected
                                      ? "#a5d6a7"
                                      : (root.wizardStep >= 1 ? "#ffe082" : "#90caf9"))
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontTitle
                            font.bold: true
                            text: {
                                if (root.wizardStep === 3)
                                    return qsTr("3. Успешно передано")
                                if (root.wizardStep === 2)
                                    return qsTr("3. Откройте страницу скачивания")
                                if (root.wizardStep === 1)
                                    return qsTr("2. Подключитесь к Wi‑Fi")
                                if (root.wizardStep === 0)
                                    return qsTr("1. Подготовка архива")
                                return httpUpload.logArchiveError.length > 0
                                       ? qsTr("Не удалось подготовить архив")
                                       : qsTr("Не удалось включить точку доступа")
                            }
                        }

                        Text {
                            visible: root.wizardStep !== 0
                            color: "white"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontBody
                            text: {
                                if (root.wizardStep === 3)
                                    return qsTr("Архив успешно передан. Точка доступа ONYX-SERVICE отключена.\nПодключитесь к сети Интернет и перешлите лог-файл в сервисную службу ООО «ФОТЕК».\nСпасибо!")
                                if (root.wizardStep === 2)
                                    return qsTr("Соединение установлено! Отсканируйте новый QR-код или откройте адрес вручную в любом браузере.")
                                if (root.wizardStep === 1) {
                                    var ssid = httpUpload.accessPointSsid.length > 0
                                               ? httpUpload.accessPointSsid
                                               : "ONYX-SERVICE"
                                    return qsTr("Аппарат раздал сеть \"%1\". Отключите мобильный интернет на вашем устройстве.\nОтсканируйте QR-код или выберите сеть %1 в списке Wi‑Fi.\nПосле обнаружения подключения QR-код изменится.").arg(ssid)
                                }
                                return httpUpload.logArchiveError.length > 0
                                       ? httpUpload.logArchiveError
                                       : qsTr("Проверьте Wi‑Fi и повторите включение точки доступа.")
                            }
                        }

                        Text {
                            visible: root.wizardStep === 0
                            color: "white"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontBody
                            text: qsTr("Идёт подготовка архива журналов. Подождите, это может занять некоторое время.")
                        }

                        Item {
                            visible: httpUpload.active && root.wizardStep >= 1
                            Layout.fillWidth: true
                            implicitHeight: activeDetails.implicitHeight + 18

                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: httpUpload.accessPointClientConnected ? "#17331d" : "#3a2f12"
                                border.width: 1
                                border.color: httpUpload.accessPointClientConnected ? "#66bb6a" : "#ffca28"
                            }

                            ColumnLayout {
                                id: activeDetails
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    top: parent.top
                                    margins: 9
                                }
                                spacing: 8

                                Text {
                                    color: httpUpload.accessPointClientConnected ? "#a5d6a7" : "#ffe082"
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: root.fontBody
                                    font.bold: true
                                    text: httpUpload.accessPointClientConnected
                                          ? qsTr("Адрес страницы")
                                          : qsTr("Параметры Wi‑Fi")
                                }

                                Text {
                                    color: "white"
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: root.fontBody
                                    text: httpUpload.accessPointClientConnected
                                          ? httpUpload.baseUrl
                                          : (httpUpload.accessPointSsid + qsTr(" · пароль: ") + httpUpload.accessPointPassword)
                                }

                                Text {
                                    visible: httpUpload.accessPointStatusText.length > 0
                                    color: "#cfd8dc"
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: root.fontSmall
                                    text: httpUpload.accessPointStatusText
                                }
                            }
                        }

                        Text {
                            visible: root.wizardStep == 1
                            color: "#e0f2f1"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontCaption
                            text: qsTr("Серийный номер: ") + root.deviceSerialDisplay
                                  + qsTr(" · ") + qsTr("Тип аппарата: ") + root.deviceTypeDisplay
                                  + qsTr(" · Wi‑Fi: ") + (wifiProbe.wifiState ? qsTr("вкл") : qsTr("выкл"))
                        }

                        SButton {
                            text: qsTr("Повторить")
                            style: "btn-primary"
                            Layout.preferredWidth: 320
                            visible: httpUpload.lastError.length > 0 || httpUpload.logArchiveError.length > 0
                            onClicked: httpUpload.startLogDownloadSession()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 260
                        radius: 10
                        color: root.wizardStep === 3
                               ? "#17331d"
                               : (httpUpload.active && httpUpload.qrImagePath.length > 0 ? "white" : "#263238")
                        border.width: 1
                        border.color: root.wizardStep === 3 || (httpUpload.active && httpUpload.accessPointClientConnected)
                                      ? "#66bb6a"
                                      : (root.wizardStep === 0 ? "#42a5f5" : (httpUpload.active ? "#ffca28" : "#78909c"))

                        Image {
                            visible: root.wizardStep > 0 && root.wizardStep < 3 && httpUpload.active && httpUpload.qrImagePath.length > 0
                            anchors.fill: parent
                            anchors.margins: 10
                            source: httpUpload.qrImagePath
                            fillMode: Image.PreserveAspectFit
                            cache: false
                        }

                        Column {
                            visible: root.wizardStep === 0
                            anchors.centerIn: parent
                            width: parent.width - 30
                            spacing: 12

                            ArchivePrepSpinner {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 72
                                height: 72
                            }

                            Text {
                                width: parent.width
                                color: "#bbdefb"
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                font.pixelSize: root.fontCaption
                                text: qsTr("Подготовка архива")
                            }
                        }

                        Text {
                            visible: root.wizardStep === 3
                            anchors.centerIn: parent
                            width: parent.width - 30
                            color: "#a5d6a7"
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontTitle
                            font.bold: true
                            text: qsTr("Готово")
                        }

                        Text {
                            visible: root.wizardStep !== 0 && root.wizardStep !== 3 && (!httpUpload.active || httpUpload.qrImagePath.length === 0)
                            anchors.centerIn: parent
                            width: parent.width - 30
                            color: "#cfd8dc"
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontCaption
                            text: httpUpload.active ? httpUpload.qrStatusText : qsTr("QR появится после создания точки доступа")
                        }
                    }
                }
            }

            Text {
                visible: httpUpload.lastError.length > 0 || httpUpload.logArchiveError.length > 0
                color: "#ffb3b3"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: root.fontBody
                text: httpUpload.lastError.length > 0 ? httpUpload.lastError : httpUpload.logArchiveError
            }
        }
    }

    DialogActionButton {
        id: returnButton
        width: 180
        height: 72
        text: qsTr("НАЗАД")
        secondaryColor: "#264093"
        secondaryBorderWidth: 1
        secondaryBorderColor: "#1E3274"
        cornerRadius: 20
        labelPixelSize: 30
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 15
        }
        onPressed: root.returnButtonPressed()
    }

    NetworkSearch {
        id: wifiProbe
    }
}
