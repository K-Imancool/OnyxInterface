import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import StratifyLabs.UI 2.0

ServiceFrame {
    id: videoDeleteRoot
    title: qsTr("УДАЛЕНИЕ ВИДЕО")

    readonly property int fontBody: 26
    readonly property int fontSmall: 22
    readonly property int panelRadius: 16
    property string statusText: ""
    property bool statusIsError: false
    property int checkedCount: 0
    property real checkedSize: 0

    function formatSize(bytes) {
        var n = Number(bytes)
        if (!isFinite(n) || n < 0)
            n = 0
        if (n < 1024)
            return Math.round(n) + qsTr(" Б")
        if (n < 1024 * 1024)
            return (n / 1024).toFixed(1) + qsTr(" КБ")
        if (n < 1024 * 1024 * 1024)
            return (n / (1024 * 1024)).toFixed(1) + qsTr(" МБ")
        return (n / (1024 * 1024 * 1024)).toFixed(1) + qsTr(" ГБ")
    }

    function refreshSelectionStats() {
        var n = 0
        var sz = 0
        for (var i = 0; i < videoModel.count; ++i) {
            if (videoModel.get(i).selected) {
                n++
                sz += Number(videoModel.get(i).fileSize)
            }
        }
        checkedCount = n
        checkedSize = sz
    }

    function rebuildModel() {
        videoModel.clear()
        if (typeof AppPaths === "undefined" || !AppPaths) {
            videoDeleteRoot.statusText = qsTr("Каталог видео недоступен")
            videoDeleteRoot.statusIsError = true
            refreshSelectionStats()
            return
        }
        var list = AppPaths.listVideoFiles()
        for (var i = 0; i < list.length; ++i) {
            videoModel.append({
                                  fileName: list[i].fileName,
                                  fileSize: list[i].fileSize,
                                  selected: false
                              })
        }
        refreshSelectionStats()
    }

    function selectedFileNames() {
        var names = []
        for (var i = 0; i < videoModel.count; ++i) {
            if (videoModel.get(i).selected)
                names.push(videoModel.get(i).fileName)
        }
        return names
    }

    function setAllSelected(selected) {
        for (var i = 0; i < videoModel.count; ++i)
            videoModel.setProperty(i, "selected", selected)
        refreshSelectionStats()
    }

    function fileCountText(n) {
        var n10 = n % 10
        var n100 = n % 100
        if (n10 === 1 && n100 !== 11)
            return n + qsTr(" файл")
        if (n10 >= 2 && n10 <= 4 && (n100 < 12 || n100 > 14))
            return n + qsTr(" файла")
        return n + qsTr(" файлов")
    }

    function deleteSelected() {
        var names = selectedFileNames()
        if (names.length === 0)
            return
        if (typeof AppPaths === "undefined" || !AppPaths) {
            videoDeleteRoot.statusText = qsTr("Каталог видео недоступен")
            videoDeleteRoot.statusIsError = true
            return
        }
        var requested = names.length
        var deleted = AppPaths.deleteVideoFiles(names)
        rebuildModel()
        if (deleted === requested) {
            videoDeleteRoot.statusText = qsTr("Удалено: %1").arg(videoDeleteRoot.fileCountText(deleted))
            videoDeleteRoot.statusIsError = false
        } else {
            videoDeleteRoot.statusText = qsTr("Удалено %1 из %2. Часть файлов удалить не удалось.")
                    .arg(deleted).arg(requested)
            videoDeleteRoot.statusIsError = true
        }
    }

    ListModel {
        id: videoModel
    }

    Component.onCompleted: rebuildModel()

    ColumnLayout {
        anchors {
            top: screenTitle.bottom
            topMargin: 20
            left: parent.left
            right: parent.right
            bottom: retButton.top
            bottomMargin: 16
            leftMargin: 28
            rightMargin: 28
        }
        spacing: 14

        Text {
            Layout.fillWidth: true
            color: "white"
            wrapMode: Text.Wrap
            font.pixelSize: videoDeleteRoot.fontBody
            horizontalAlignment: Text.AlignHCenter
            lineHeight: 1.15
            text: qsTr("Отметьте видео, которые нужно удалить.")
        }

        Text {
            Layout.fillWidth: true
            color: videoDeleteRoot.statusIsError ? "#FF8A80" : "#B0C4C4"
            wrapMode: Text.Wrap
            font.pixelSize: videoDeleteRoot.fontSmall
            horizontalAlignment: Text.AlignHCenter
            visible: videoDeleteRoot.statusText.length > 0
            text: videoDeleteRoot.statusText
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: videoDeleteRoot.panelRadius
            color: "#102828"
            border.width: 1
            border.color: "#5f7f7f"

            ListView {
                id: videoList
                anchors {
                    fill: parent
                    leftMargin: 10
                    topMargin: 10
                    bottomMargin: 10
                    rightMargin: 22
                }
                clip: true
                model: videoModel
                spacing: 10
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    id: videoScrollBar
                    policy: videoList.contentHeight > videoList.height
                            ? ScrollBar.AlwaysOn
                            : ScrollBar.AlwaysOff
                    width: 14
                    background: Rectangle {
                        implicitWidth: 14
                        radius: 7
                        color: "#1a3535"
                    }
                    contentItem: Rectangle {
                        implicitWidth: 14
                        radius: 7
                        color: "#1D7DC5"
                        opacity: videoScrollBar.pressed ? 1 : 0.8
                    }
                }

                delegate: Rectangle {
                    width: videoList.width
                    height: 86
                    radius: 16
                    color: selected ? "#2a4a5a" : "#1a3535"
                    border.width: selected ? 2 : 1
                    border.color: selected ? "#1D7DC5" : "#5f7f7f"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 14

                        Rectangle {
                            width: 34
                            height: 34
                            radius: 8
                            color: selected ? "#1D7DC5" : "transparent"
                            border.width: 2
                            border.color: selected ? "#1D7DC5" : "#8AA0A0"

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
                                color: "white"
                                font.pixelSize: videoDeleteRoot.fontBody
                                font.bold: true
                                elide: Text.ElideRight
                                text: fileName
                            }

                            Text {
                                Layout.fillWidth: true
                                color: "#B0C4C4"
                                font.pixelSize: videoDeleteRoot.fontSmall
                                text: videoDeleteRoot.formatSize(fileSize)
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            videoModel.setProperty(index, "selected", !selected)
                            videoDeleteRoot.statusText = ""
                            videoDeleteRoot.refreshSelectionStats()
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: videoModel.count === 0
                    color: "#B0C4C4"
                    font.pixelSize: videoDeleteRoot.fontBody
                    text: qsTr("Нет видео в папке FOTEK/Video")
                }
            }
        }

        Text {
            Layout.fillWidth: true
            color: "#B0C4C4"
            wrapMode: Text.Wrap
            font.pixelSize: videoDeleteRoot.fontSmall
            horizontalAlignment: Text.AlignHCenter
            visible: videoDeleteRoot.checkedCount > 0
            text: qsTr("Выбрано: %1, %2")
                  .arg(videoDeleteRoot.fileCountText(videoDeleteRoot.checkedCount))
                  .arg(videoDeleteRoot.formatSize(videoDeleteRoot.checkedSize))
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            SButton {
                style: "btn-primary lg"
                Layout.preferredWidth: 240
                Layout.preferredHeight: 70
                text: qsTr("Выбрать все")
                enabled: videoModel.count > 0
                onPressed: videoDeleteRoot.setAllSelected(true)
            }

            SButton {
                style: "btn-primary lg"
                Layout.preferredWidth: 220
                Layout.preferredHeight: 70
                text: qsTr("Снять все")
                enabled: videoModel.count > 0
                onPressed: videoDeleteRoot.setAllSelected(false)
            }

            Item { Layout.fillWidth: true }

            SButton {
                style: "btn-danger lg"
                Layout.preferredWidth: 240
                Layout.preferredHeight: 70
                text: qsTr("Удалить")
                enabled: videoDeleteRoot.checkedCount > 0
                onPressed: confirmDeleteDialog.open()
            }
        }
    }

    Dialog {
        id: confirmDeleteDialog
        title: qsTr("Подтверждение удаления")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent

        Label {
            text: qsTr("Удалить %1?\nЭто действие необратимо.")
                  .arg(videoDeleteRoot.fileCountText(videoDeleteRoot.checkedCount))
            font.pixelSize: 20
        }

        onAccepted: videoDeleteRoot.deleteSelected()
    }
}
