import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0
import StratifyLabs.UI 2.0

Item {
    id: root
    signal returnButtonPressed()

    property bool foldersConfirmed: false

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

    function progCountText(n) {
        var n10 = n % 10
        var n100 = n % 100
        if (n10 === 1 && n100 !== 11)
            return n + qsTr(" программа")
        if (n10 >= 2 && n10 <= 4 && (n100 < 12 || n100 > 14))
            return n + qsTr(" программы")
        return n + qsTr(" программ")
    }

    function rebuildScopeModel() {
        scopeModel.clear()
        if (typeof userProgTransfer === "undefined" || !userProgTransfer)
            return
        var list = userProgTransfer.scopes
        for (var i = 0; i < list.length; ++i) {
            scopeModel.append({
                                  scopeId: list[i].id,
                                  scopeName: list[i].name,
                                  progCount: list[i].progCount,
                                  selected: false
                              })
        }
    }

    function selectedCount() {
        var n = 0
        for (var i = 0; i < scopeModel.count; ++i) {
            if (scopeModel.get(i).selected)
                n++
        }
        return n
    }

    function selectedScopeIds() {
        var ids = []
        for (var i = 0; i < scopeModel.count; ++i) {
            if (scopeModel.get(i).selected)
                ids.push(scopeModel.get(i).scopeId)
        }
        return ids
    }

    function setAllSelected(selected) {
        for (var i = 0; i < scopeModel.count; ++i)
            scopeModel.setProperty(i, "selected", selected)
    }

    function exportFileNamePreview(prefix) {
        if (typeof httpUpload === "undefined" || !httpUpload)
            return ""
        return httpUpload.buildUserProgExportFileName(prefix)
    }

    function beginDownloadWithFileName(fileName) {
        var ids = selectedScopeIds()
        if (ids.length === 0)
            return
        foldersConfirmed = true
        if (typeof httpUpload !== "undefined")
            httpUpload.startUserProgDownloadSession(ids, fileName)
    }

    function startDownload() {
        if (selectedCount() === 0)
            return
        exportFileNameDialog.open()
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
    readonly property bool wizardCompleted: wizardStep === 3

    ListModel {
        id: scopeModel
    }

    Component.onCompleted: {
        if (typeof userProgTransfer !== "undefined" && userProgTransfer)
            userProgTransfer.refreshScopes()
        rebuildScopeModel()
    }

    Component.onDestruction: {
        if (typeof httpUpload !== "undefined" && httpUpload.active)
            httpUpload.stopSession()
    }

    Connections {
        target: typeof userProgTransfer !== "undefined" ? userProgTransfer : null
        function onScopesChanged() {
            if (!root.foldersConfirmed)
                root.rebuildScopeModel()
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
        text: root.foldersConfirmed
              ? qsTr("СКАЧИВАНИЕ ПРОГРАММ ПОЛЬЗОВАТЕЛЯ")
              : qsTr("ВЫБОР ПАПОК ДЛЯ СКАЧИВАНИЯ")
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    Item {
        id: folderSelectPanel
        visible: !root.foldersConfirmed
        anchors {
            top: screenTitle.bottom
            left: parent.left
            right: parent.right
            bottom: returnButton.top
            margins: 15
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                Layout.fillWidth: true
                color: "white"
                wrapMode: Text.Wrap
                font.pixelSize: root.fontBody
                text: qsTr("Отметьте папки, программы из которых нужно скачать.")
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#122323"
                border.width: 1
                border.color: "#42a5f5"

                ListView {
                    id: scopeList
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    model: scopeModel
                    spacing: 8

                    delegate: Rectangle {
                        width: scopeList.width
                        height: 76
                        radius: 10
                        color: selected ? "#1E3274" : "#1a2a2a"
                        border.width: 2
                        border.color: selected ? "#faa731" : "#546e7a"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            Rectangle {
                                width: 32
                                height: 32
                                radius: 6
                                color: selected ? "#faa731" : "transparent"
                                border.width: 2
                                border.color: selected ? "#faa731" : "#90caf9"

                                Text {
                                    anchors.centerIn: parent
                                    visible: selected
                                    text: "✓"
                                    color: "#1a2a2a"
                                    font.pixelSize: 22
                                    font.bold: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    color: "white"
                                    font.pixelSize: root.fontBody
                                    font.bold: true
                                    elide: Text.ElideRight
                                    text: scopeName
                                }

                                Text {
                                    Layout.fillWidth: true
                                    color: "#cfd8dc"
                                    font.pixelSize: root.fontSmall
                                    text: root.progCountText(progCount)
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: scopeModel.setProperty(index, "selected", !selected)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: scopeModel.count === 0
                        color: "#cfd8dc"
                        font.pixelSize: root.fontBody
                        text: qsTr("Нет папок пользовательских программ")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                SButton {
                    text: qsTr("Выбрать все")
                    style: "btn-primary"
                    enabled: scopeModel.count > 0
                    onClicked: root.setAllSelected(true)
                }

                SButton {
                    text: qsTr("Снять все")
                    style: "btn-secondary"
                    enabled: scopeModel.count > 0
                    onClicked: root.setAllSelected(false)
                }

                Item { Layout.fillWidth: true }

                SButton {
                    text: qsTr("Далее")
                    style: "btn-success"
                    enabled: root.selectedCount() > 0
                    onClicked: root.startDownload()
                }
            }
        }
    }

    ScrollView {
        id: scrollView
        visible: root.foldersConfirmed
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
                Layout.preferredHeight: receivePanelRow.implicitHeight + 28

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
                    width: scrollView.width - 28
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
                                    return qsTr("4. Успешно передано")
                                if (root.wizardStep === 2)
                                    return qsTr("3. Откройте страницу скачивания")
                                if (root.wizardStep === 1)
                                    return qsTr("2. Подключитесь к Wi‑Fi")
                                if (root.wizardStep === 0)
                                    return qsTr("1. Подготовка файла")
                                return httpUpload.logArchiveError.length > 0
                                       ? qsTr("Не удалось подготовить файл")
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
                                    return qsTr("Файл программ сохранён на устройство.\nТочка доступа ONYX-SERVICE отключена.")
                                if (root.wizardStep === 2)
                                    return qsTr("Соединение установлено! Отсканируйте новый QR-код или откройте адрес вручную в любом браузере.")
                                if (root.wizardStep === 1) {
                                    var ssid = httpUpload.accessPointSsid.length > 0
                                               ? httpUpload.accessPointSsid
                                               : "ONYX-SERVICE"
                                    return qsTr("!Отключите мобильный интернет на вашем устройстве!\nОтсканируйте QR-код или выберите сеть %1 в списке Wi‑Fi.\nПосле обнаружения подключения QR-код изменится.").arg(ssid)
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
                            text: qsTr("Идёт подготовка файла программ. Подождите.")
                        }

                        Text {
                            visible: root.foldersConfirmed
                                     && root.wizardStep >= 1
                                     && root.wizardStep <= 3
                                     && httpUpload.userProgDownloadFileName.length > 0
                            color: "#bbdefb"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontCaption
                            text: qsTr("Файл: ") + httpUpload.userProgDownloadFileName
                        }

                        Item {
                            visible: httpUpload.active && root.wizardStep >= 1
                            Layout.fillWidth: true
                            Layout.preferredHeight: activeDetails.implicitHeight + 18

                            Rectangle {
                                anchors.fill: parent
                                radius: 8
                                color: httpUpload.accessPointClientConnected ? "#17331d" : "#3a2f12"
                                border.width: 1
                                border.color: httpUpload.accessPointClientConnected ? "#66bb6a" : "#ffca28"
                            }

                            ColumnLayout {
                                id: activeDetails
                                x: 9
                                y: 9
                                width: parent.width - 18
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
                            onClicked: root.startDownload()
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
                                text: qsTr("Подготовка файла")
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
        onPressed: {
            if (root.foldersConfirmed && root.wizardStep !== 3) {
                root.foldersConfirmed = false
                if (typeof httpUpload !== "undefined" && httpUpload.active)
                    httpUpload.stopSession()
                return
            }
            root.returnButtonPressed()
        }
    }

    NetworkSearch {
        id: wifiProbe
    }

    Dialog {
        id: exportFileNameDialog
        modal: true
        parent: Overlay.overlay
        width: Math.min(root.width * 0.92, 980)
        readonly property int overlayHeight: parent ? parent.height : root.height
        readonly property int keyboardReserve: {
            if (!Qt.inputMethod.visible)
                return 0
            var r = Qt.inputMethod.keyboardRectangle
            if (r.height > 0)
                return Math.max(0, overlayHeight - r.y)
            return Math.round(overlayHeight * 0.42)
        }
        height: Math.max(360, Math.min(560, overlayHeight - keyboardReserve - 32))
        x: parent ? (parent.width - width) / 2 : 0
        y: 16
        title: qsTr("Имя файла для сохранения")
        Overlay.modal: Rectangle {
            color: "#70000000"
        }

        function submit() {
            exportNameInput.focus = false
            Qt.inputMethod.hide()
            root.beginDownloadWithFileName(root.exportFileNamePreview(exportNameInput.text))
            close()
        }

        onOpened: {
            exportNameInput.text = ""
            Qt.callLater(function() {
                exportNameInput.forceActiveFocus()
                Qt.inputMethod.show()
            })
        }

        Connections {
            target: Qt.inputMethod
            enabled: exportFileNameDialog.visible
            function onVisibleChanged() {
                if (!Qt.inputMethod.visible && exportNameInput.activeFocus)
                    exportNameInput.focus = false
            }
        }

        contentItem: Rectangle {
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 22
                anchors.bottomMargin: 18
                spacing: 20

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: root.fontBody
                    text: qsTr("Введите имя файла. Пробелы и недопустимые символы будут заменены на «_», в конце добавляется тип аппарата.")
                }

                TextField {
                    id: exportNameInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    font.pixelSize: root.fontBody
                    placeholderText: qsTr("Например: ИВАНОВ ХИРУРГИЯ")
                    Keys.onReturnPressed: exportFileNameDialog.submit()
                    Keys.onEnterPressed: exportFileNameDialog.submit()
                    onActiveFocusChanged: {
                        if (activeFocus)
                            Qt.inputMethod.show()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: root.fontCaption
                    color: "#5A6478"
                    text: qsTr("Файл: ") + root.exportFileNamePreview(exportNameInput.text)
                }

//                Label {
//                    Layout.fillWidth: true
//                    wrapMode: Text.WordWrap
//                    font.pixelSize: root.fontSmall
//                    color: "#5A6478"
//                    text: typeof httpUpload !== "undefined" && httpUpload
//                          ? qsTr("Суффикс: ") + httpUpload.buildUserProgExportFileName("")
//                          : ""
//                }

                Item { Layout.fillHeight: true }
            }
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 132

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
                    onPressed: {
                        exportNameInput.focus = false
                        Qt.inputMethod.hide()
                        exportFileNameDialog.close()
                    }
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 220
                    Layout.fillHeight: true
                    text: qsTr("ПРИНЯТЬ")
                    labelPixelSize: 34
                    primary: true
                    onPressed: exportFileNameDialog.submit()
                }
            }
        }
    }
}
