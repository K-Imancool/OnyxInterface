import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

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
                    color: root.fotekBlue
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
    property color fotekBlue: "#264093"
    property color fotekOrange: "#faa731"
    readonly property int screenMargin: 34
    readonly property int panelRadius: 20
    readonly property int headerHeight: 70
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
        color: "#F3F5F9"
    }

    Item {
        id: headerArea
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
            topMargin: root.screenMargin - 8
        }
        width: parent.width - root.screenMargin * 2
        height: root.headerHeight

        Text {
            text: root.foldersConfirmed
                  ? qsTr("СКАЧИВАНИЕ ПРОГРАММ ПОЛЬЗОВАТЕЛЯ")
                  : qsTr("ВЫБОР ПАПОК ДЛЯ СКАЧИВАНИЯ")
            anchors.centerIn: parent
            color: root.fotekBlue
            font.pixelSize: 36
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width
            lineHeight: 1.05
        }
    }

    Item {
        id: folderSelectPanel
        visible: !root.foldersConfirmed
        anchors {
            top: headerArea.bottom
            topMargin: 22
            left: parent.left
            right: parent.right
            bottom: footerArea.top
            bottomMargin: 20
            leftMargin: root.screenMargin
            rightMargin: root.screenMargin
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 16

            Text {
                Layout.fillWidth: true
                color: "black"
                wrapMode: Text.Wrap
                font.pixelSize: root.fontBody
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.15
                text: qsTr("Отметьте папки, программы из которых нужно скачать.")
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: root.panelRadius
                color: "#80FFFFFF"
                border.width: 1
                border.color: "#C5CAD3"

                ListView {
                    id: scopeList
                    anchors {
                        fill: parent
                        leftMargin: 10
                        topMargin: 10
                        bottomMargin: 10
                        rightMargin: 22
                    }
                    clip: true
                    model: scopeModel
                    spacing: 10
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar {
                        id: scopeScrollBar
                        policy: scopeList.contentHeight > scopeList.height
                                ? ScrollBar.AlwaysOn
                                : ScrollBar.AlwaysOff
                        width: 14
                        background: Rectangle {
                            implicitWidth: 14
                            radius: 7
                            color: "#D5DAE3"
                        }
                        contentItem: Rectangle {
                            implicitWidth: 14
                            radius: 7
                            color: root.fotekBlue
                            opacity: scopeScrollBar.pressed ? 1 : 0.8
                        }
                    }

                    delegate: Rectangle {
                        width: scopeList.width
                        height: 86
                        radius: 16
                        color: selected ? "#E8EEF8" : "#FFFFFF"
                        border.width: selected ? 2 : 1
                        border.color: selected ? root.fotekOrange : "#C5CAD3"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            Rectangle {
                                width: 34
                                height: 34
                                radius: 8
                                color: selected ? root.fotekOrange : "transparent"
                                border.width: 2
                                border.color: selected ? root.fotekOrange : root.fotekBlue

                                Text {
                                    anchors.centerIn: parent
                                    visible: selected
                                    text: "✓"
                                    color: "white"
                                    font.pixelSize: 22
                                    font.bold: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    color: root.fotekBlue
                                    font.pixelSize: root.fontBody
                                    font.bold: true
                                    elide: Text.ElideRight
                                    text: scopeName
                                }

                                Text {
                                    Layout.fillWidth: true
                                    color: "#5A6478"
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
                        color: "#5A6478"
                        font.pixelSize: root.fontBody
                        text: qsTr("Нет папок пользовательских программ")
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                DialogActionButton {
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 64
                    text: qsTr("Выбрать все")
                    secondaryColor: root.fotekBlue
                    secondaryBorderWidth: 1
                    secondaryBorderColor: "#1E3274"
                    cornerRadius: root.panelRadius
                    labelPixelSize: 24
                    enabled: scopeModel.count > 0
                    onPressed: root.setAllSelected(true)
                }

                DialogActionButton {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 64
                    text: qsTr("Снять все")
                    secondaryColor: root.fotekBlue
                    secondaryBorderWidth: 1
                    secondaryBorderColor: "#1E3274"
                    cornerRadius: root.panelRadius
                    labelPixelSize: 24
                    enabled: scopeModel.count > 0
                    onPressed: root.setAllSelected(false)
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 64
                    text: qsTr("ДАЛЕЕ")
                    primary: true
                    cornerRadius: root.panelRadius
                    labelPixelSize: 26
                    enabled: root.selectedCount() > 0
                    onPressed: root.startDownload()
                }
            }
        }
    }

    ScrollView {
        id: scrollView
        visible: root.foldersConfirmed
        anchors {
            top: headerArea.bottom
            topMargin: 22
            left: parent.left
            right: parent.right
            bottom: footerArea.top
            bottomMargin: 20
            leftMargin: root.screenMargin
            rightMargin: root.screenMargin
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
                    radius: root.panelRadius
                    color: "#80FFFFFF"
                    border.width: 1
                    border.color: root.wizardStep === 3
                                  ? "#2E7D32"
                                  : (httpUpload.active && httpUpload.accessPointClientConnected
                                     ? "#2E7D32"
                                     : (root.wizardStep >= 1 ? root.fotekOrange : root.fotekBlue))
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
                                   ? "#2E7D32"
                                   : (httpUpload.active && httpUpload.accessPointClientConnected
                                      ? "#2E7D32"
                                      : (root.wizardStep >= 1 ? root.fotekOrange : root.fotekBlue))
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
                            color: "black"
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
                            color: "black"
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
                            color: "#5A6478"
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
                                radius: 12
                                color: httpUpload.accessPointClientConnected ? "#E8F5E9" : "#FFF8E1"
                                border.width: 1
                                border.color: httpUpload.accessPointClientConnected ? "#2E7D32" : root.fotekOrange
                            }

                            ColumnLayout {
                                id: activeDetails
                                x: 9
                                y: 9
                                width: parent.width - 18
                                spacing: 8

                                Text {
                                    color: httpUpload.accessPointClientConnected ? "#2E7D32" : root.fotekOrange
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: root.fontBody
                                    font.bold: true
                                    text: httpUpload.accessPointClientConnected
                                          ? qsTr("Адрес страницы")
                                          : qsTr("Параметры Wi‑Fi")
                                }

                                Text {
                                    color: "black"
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: root.fontBody
                                    text: httpUpload.accessPointClientConnected
                                          ? httpUpload.baseUrl
                                          : (httpUpload.accessPointSsid + qsTr(" · пароль: ") + httpUpload.accessPointPassword)
                                }

                                Text {
                                    visible: httpUpload.accessPointStatusText.length > 0
                                    color: "#5A6478"
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: root.fontSmall
                                    text: httpUpload.accessPointStatusText
                                }
                            }
                        }

                        Text {
                            visible: root.wizardStep == 1
                            color: "#5A6478"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: root.fontCaption
                            text: qsTr("Серийный номер: ") + root.deviceSerialDisplay
                                  + qsTr(" · ") + qsTr("Тип аппарата: ") + root.deviceTypeDisplay
                                  + qsTr(" · Wi‑Fi: ") + (wifiProbe.wifiState ? qsTr("вкл") : qsTr("выкл"))
                        }

                        DialogActionButton {
                            text: qsTr("ПОВТОРИТЬ")
                            primary: true
                            Layout.preferredWidth: 280
                            Layout.preferredHeight: 64
                            cornerRadius: root.panelRadius
                            labelPixelSize: 26
                            visible: httpUpload.lastError.length > 0 || httpUpload.logArchiveError.length > 0
                            onPressed: root.startDownload()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 260
                        radius: 16
                        color: root.wizardStep === 3
                               ? "#E8F5E9"
                               : (httpUpload.active && httpUpload.qrImagePath.length > 0 ? "white" : "#EEF1F6")
                        border.width: 1
                        border.color: root.wizardStep === 3 || (httpUpload.active && httpUpload.accessPointClientConnected)
                                      ? "#2E7D32"
                                      : (root.wizardStep === 0 ? root.fotekBlue : (httpUpload.active ? root.fotekOrange : "#C5CAD3"))

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
                                color: root.fotekBlue
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
                            color: "#2E7D32"
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
                            color: "#5A6478"
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
                color: "#C62828"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: root.fontBody
                text: httpUpload.lastError.length > 0 ? httpUpload.lastError : httpUpload.logArchiveError
            }
        }
    }

    Item {
        id: footerArea
        height: 72
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: root.screenMargin
            rightMargin: root.screenMargin
            bottomMargin: root.screenMargin
        }

        DialogActionButton {
            id: returnButton
            width: 180
            height: parent.height
            text: qsTr("НАЗАД")
            secondaryColor: root.fotekBlue
            secondaryBorderWidth: 1
            secondaryBorderColor: "#1E3274"
            cornerRadius: root.panelRadius
            labelPixelSize: 30
            onPressed: {
                if (root.foldersConfirmed && root.wizardStep !== 3) {
                    root.foldersConfirmed = false
                    if (typeof httpUpload !== "undefined" && httpUpload.active)
                        httpUpload.stopSession()
                    return
                }
                root.returnButtonPressed()
            }
            anchors {
                left: parent.left
                verticalCenter: parent.verticalCenter
            }
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
