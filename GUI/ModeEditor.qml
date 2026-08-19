import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Popup {
    id: root
    modal: true
    parent: Overlay.overlay
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    x: 0
    y: 0
    padding: 0

    property int socId
    property int modeIndex
    property bool isCoag
    property bool deferCommit: false
    property bool dialogAccepted: false
    property bool openingInProgress: false
    readonly property var modeEditor: Editor

    readonly property color fotekBlue: "#264093"
    readonly property color uiMidGray: "#5A6478"
    readonly property int screenMargin: 20
    readonly property int previewImageSize: 110
    readonly property int briefFontSize: 26
    readonly property int briefVerticalMargin: 10
    readonly property color previewBorderColor: "#C7CEDA"
    readonly property color listSelectedBackground: isCoag ? "#0B4FB3" : "#F4D13D"
    readonly property color listSelectedText: isCoag ? "white" : "black"

    property var itemIdArr: []
    property var itemNameArr: []
    property string imagePrefix: "mode"
    property var displayToBackend: []
    property int displayIndex: -1
    property int endoVariant: 1

    readonly property int endoCutIdFirst: ESHF.ENDO_I_0
    readonly property int endoCutIdLast: ESHF.ENDO_P_FORCE_3
    readonly property int endoCutGroupSize: 4
    readonly property int endoCutDefaultVariant: 1

    property int currentModeId: {
        var idx = modeEditor.currentModeIndex
        if (idx < 0 || idx >= itemIdArr.length)
            return 0
        return itemIdArr[idx]
    }

    readonly property bool showEndoVariantButtons: {
        if (!modeSelected || isCoag || displayIndex < 0 || displayIndex >= displayToBackend.length)
            return false
        var info = displayToBackend[displayIndex]
        return info && info.isGroup === true
    }

    background: Rectangle {
        color: "#F3F5F9"
    }

    ListModel {
        id: combinedModel
    }

    function isEndoCutId(id) {
        var n = parseInt(id)
        return !isCoag && n >= endoCutIdFirst && n <= endoCutIdLast
    }

    function endoCutGroupKey(id) {
        return Math.floor((parseInt(id) - endoCutIdFirst) / endoCutGroupSize)
    }

    function endoCutVariantOf(id) {
        return (parseInt(id) - endoCutIdFirst) % endoCutGroupSize
    }

    function stripEndoVariantSuffix(name) {
        return String(name).replace(/[\s\-]+[0-3]\s*$/, "")
    }

    function endoGroupDisplayName(id, name) {
        var stripped = stripEndoVariantSuffix(name).trim()
        if (stripped.length > 0)
            return stripped
        switch (endoCutGroupKey(id)) {
        case 0: return qsTr("ЭНДО И")
        case 1: return qsTr("ЭНДО И ФОРС")
        case 2: return qsTr("ЭНДО П")
        case 3: return qsTr("ЭНДО П ФОРС")
        default: return String(name)
        }
    }

    function preferredEndoVariant(info) {
        if (!info || !info.variants)
            return endoCutDefaultVariant
        if (info.variants[endoCutDefaultVariant] >= 0)
            return endoCutDefaultVariant
        for (var v = 0; v < endoCutGroupSize; ++v) {
            if (info.variants[v] >= 0)
                return v
        }
        return endoCutDefaultVariant
    }

    function backendIndexForDisplay(di, variant) {
        var info = displayToBackend[di]
        if (!info)
            return -1
        if (!info.isGroup)
            return info.backendIndex
        if (info.variants[variant] >= 0)
            return info.variants[variant]
        var fallback = preferredEndoVariant(info)
        if (info.variants[fallback] >= 0)
            return info.variants[fallback]
        return -1
    }

    function endoVariantAvailable(variant) {
        if (displayIndex < 0 || displayIndex >= displayToBackend.length)
            return false
        var info = displayToBackend[displayIndex]
        return !!(info && info.isGroup && info.variants[variant] >= 0)
    }

    function displayIndexForBackend(backendIndex) {
        for (var di = 0; di < displayToBackend.length; ++di) {
            var info = displayToBackend[di]
            if (!info)
                continue
            if (!info.isGroup) {
                if (info.backendIndex === backendIndex)
                    return di
                continue
            }
            for (var v = 0; v < endoCutGroupSize; ++v) {
                if (info.variants[v] === backendIndex)
                    return di
            }
        }
        return backendIndex >= 0 ? backendIndex : 0
    }

    function pinEndoGroupListAppearance() {
        for (var di = 0; di < displayToBackend.length; ++di) {
            var info = displayToBackend[di]
            if (!info || !info.isGroup)
                continue
            var bi = backendIndexForDisplay(di, preferredEndoVariant(info))
            if (bi < 0 || bi >= itemIdArr.length)
                continue
            combinedModel.setProperty(di, "itemId", itemIdArr[bi])
            combinedModel.setProperty(di, "itemName",
                                      endoGroupDisplayName(itemIdArr[bi], itemNameArr[bi]))
        }
    }

    function applyDisplaySelection(di, variant) {
        if (di < 0 || di >= displayToBackend.length)
            return
        var info = displayToBackend[di]
        displayIndex = di
        var selectedVariant = 0
        if (info.isGroup) {
            selectedVariant = (info.variants[variant] >= 0) ? variant : preferredEndoVariant(info)
            endoVariant = selectedVariant
        }
        var bi = backendIndexForDisplay(di, selectedVariant)
        if (bi >= 0)
            modeEditor.currentModeIndex = bi
        if (modeListView.curIndex !== di)
            modeListView.curIndex = di
    }

    function selectDisplayItem(index) {
        var info = displayToBackend[index]
        if (!info)
            return
        var variant = endoCutDefaultVariant
        if (info.isGroup && index === displayIndex && info.variants[endoVariant] >= 0)
            variant = endoVariant
        else if (info.isGroup)
            variant = preferredEndoVariant(info)
        applyDisplaySelection(index, variant)
    }

    function selectEndoVariant(variant) {
        if (!endoVariantAvailable(variant))
            return
        applyDisplaySelection(displayIndex, variant)
    }

    function selectedModeFullTitle() {
        var idx = modeEditor.currentModeIndex
        if (idx >= 0 && idx < itemNameArr.length) {
            var name = String(itemNameArr[idx])
            var id = itemIdArr[idx]
            if (isEndoCutId(id)) {
                if (/[\s\-]+[0-3]\s*$/.test(name))
                    return name
                return endoGroupDisplayName(id, name) + " " + endoCutVariantOf(id)
            }
            return name
        }
        var mode = modeEditor.currentMode
        if (mode && mode.name !== undefined && mode.name !== null) {
            var modeName = String(mode.name)
            if (isEndoCutId(mode.id)) {
                if (/[\s\-]+[0-3]\s*$/.test(modeName))
                    return modeName
                return endoGroupDisplayName(mode.id, modeName) + " " + endoCutVariantOf(mode.id)
            }
            return modeName
        }
        return ""
    }

    readonly property bool modeSelected: {
        var mode = modeEditor.currentMode
        if (mode && mode.id !== undefined && mode.id !== null)
            return parseInt(mode.id) !== ESHF.NO_MODE
        var idx = modeEditor.currentModeIndex
        if (idx < 0 || idx >= itemIdArr.length)
            return false
        return parseInt(itemIdArr[idx]) !== ESHF.NO_MODE
    }

    function deselectMode() {
        if (openingInProgress)
            return
        var idx = combinedModel.count - 1
        if (idx < 0)
            return
        modeListView.selectIndex(idx)
    }

    function updateModel() {
        combinedModel.clear()
        displayIndex = -1

        if (itemIdArr.length !== itemNameArr.length) {
            console.warn("Lists from C++ have different lengths!")
            return
        }

        var mapping = []
        var groupDisplayIndex = ({})
        for (var i = 0; i < itemIdArr.length; i++) {
            var id = parseInt(itemIdArr[i])
            if (isEndoCutId(id)) {
                var key = endoCutGroupKey(id)
                var variant = endoCutVariantOf(id)
                if (groupDisplayIndex[key] !== undefined) {
                    mapping[groupDisplayIndex[key]].variants[variant] = i
                    continue
                }
                var variants = [-1, -1, -1, -1]
                variants[variant] = i
                groupDisplayIndex[key] = mapping.length
                mapping.push({
                                 isGroup: true,
                                 groupKey: key,
                                 variants: variants
                             })
                combinedModel.append({
                                         itemId: itemIdArr[i],
                                         itemName: endoGroupDisplayName(id, itemNameArr[i])
                                     })
                continue
            }
            mapping.push({
                             isGroup: false,
                             backendIndex: i,
                             variants: [-1, -1, -1, -1]
                         })
            combinedModel.append({
                                     itemId: itemIdArr[i],
                                     itemName: itemNameArr[i]
                                 })
        }
        displayToBackend = mapping
        pinEndoGroupListAppearance()
        modeListView.innerModel = combinedModel
    }

    function closeWithoutCommit() {
        dialogAccepted = false
        if (!deferCommit)
            modeEditor.rollBack()
        root.close()
    }

    function commitAndClose() {
        dialogAccepted = true
        if (!deferCommit)
            modeEditor.commitChanges()
        root.close()
    }

    function acceptAndClose() {
        if (!modeEditor.hasChanges) {
            closeWithoutCommit()
            return
        }
        commitAndClose()
    }

    onAboutToShow: dialogAccepted = false

    onOpened: {
        openingInProgress = true
        modeEditor.initialize(socId, modeIndex, isCoag)

        itemNameArr = modeEditor.modeNames
        itemIdArr = modeEditor.modeNamesIds()

        updateModel()

        var idx = root.modeIndex
        if (idx < 0 || idx >= itemNameArr.length)
            idx = modeEditor.currentModeIndex
        if (idx < 0 || idx >= itemNameArr.length)
            idx = 0

        var di = displayIndexForBackend(idx)
        var info = displayToBackend[di]
        var variant = endoCutDefaultVariant
        if (info && info.isGroup)
            variant = endoCutVariantOf(itemIdArr[idx])
        applyDisplaySelection(di, variant)

        Qt.callLater(function() {
            if (modeListView.curIndex !== displayIndex)
                modeListView.curIndex = displayIndex
            modeListView.positionSelectedItem()
            Qt.callLater(function() {
                modeListView.positionSelectedItem()
                openingInProgress = false
            })
        })
    }

    Rectangle {
        anchors.fill: parent
        color: "#F3F5F9"

        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 78
            color: isCoag ? "#0B4FB3" : "#F4D13D"

            Label {
                anchors {
                    left: parent.left
                    right: closeButton.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: root.screenMargin
                    rightMargin: 12
                }
                text: !isCoag
                      ? qsTr("Выберите режим РЕЗАНИЯ для выхода %1").arg(modeEditor.socketName)
                      : qsTr("Выберите режим КОАГУЛЯЦИИ для выхода %1").arg(modeEditor.socketName)
                horizontalAlignment: Qt.AlignHCenter
                verticalAlignment: Qt.AlignVCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 34
                font.bold: true
                color: isCoag ? "white" : "black"
            }

            Button {
                id: closeButton
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    right: parent.right
                    rightMargin: root.screenMargin
                }
                width: 68
                onPressed: closeWithoutCommit()

                background: Rectangle {
                    color: "transparent"
                }

                contentItem: Text {
                    text: qsTr("X")
                    font.pixelSize: 34
                    font.bold: true
                    color: isCoag ? "white" : "black"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        RowLayout {
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            spacing: 9

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                color: "transparent"

                Button {
                    id: modeScrollUp
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 56
                    text: qsTr("▲")
                    font.pixelSize: 24
                    background: Rectangle {
                        radius: 18
                        color: "white"
                        border.color: root.fotekBlue
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: root.fotekBlue
                        font.pixelSize: 24
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: modeListView.scrollUp()
                }

                ItemList {
                    id: modeListView
                    anchors.top: modeScrollUp.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: modeScrollDown.top
                    anchors.bottomMargin: 10
                    curIndex: -1
                    imageSourceTemplate: "image://modes/" + imagePrefix + "%1"
                    selectedBackgroundColor: root.listSelectedBackground
                    selectedTextColor: root.listSelectedText
                    unselectedTextColor: root.fotekBlue
                    itemBackgroundColor: "transparent"
                    selectedBorderColor: "transparent"
                    itemBorderColor: "transparent"
                    selectedBorderWidth: 0
                    itemBorderWidth: 0
                    itemCornerRadius: 8
                    keepSelectedItemAtTop: true
                    selectedItemRowsAbove: 2
                    noAutoScrollItemId: ESHF.NO_MODE
                    itemFontPixelSize: 22
                }

                Button {
                    id: modeScrollDown
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 56
                    text: qsTr("▼")
                    font.pixelSize: 24
                    background: Rectangle {
                        radius: 18
                        color: "white"
                        border.color: root.fotekBlue
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: root.fotekBlue
                        font.pixelSize: 24
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onPressed: modeListView.scrollDown()
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "#C7CEDA"
            }

            ColumnLayout {
                id: previewPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                readonly property int briefAreaWidth: {
                    var w = previewPanel.width - previewImageSize - 12
                    return w > 10 ? w : 10
                }

                Label {
                    Layout.fillWidth: true
                    visible: root.modeSelected || selectedModeFullTitle().length > 0
                    text: {
                        var _ = root.currentModeId
                        return root.modeSelected ? selectedModeFullTitle() : qsTr("РЕЖИМ НЕ ВЫБРАН\n\n(ВЫКЛЮЧЕН)")
                    }
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 32
                    font.bold: true
                    color: fotekBlue
                }

                RowLayout {
                    id: briefRow
                    Layout.fillWidth: true
                    visible: root.modeSelected
                    Layout.preferredHeight: Math.max(previewImageSize, briefRect.briefContentHeight)
                    spacing: 12

                    Rectangle {
                        id: briefRect
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(previewImageSize, briefContentHeight)
                        Layout.minimumHeight: previewImageSize
                        Layout.alignment: Qt.AlignTop
                        radius: 20
                        color: "white"
                        border.width: 1
                        border.color: previewBorderColor

                        readonly property int briefTextWidth: {
                            var inner = width - 2 * briefVerticalMargin
                            if (inner > 1)
                                return inner
                            var fallback = previewPanel.briefAreaWidth - 2 * briefVerticalMargin
                            return fallback > 1 ? fallback : 1
                        }

                        readonly property int briefContentHeight:
                            Math.ceil(briefMeasure.paintedHeight) + 2 * briefVerticalMargin + 4

                        Text {
                            id: briefMeasure
                            visible: false
                            width: briefRect.briefTextWidth
                            text: modeEditor.modeBrief
                            font.pixelSize: briefFontSize
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            width: briefRect.briefTextWidth
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: briefVerticalMargin
                            text: modeEditor.modeBrief
                            color: root.fotekBlue
                            font.pixelSize: briefFontSize
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: previewImageSize
                        Layout.preferredHeight: previewImageSize
                        Layout.alignment: Qt.AlignTop
                        radius: 20
                        color: "white"
                        border.width: 1
                        border.color: previewBorderColor

                        Image {
                            anchors.fill: parent
                            anchors.margins: 8
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: ("image://modes/" + imagePrefix + "%1").arg(currentModeId)
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: root.modeSelected
                    radius: 20
                    color: "white"
                    border.width: 1
                    border.color: previewBorderColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: modeEditor.modeDescript
                            color: root.fotekBlue
                            font.pixelSize: 24
                            wrapMode: Text.WordWrap
                            elide: Text.ElideRight
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 106
                            Layout.maximumHeight: 106
                            visible: root.showEndoVariantButtons
                            spacing: 6

                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Выберите тип подачи импульсов")
                                color: root.fotekBlue
                                font.pixelSize: 22
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 62
                                Layout.maximumHeight: 62
                                spacing: 10

                                Repeater {
                                    model: 4
                                    Button {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 62
                                        Layout.maximumHeight: 62
                                        enabled: root.endoVariantAvailable(index)
                                        onPressed: root.selectEndoVariant(index)

                                        background: Rectangle {
                                            radius: 14
                                            color: root.endoVariant === index && parent.enabled
                                                   ? root.listSelectedBackground : "white"
                                            border.width: 1
                                            border.color: root.fotekBlue
                                            opacity: parent.enabled ? 1.0 : 0.4
                                        }
                                        contentItem: Text {
                                            text: String(index)
                                            color: root.endoVariant === index && parent.enabled
                                                   ? root.listSelectedText : root.fotekBlue
                                            font.pixelSize: 24
                                            font.bold: true
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 86
            color: "transparent"

            DialogActionButton {
                anchors.left: parent.left
                anchors.leftMargin: root.screenMargin
                anchors.verticalCenter: parent.verticalCenter
                width: 195
                height: 62
                text: qsTr("ОТМЕНА")
                secondaryColor: "white"
                secondaryBorderWidth: 2
                secondaryBorderColor: root.fotekBlue
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: root.fotekBlue
                onPressed: closeWithoutCommit()
            }

            DialogActionButton {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: 420
                height: 62
                visible: root.modeSelected
                text: qsTr("ВЫКЛЮЧИТЬ РЕЖИМ")
                secondaryColor: "white"
                secondaryBorderWidth: 2
                secondaryBorderColor: root.fotekBlue
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: root.fotekBlue
                onPressed: deselectMode()
            }

            DialogActionButton {
                anchors.right: parent.right
                anchors.rightMargin: root.screenMargin
                anchors.verticalCenter: parent.verticalCenter
                width: 195
                height: 62
                text: qsTr("ПРИНЯТЬ")
                primary: true
                enabled: true
                primaryEnabledColor: modeEditor.hasChanges ? root.fotekBlue : "#26409370"
                primaryDisabledColor: "#26409370"
                primaryBorderWidth: 1
                primaryBorderColor: "#1E3274"
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: "white"
                onPressed: acceptAndClose()
            }
        }
    }

    Connections {
        target: modeListView
        function onNewIndexSelected(index) {
            if (openingInProgress)
                return
            selectDisplayItem(index)
        }
    }
}
