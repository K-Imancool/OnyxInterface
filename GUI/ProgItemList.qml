import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Rectangle {
    id: recProgs

    signal clickedButton(int idx)
    signal programSelected(string scopeName, string progName, int scopeId, int progId)
    signal returnButtonPressed()

    color: "#F3F5F9"

    property var itemIdArr: []
    property var itemNameArr: []
    property bool loadClear: true
    required property bool recommended
    property bool editable: false
    property bool userSelectedScope: false
    property int pendingDeleteIndex: -1
    property bool pendingDeleteIsScope: false
    property string pendingDeleteName: ""
    property int pendingSubProgParentIndex: -1
    property string pendingSubProgParentName: ""

    readonly property color fotekBlue: "#264093"
    readonly property color fotekOrange: "#faa731"
    readonly property color uiMidGray: "#5A6478"
    readonly property color scopeSelectedBackground: fotekBlue
    readonly property color scopeSelectedText: "white"
    readonly property color progSelectedBackground: fotekOrange
    readonly property color progSelectedText: "black"
    readonly property int screenMargin: 20
    readonly property bool subProgramsPanelVisible: subProgsModel.count > 0
    readonly property int mainListPreferredWidth: 320
    readonly property int subListPreferredWidth: 170
    readonly property color subProgItemBackground: fotekOrange
    readonly property color subProgUnselectedText: "black"
    readonly property color subProgSelectedText: "black"

    ListModel {
        id: scopeModel
    }

    ListModel {
        id: progsModel
    }

    ListModel {
        id: subProgsModel
    }

    function updateModel() {
        progsModel.clear()
        progList.innerModel = progsModel
        progList.curIndex = -1
        closeSubProgramsPanel()

        if (!userSelectedScope || scopeList.curIndex < 0)
            return

        itemNameArr = recomHandle.progNameList
        itemIdArr = recomHandle.progIdList
        if (itemIdArr.length !== itemNameArr.length) {
            console.warn("Lists from C++ have different lengths!")
            return
        }
        for (var i = 0; i < itemIdArr.length; i++) {
            progsModel.append({
                                  itemId: itemIdArr[i],
                                  itemName: itemNameArr[i],
                                  rowIndex: i,
                                  hasSubPrograms: recommended && recomHandle.hasSubPrograms(i)
                              })
        }
        progList.innerModel = progsModel
    }

    function init() {
        var previousScopeId = -1
        if (userSelectedScope && scopeList.curIndex >= 0 && scopeList.curIndex < scopeModel.count)
            previousScopeId = scopeModel.get(scopeList.curIndex).itemId

        scopeModel.clear()
        itemNameArr = recomHandle.scopeNameList
        itemIdArr = recomHandle.scopeIdList
        if (itemIdArr.length !== itemNameArr.length) {
            console.warn("Lists from C++ have different lengths!")
            return
        }
        for (var i = 0; i < itemIdArr.length; i++) {
            scopeModel.append({
                                  itemId: itemIdArr[i],
                                  itemName: itemNameArr[i],
                                  rowIndex: i
                              })
        }
        scopeList.innerModel = scopeModel

        if (scopeModel.count <= 0) {
            userSelectedScope = false
            scopeList.curIndex = -1
            return
        }

        var restoredIndex = -1
        if (previousScopeId >= 0) {
            for (var j = 0; j < scopeModel.count; ++j) {
                if (scopeModel.get(j).itemId === previousScopeId) {
                    restoredIndex = j
                    break
                }
            }
        }

        if (restoredIndex >= 0) {
            userSelectedScope = true
            scopeList.curIndex = restoredIndex
        } else {
            userSelectedScope = false
            scopeList.curIndex = -1
        }
    }

    function requestDeleteScope(index) {
        if (index < 0 || index >= scopeModel.count)
            return

        pendingDeleteIndex = index
        pendingDeleteIsScope = true
        pendingDeleteName = scopeModel.get(index).itemName
        confirmDeleteDialog.open()
    }

    function requestDeleteProg(index) {
        if (index < 0 || index >= progsModel.count)
            return

        pendingDeleteIndex = index
        pendingDeleteIsScope = false
        pendingDeleteName = progsModel.get(index).itemName
        confirmDeleteDialog.open()
    }

    function scopeNameAtCurrentIndex() {
        return scopeList.curIndex >= 0 && scopeList.curIndex < scopeModel.count
                ? scopeModel.get(scopeList.curIndex).itemName
                : ""
    }

    function scopeIdAtCurrentIndex() {
        return scopeList.curIndex >= 0 && scopeList.curIndex < scopeModel.count
                ? scopeModel.get(scopeList.curIndex).itemId
                : -1
    }

    function loadSelectedProgram(progId, progName) {
        if (progId < 0)
            return false
        if (!appControl.loadProgram(progId, loadClear))
            return false

        recProgs.programSelected(scopeNameAtCurrentIndex(), progName,
                                 scopeIdAtCurrentIndex(), progId)
        recProgs.clickedButton(-1)
        return true
    }

    function subProgramSuffix(parentName, subName) {
        if (!subName)
            return ""
        if (!parentName)
            return subName

        var prefix = parentName + " "
        if (subName.indexOf(prefix) === 0)
            return subName.substring(prefix.length)

        var parentLower = parentName.toLowerCase()
        var subLower = subName.toLowerCase()
        if (subLower.indexOf(parentLower) === 0) {
            var rest = subName.substring(parentName.length).replace(/^\s+/, "")
            if (rest.length > 0)
                return rest
        }

        return subName
    }

    function closeSubProgramsPanel() {
        pendingSubProgParentIndex = -1
        pendingSubProgParentName = ""
        subProgsModel.clear()
        subProgList.curIndex = -1
    }

    function showSubProgramsPanel(index) {
        if (index < 0 || index >= progsModel.count)
            return

        var subPrograms = recomHandle.subProgramsAt(index)
        if (!subPrograms || subPrograms.length === 0) {
            closeSubProgramsPanel()
            return
        }

        if (pendingSubProgParentIndex === index && subProgramsPanelVisible)
            return

        pendingSubProgParentIndex = index
        pendingSubProgParentName = progsModel.get(index).itemName
        subProgsModel.clear()
        for (var i = 0; i < subPrograms.length; ++i) {
            subProgsModel.append({
                                     itemId: subPrograms[i].id,
                                     itemName: subProgramSuffix(pendingSubProgParentName, subPrograms[i].name),
                                     rowIndex: i
                                 })
        }
        subProgList.curIndex = -1
        progList.curIndex = index
    }

    Component.onCompleted: {
        recomHandle.isRecomProgs = recommended
    }

    Connections {
        target: recomHandle
        function onScopeNameListChanged() {
            init()
            updateModel()
        }
        function onScopeIdxChanged() {
            if (!recProgs.userSelectedScope)
                return
            if (recomHandle.scopeIdx >= 0 && recomHandle.scopeIdx < scopeModel.count)
                scopeList.curIndex = recomHandle.scopeIdx
        }
        function onProgNameListChanged() {
            updateModel()
        }
    }

    Rectangle {
        id: headerRect
        height: 78
        color: fotekBlue
        anchors {
            left: parent.left
            top: parent.top
            right: parent.right
        }

        Label {
            anchors.fill: parent
            anchors.leftMargin: screenMargin
            anchors.rightMargin: screenMargin
            text: recommended
                  ? qsTr("РЕКОМЕНДУЕМЫЕ ПРОГРАММЫ")
                  : qsTr("ПРОГРАММЫ ПОЛЬЗОВАТЕЛЯ")
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            wrapMode: Text.WordWrap
            font.pixelSize: 34
            font.bold: true
            color: "white"
        }
    }

    RowLayout {
        id: listsRow
        anchors {
            top: headerRect.bottom
            left: parent.left
            right: parent.right
            bottom: footer.top
            margins: 16
        }
        spacing: 9

        Rectangle {
            id: scopeRect
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredWidth: mainListPreferredWidth
            color: "transparent"

            Label {
                id: scopeLabel
                text: recommended
                      ? qsTr("Выберите область")
                      : qsTr("Выберите папку")
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 24
                font.bold: true
                color: uiMidGray
            }

            Button {
                id: scopeScrollUp
                anchors.top: scopeLabel.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                height: 44
                text: qsTr("▲")
                font.pixelSize: 24
                background: Rectangle {
                    radius: 18
                    color: "white"
                    border.color: fotekBlue
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: fotekBlue
                    font.pixelSize: 24
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onPressed: scopeList.scrollUp()
            }

            ItemList {
                id: scopeList
                anchors {
                    top: scopeScrollUp.bottom
                    topMargin: 10
                    left: parent.left
                    right: parent.right
                    bottom: scopeScrollDown.top
                    bottomMargin: 10
                }
                editable: !recProgs.recommended
                alwaysShowEditActions: !recProgs.recommended
                noImage: true
                hideNoImageSymbol: true
                selectedBackgroundColor: scopeSelectedBackground
                selectedTextColor: scopeSelectedText
                unselectedTextColor: fotekBlue
                itemBackgroundColor: "transparent"
                selectedBorderColor: "transparent"
                itemBorderColor: "transparent"
                selectedBorderWidth: 0
                itemBorderWidth: 0
                itemCornerRadius: 8
                keepSelectedItemAtTop: true
                noAutoScrollItemId: 1000
                itemFontPixelSize: 28
            }

            Button {
                id: scopeScrollDown
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 44
                text: qsTr("▼")
                font.pixelSize: 24
                background: Rectangle {
                    radius: 18
                    color: "white"
                    border.color: fotekBlue
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: fotekBlue
                    font.pixelSize: 24
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onPressed: scopeList.scrollDown()
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: "#C7CEDA"
        }

        Rectangle {
            id: progRect
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredWidth: mainListPreferredWidth
            color: "transparent"

            Label {
                id: progLabel
                text: qsTr("Выберите программу")
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 24
                font.bold: true
                color: uiMidGray
            }

            Button {
                id: progScrollUp
                anchors.top: progLabel.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                height: 44
                text: qsTr("▲")
                font.pixelSize: 24
                background: Rectangle {
                    radius: 18
                    color: "white"
                    border.color: fotekBlue
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: fotekBlue
                    font.pixelSize: 24
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onPressed: progList.scrollUp()
            }

            ItemList {
                id: progList
                anchors {
                    top: progScrollUp.bottom
                    topMargin: 10
                    left: parent.left
                    right: parent.right
                    bottom: progScrollDown.top
                    bottomMargin: 10
                }
                editable: !recProgs.recommended
                alwaysShowEditActions: !recProgs.recommended
                noImage: true
                hideNoImageSymbol: true
                scrollSelectsItem: false
                selectedBackgroundColor: progSelectedBackground
                selectedTextColor: progSelectedText
                unselectedTextColor: "white"
                itemBackgroundColor: fotekBlue
                selectedBorderColor: "transparent"
                itemBorderColor: "transparent"
                selectedBorderWidth: 0
                itemBorderWidth: 0
                itemCornerRadius: 8
                keepSelectedItemAtTop: true
                noAutoScrollItemId: 1000
                itemFontPixelSize: 28
                showExpandIndicator: recProgs.recommended
                expandIndicatorActiveIndex: pendingSubProgParentIndex
            }

            Button {
                id: progScrollDown
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 44
                text: qsTr("▼")
                font.pixelSize: 24
                background: Rectangle {
                    radius: 18
                    color: "white"
                    border.color: fotekBlue
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: fotekBlue
                    font.pixelSize: 24
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onPressed: progList.scrollDown()
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            color: "#C7CEDA"
            visible: subProgramsPanelVisible
        }

        Rectangle {
            id: subProgRect
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredWidth: subListPreferredWidth
            color: "transparent"
            visible: subProgramsPanelVisible

            Label {
                id: subProgLabel
                text: qsTr("Выберите вариант")
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 20
                font.bold: true
                color: uiMidGray
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            Button {
                id: subProgScrollUp
                anchors.top: subProgLabel.bottom
                anchors.topMargin: 8
                anchors.left: parent.left
                anchors.right: parent.right
                height: 44
                text: qsTr("▲")
                font.pixelSize: 24
                background: Rectangle {
                    radius: 18
                    color: "white"
                    border.color: fotekBlue
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: fotekBlue
                    font.pixelSize: 24
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onPressed: subProgList.scrollUp()
            }

            ItemList {
                id: subProgList
                anchors {
                    top: subProgScrollUp.bottom
                    topMargin: 10
                    left: parent.left
                    right: parent.right
                    bottom: subProgScrollDown.top
                    bottomMargin: 10
                }
                innerModel: subProgsModel
                noImage: true
                hideNoImageSymbol: true
                scrollSelectsItem: false
                selectedBackgroundColor: subProgItemBackground
                selectedTextColor: subProgSelectedText
                unselectedTextColor: subProgUnselectedText
                itemBackgroundColor: subProgItemBackground
                selectedBorderColor: fotekBlue
                itemBorderColor: "transparent"
                selectedBorderWidth: 2
                itemBorderWidth: 0
                itemCornerRadius: 8
                keepSelectedItemAtTop: true
                noAutoScrollItemId: 1000
                itemFontPixelSize: 22
            }

            Button {
                id: subProgScrollDown
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 44
                text: qsTr("▼")
                font.pixelSize: 24
                background: Rectangle {
                    radius: 18
                    color: "white"
                    border.color: fotekBlue
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: fotekBlue
                    font.pixelSize: 24
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onPressed: subProgList.scrollDown()
            }
        }
    }

    Rectangle {
        id: footer
        height: 86
        color: "transparent"
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        DialogActionButton {
            id: retButton
            anchors.left: parent.left
            anchors.leftMargin: screenMargin
            anchors.verticalCenter: parent.verticalCenter
            width: 180
            height: 62
            text: qsTr("НАЗАД")
            secondaryColor: "white"
            secondaryBorderWidth: 2
            secondaryBorderColor: fotekBlue
            cornerRadius: 20
            labelPixelSize: 30
            labelColor: fotekBlue
            onPressed: recProgs.returnButtonPressed()
        }

        DialogActionButton {
            id: addScopeButton
            visible: !recProgs.recommended
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: 500
            height: 62
            text: qsTr("ДОБАВИТЬ НОВУЮ ПАПКУ")
            secondaryColor: fotekBlue
            cornerRadius: 20
            labelPixelSize: 28
            labelColor: "white"
            onPressed: addScopeDialog.open()
        }
    }

    Dialog {
        id: confirmDeleteDialog
        title: ""
        modal: true
        width: Math.min(recProgs.width * 0.92, 980)
        height: 420
        x: (recProgs.width - width) / 2
        y: (recProgs.height - height) / 2

        contentItem: Rectangle {
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                anchors.topMargin: 26
                anchors.bottomMargin: 14
                spacing: 18

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Подтверждение удаления")
                    horizontalAlignment: Qt.AlignHCenter
                    font.pixelSize: 40
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: recProgs.pendingDeleteIsScope
                          ? qsTr("Область \"%1\" будет удалена. Продолжить?").arg(recProgs.pendingDeleteName)
                          : qsTr("Программа \"%1\" будет удалена. Продолжить?").arg(recProgs.pendingDeleteName)
                    horizontalAlignment: Qt.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 36
                }

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
                anchors.topMargin: 20
                anchors.bottomMargin: 20
                spacing: 18

                DialogActionButton {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    text: qsTr("ОТМЕНА")
                    labelPixelSize: 34
                    onPressed: confirmDeleteDialog.close()
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 240
                    Layout.fillHeight: true
                    text: qsTr("ПРИНЯТЬ")
                    primary: true
                    labelPixelSize: 34
                    onPressed: {
                        if (recProgs.pendingDeleteIndex >= 0) {
                            if (recProgs.pendingDeleteIsScope)
                                recomHandle.deleteScopeRequest(recProgs.pendingDeleteIndex)
                            else
                                recomHandle.deleteProgRequest(recProgs.pendingDeleteIndex)
                        }
                        confirmDeleteDialog.close()
                    }
                }
            }
        }

        onClosed: {
            recProgs.pendingDeleteIndex = -1
            recProgs.pendingDeleteName = ""
        }
    }

    Dialog {
        id: addScopeDialog
        modal: true
        parent: Overlay.overlay
        width: Math.min(recProgs.width * 0.92, 980)
        height: 360
        x: parent ? (parent.width - width) / 2 : 0
        y: 80
        title: qsTr("Новая папка")
        Overlay.modal: Rectangle {
            color: "#70000000"
        }

        function ensureKeyboard() {
            if (!newScopeEdit.activeFocus)
                newScopeEdit.forceActiveFocus()
            Qt.inputMethod.show()
        }

        function submit() {
            var newName = newScopeEdit.text.trim()
            newScopeEdit.focus = false
            Qt.inputMethod.hide()
            if (newName.length > 0)
                recomHandle.addScopeRequest(newName)
            close()
        }

        onOpened: {
            newScopeEdit.text = ""
            Qt.callLater(function() {
                newScopeEdit.forceActiveFocus()
                Qt.inputMethod.show()
            })
        }

        Connections {
            target: Qt.inputMethod
            enabled: addScopeDialog.visible
            function onVisibleChanged() {
                if (!Qt.inputMethod.visible && newScopeEdit.activeFocus)
                    newScopeEdit.focus = false
            }
        }

        contentItem: Rectangle {
            color: "white"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 16

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Qt.AlignCenter
                    text: qsTr("Укажите название папки:")
                    color: "black"
                    font.pixelSize: 34
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                TextField {
                    id: newScopeEdit
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    color: "black"
                    selectByMouse: true
                    activeFocusOnPress: true
                    inputMethodHints: Qt.ImhNoPredictiveText
                    font.pixelSize: 32
                    placeholderText: qsTr("Название")
                    background: Rectangle {
                        color: "#f5f5f5"
                        border.color: newScopeEdit.activeFocus ? "#4a9eff" : "#7a7a7a"
                        border.width: 2
                        radius: 6
                    }
                    onActiveFocusChanged: {
                        if (activeFocus)
                            Qt.inputMethod.show()
                    }
                    Keys.onReturnPressed: addScopeDialog.submit()
                    Keys.onEnterPressed: addScopeDialog.submit()

                    MouseArea {
                        anchors.fill: parent
                        propagateComposedEvents: true
                        onPressed: {
                            addScopeDialog.ensureKeyboard()
                            mouse.accepted = false
                        }
                    }
                }
            }
        }

        footer: Rectangle {
            color: "transparent"
            implicitHeight: 120

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
                        newScopeEdit.focus = false
                        Qt.inputMethod.hide()
                        addScopeDialog.reject()
                    }
                }

                Item { Layout.fillWidth: true }

                DialogActionButton {
                    Layout.preferredWidth: 220
                    Layout.fillHeight: true
                    text: qsTr("ПРИНЯТЬ")
                    primary: true
                    labelPixelSize: 34
                    enabled: newScopeEdit.text.trim().length > 0
                    onPressed: addScopeDialog.submit()
                }
            }
        }

        onRejected: {
            newScopeEdit.focus = false
            Qt.inputMethod.hide()
        }
    }

    Connections {
        target: scopeList
        function onNewIndexSelected(index) {
            recProgs.userSelectedScope = true
            recomHandle.scopeIdx = index
            recProgs.updateModel()
        }
        function onDeleteItem(index) {
            recProgs.requestDeleteScope(index)
        }
        function onEditItemName(index, name) {
            recomHandle.renameScopeRequest(index, name)
        }
    }

    Connections {
        target: subProgList
        function onNewIndexSelected(index) {
            if (index < 0 || index >= subProgsModel.count)
                return

            var subProgId = subProgsModel.get(index).itemId
            var subProgName = subProgsModel.get(index).itemName
            var fullProgName = recProgs.pendingSubProgParentName
            if (fullProgName.length > 0 && subProgName.length > 0)
                fullProgName += " — " + subProgName

            recProgs.loadSelectedProgram(subProgId, fullProgName)
        }
    }

    Connections {
        target: progList
        function onIndexHighlighted(index) {
            if (!recProgs.recommended)
                return

            if (recomHandle.hasSubPrograms(index))
                recProgs.showSubProgramsPanel(index)
            else
                recProgs.closeSubProgramsPanel()
        }
        function onNewIndexSelected(index) {
            if (recProgs.recommended && recomHandle.hasSubPrograms(index)) {
                recProgs.showSubProgramsPanel(index)
                return
            }

            recProgs.closeSubProgramsPanel()

            var pid = (index >= 0 && index < recomHandle.progIdList.length)
                    ? recomHandle.progIdList[index] : -1
            if (pid < 0)
                return

            var progName = index >= 0 && index < progsModel.count
                    ? progsModel.get(index).itemName
                    : ""

            recProgs.loadSelectedProgram(pid, progName)
        }
        function onDeleteItem(index) {
            recProgs.requestDeleteProg(index)
        }
        function onEditItemName(index, name) {
            recomHandle.renameProgRequest(index, name)
        }
    }
}
