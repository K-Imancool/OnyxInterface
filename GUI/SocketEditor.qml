import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Popup {
    id: root
    modal: true
    parent: Overlay.overlay
    width: parent.width
    height: parent.height
    x: 0
    y: 0
    padding: 0

    background: Rectangle {
        color: "#F3F5F9"
    }

    property int socId: -1
    property int modeIndex: -1
    property bool isCoag: false
    readonly property var modeEditor: Editor

    property var modeIds: []
    property var instrNums: []
    property string centerView: "power" // power, modePreview, instrPreview
    property int committedModeIndex: -1
    property int committedInstrIndex: -1
    property bool internalIndexChange: false
    property bool openingInProgress: false
    property int pendingAutoMode: -1
    property string autoModeConfirmText: ""
    property var autoModeBaseline: [0, 0, 0, 0]
    property int autoDelayBaseline: 0
    property int socketAutoModeState: 0
    property bool editorDataReady: false
    property int preparedSocId: -1
    property int preparedModeIndex: -1
    property bool preparedIsCoag: false
    readonly property color fotekBlue: "#264093"
    readonly property color fotekOrange: "#faa731"
    readonly property int screenMargin: 20
    readonly property int controlButtonHeight: 80
    readonly property int recommendedButtonHeight: 90
    readonly property int recommendedEndoButtonHeight: 70
    readonly property color powerStepButtonBg: "white"
    readonly property color powerStepButtonText: fotekBlue
    readonly property int powerStepButtonWidth: 110
    readonly property color uiMidGray: "#5A6478"
    readonly property color autoBtnOnFill: fotekOrange
    readonly property color autoBtnOnBorder: "#1E3274"
    readonly property color autoBtnOffFill: "white"
    readonly property color autoBtnOffBorder: "#C7CEDA"
    readonly property color autoBtnOffText: fotekBlue
    readonly property color listSelectedBackground: isCoag ? "#0B4FB3" : "#F4D13D"
    readonly property color listSelectedText: isCoag ? "white" : "black"

    function modeImagePrefix() {
        return "mode"
    }

    function isModeLockedAt(index) {
        if (typeof featureUnlock === "undefined" || !featureUnlock) {
            return false
        }
        if (index < 0 || index >= modeIds.length) {
            return false
        }
        return featureUnlock.isModeLocked(parseInt(modeIds[index]))
    }

    function updateModeModel() {
        modeModel.clear()
        modeIds = modeEditor.modeNamesIds()
        var modeNames = modeEditor.modeNames
        for (var i = 0; i < modeNames.length; ++i) {
            modeModel.append({
                                 itemId: i < modeIds.length ? modeIds[i] : "0",
                                 itemName: modeNames[i],
                                 locked: isModeLockedAt(i)
                             })
        }
        modeListView.innerModel = modeModel
    }

    function updateInstrModel() {
        instrModel.clear()
        instrNums = modeEditor.instrListNums()
        var instrNames = modeEditor.instrList
        for (var i = 0; i < instrNames.length; ++i) {
            instrModel.append({
                                  itemId: i < instrNums.length ? instrNums[i] : "0",
                                  itemName: instrNames[i]
                              })
        }
        instrListView.innerModel = instrModel
    }

    function usesUnitPowerStep(modeId) {
        return modeId === ESHF.BI_COAG_MICRO
    }

    function nextPowerUp(power, maxPower, unitStep) {
        var value = power
        if (unitStep)
            value += 1
        else if (value < 20)
            value += 1
        else if (value < 50)
            value += 2
        else if (value < 100)
            value += 5
        else if (value < 200)
            value += 10
        else if (value < 400)
            value += 25
        return Math.min(value, maxPower)
    }

    function nextPowerDown(power, unitStep) {
        if (unitStep)
            return Math.max(power - 1, 1)
        var value = power
        if (value <= 1) value = 1
        else if (value <= 20) value -= 1
        else if (value <= 50) value -= 2
        else if (value <= 100) value -= 5
        else if (value <= 200) value -= 10
        else if (value <= 400) value -= 25
        return Math.max(value, 1)
    }

    property bool powerRepeatIncrease: false

    function stepMainPower(increase) {
        if (!modeSelected() || modeEditor.isEndo || modeEditor.currentMode.maxpower <= 0) {
            return
        }
        var unitStep = usesUnitPowerStep(currentModeId())
        if (increase) {
            modeEditor.updateParameter("currentpower",
                                       nextPowerUp(modeEditor.currentPower,
                                                   modeEditor.currentMode.maxpower,
                                                   unitStep))
        } else {
            modeEditor.updateParameter("currentpower", nextPowerDown(modeEditor.currentPower, unitStep))
        }
    }

    function startMainPowerRepeat(increase) {
        powerRepeatIncrease = increase
        stepMainPower(increase)
        powerRepeatDelay.restart()
    }

    function stopMainPowerRepeat() {
        powerRepeatDelay.stop()
        powerRepeatTick.stop()
    }

    Timer {
        id: powerRepeatDelay
        interval: 1500
        repeat: false
        onTriggered: powerRepeatTick.start()
    }

    Timer {
        id: powerRepeatTick
        interval: 120
        repeat: true
        onTriggered: stepMainPower(powerRepeatIncrease)
    }

    function modeTitleText() {
        var currentMode = modeEditor.currentMode
        if (!currentMode || currentMode.name === undefined || currentMode.name === null) {
            return ""
        }
        return currentMode.name
    }

    function endoPulseRateText() {
        var match = modeTitleText().match(/-([1-3])\s*$/)
        if (!match) {
            return qsTr("Подача импульсов СРЕДНЯЯ")
        }
        switch (parseInt(match[1])) {
        case 1:
            return qsTr("Подача импульсов РЕДКАЯ")
        case 2:
            return qsTr("Подача импульсов СРЕДНЯЯ")
        case 3:
            return qsTr("Подача импульсов ЧАСТАЯ")
        default:
            return qsTr("Подача импульсов СРЕДНЯЯ")
        }
    }

    function currentInstrName() {
        if (modeEditor.currentInstrIndex < 0 || modeEditor.currentInstrIndex >= modeEditor.instrList.length) {
            return ""
        }
        return modeEditor.instrList[modeEditor.currentInstrIndex]
    }

    function instrumentSelected() {
        if (modeEditor.currentInstrIndex < 0 || modeEditor.currentInstrIndex >= instrNums.length) {
            return false
        }
        return parseInt(instrNums[modeEditor.currentInstrIndex]) !== 1000
    }

    function instrumentIconSource() {
        if (!instrumentSelected()) {
            return ""
        }
        return "image://instruments/"
                + (isCoag ? "coaginstr" : "cutinstr")
                + instrNums[modeEditor.currentInstrIndex]
    }

    function socketAutoMode() {
        return socketAutoModeState
    }

    function setSocketAutoMode(mode) {
        socketAutoModeState = mode
        periphHandle.setAutoMode(socId, mode)
    }

    function requestAutoMode(mode, confirmationText) {
        if (mode === 1 && isBiCoagDissMode())
            return
        var currentMode = socketAutoMode()
        if (currentMode === mode) {
            setSocketAutoMode(0)
            return
        }
        pendingAutoMode = mode
        autoModeConfirmText = confirmationText
        autoModeConfirmPopup.open()
    }

    function isSoftMode() {
        return currentModeId() === ESHF.SOFT
    }

    function isTermoMode() {
        var modeId = currentModeId()
        return modeId === ESHF.TERMOSHOV || modeId === ESHF.TERMOSHOV_A
    }

    function isBiCoagMode() {
        var modeId = currentModeId()
        return modeId === ESHF.BI_COAG || modeId === ESHF.BI_COAG_DISS || modeId === ESHF.BI_COAG_MICRO
    }

    function isBiCoagDissMode() {
        return currentModeId() === ESHF.BI_COAG_DISS
    }

    function allowsBiAutoStop() {
        return isBiCoagMode() && !isBiCoagDissMode()
    }

    function dropDisallowedAutoStop() {
        if (socId > 1 || !isCoag)
            return
        if (isBiCoagDissMode() && socketAutoModeState === 1)
            setSocketAutoMode(0)
    }

    function captureAutoModeBaseline() {
        autoModeBaseline = [periphHandle.autoMode(0), periphHandle.autoMode(1),
                            periphHandle.autoMode(2), periphHandle.autoMode(3)]
        autoDelayBaseline = periphHandle.autoDelayMs
    }

    function syncAutoModeDirtyFromAuto() {
        var diff = false
        for (var i = 0; i < 4; i++) {
            if (periphHandle.autoMode(i) !== autoModeBaseline[i]) {
                diff = true
                break
            }
        }
        if (!diff && periphHandle.autoDelayMs !== autoDelayBaseline) {
            diff = true
        }
        modeEditor.setAutoModeDirty(diff)
    }

    function restoreAutoModesFromBaseline() {
        for (var i = 0; i < 4; i++) {
            periphHandle.setAutoMode(i, autoModeBaseline[i])
        }
        periphHandle.setAutoDelayMs(autoDelayBaseline)
        socketAutoModeState = periphHandle.autoMode(socId)
    }

    function cancelEditorAndClose() {
        restoreAutoModesFromBaseline()
        modeEditor.setAutoModeDirty(false)
        modeEditor.rollBack()
        root.close()
    }

    function isMonopolarSocket() {
        var socketName = modeEditor.socketName ? String(modeEditor.socketName).toUpperCase() : ""
        return socketName.indexOf("МОНО") !== -1 || socketName.indexOf("MONO") !== -1
    }

    function neutralMaxPower() {
        return neutralPowerWarningDialog.maxPowerForNeutralSize(periphHandle.neutralSize)
    }

    function needsNeutralPowerWarning() {
        if (!isMonopolarSocket() || !modeSelected())
            return false

        var maxPower = neutralMaxPower()
        if (maxPower >= 400)
            return false

        return modeEditor.currentPower > maxPower
    }

    function finishCommitAndClose() {
        modeEditor.commitChanges()
        recomHandle.saveCurrentState()
        root.close()
    }

    function attemptCommitAndClose() {
        if (neutralPowerWarningDialog.opened)
            return

        if (!modeEditor.hasChanges) {
            root.close()
            return
        }

        if (needsNeutralPowerWarning()) {
            neutralPowerWarningDialog.socketName = modeEditor.socketName
            neutralPowerWarningDialog.maxNeutralPower = neutralMaxPower()
            neutralPowerWarningDialog.showWarning()
            return
        }

        finishCommitAndClose()
    }

    function currentModeId() {
        var currentMode = modeEditor.currentMode
        if (!currentMode || currentMode.id === undefined || currentMode.id === null) {
            return -1
        }
        return parseInt(currentMode.id)
    }

    function modeSelected() {
        if (modeEditor.currentModeIndex < 0) {
            return false
        }
        if (modeEditor.modeNames.length > 0 && modeEditor.currentModeIndex === modeEditor.modeNames.length - 1) {
            return false
        }
        var currentMode = modeEditor.currentMode
        if (currentMode && currentMode.id !== undefined && currentMode.id !== null) {
            return currentMode.id !== ESHF.NO_MODE
        }
        return true
    }

    function endoCutEffect() {
        return Math.floor(modeEditor.currentPower / 10)
    }

    function endoCoagEffect() {
        return modeEditor.currentPower % 10
    }

    function setEndoPower(cutEffect, coagEffect) {
        var cut = Math.max(1, Math.min(3, cutEffect))
        var coag = Math.max(1, Math.min(3, coagEffect))
        modeEditor.updateParameter("currentpower", cut * 10 + coag)
    }

    function normalizeEndoCurrentPower() {
        if (!modeEditor.isEndo) {
            return
        }
        var cut = endoCutEffect()
        var coag = endoCoagEffect()
        var cutClamped = Math.max(1, Math.min(3, cut))
        var coagClamped = Math.max(1, Math.min(3, coag))
        var packed = cutClamped * 10 + coagClamped
        if (packed !== modeEditor.currentPower) {
            modeEditor.updateParameter("currentpower", packed)
        }
    }

    function restoreCommittedSelection() {
        internalIndexChange = true
        modeEditor.currentModeIndex = committedModeIndex
        modeEditor.currentInstrIndex = committedInstrIndex
        modeListView.curIndex = committedModeIndex
        instrListView.curIndex = committedInstrIndex
        internalIndexChange = false
        centerView = "power"
    }

    function visibleRecommendedPowers() {
        var result = []
        if (modeEditor.lowPowerBound !== 0)
            result.push(modeEditor.lowPowerBound)
        if (modeEditor.midPowerBound !== 0)
            result.push(modeEditor.midPowerBound)
        if (modeEditor.highPowerBound !== 0)
            result.push(modeEditor.highPowerBound)
        return result
    }

    function isRecommendedPowerSelected() {
        var bounds = visibleRecommendedPowers()
        for (var i = 0; i < bounds.length; ++i) {
            if (modeEditor.currentPower === bounds[i])
                return true
        }
        return bounds.length === 0
    }

    function defaultRecommendedPower() {
        var bounds = visibleRecommendedPowers()
        if (bounds.length === 0)
            return modeEditor.currentPower
        if (bounds.length <= 2)
            return bounds[0]
        return bounds[1]
    }

    function ensureRecommendedPowerIfNeeded() {
        if (!modeSelected())
            return
        if (!isRecommendedPowerSelected())
            modeEditor.updateParameter("currentpower", defaultRecommendedPower())
    }

    function applyPreviewSelection() {
        ensureRecommendedPowerIfNeeded()
        committedModeIndex = modeEditor.currentModeIndex
        committedInstrIndex = modeEditor.currentInstrIndex
        centerView = "power"
    }

    function editorParamsMatch() {
        return editorDataReady
               && preparedSocId === socId
               && preparedModeIndex === modeIndex
               && preparedIsCoag === isCoag
    }

    function prepareEditorData() {
        if (socId < 0 || modeIndex < 0)
            return
        if (editorParamsMatch())
            return

        modeEditor.initialize(socId, modeIndex, isCoag)
        updateModeModel()
        updateInstrModel()
        preparedSocId = socId
        preparedModeIndex = modeIndex
        preparedIsCoag = isCoag
        editorDataReady = true
    }

    onAboutToShow: prepareEditorData()

    onClosed: editorDataReady = false

    onOpened: {
        openingInProgress = true
        internalIndexChange = true
        prepareEditorData()
        committedModeIndex = modeEditor.currentModeIndex
        committedInstrIndex = modeEditor.currentInstrIndex
        centerView = "power"
        socketAutoModeState = periphHandle.autoMode(socId)
        dropDisallowedAutoStop()
        captureAutoModeBaseline()
        modeEditor.setAutoModeDirty(false)
        modeListView.curIndex = modeEditor.currentModeIndex
        instrListView.curIndex = modeEditor.currentInstrIndex
        normalizeEndoCurrentPower()
        Qt.callLater(function() {
            modeListView.positionSelectedItem()
            instrListView.positionSelectedItem()
            internalIndexChange = false
            openingInProgress = false
        })
    }

    ListModel { id: modeModel }
    ListModel { id: instrModel }

    Connections {
        target: typeof featureUnlock !== "undefined" ? featureUnlock : null
        function onActivatedKeysChanged() {
            if (root.visible) {
                updateModeModel()
            }
        }
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
                id: titleLabel
                anchors {
                    left: parent.left
                    right: closeButton.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: root.screenMargin
                    rightMargin: 12
                }
                text: isCoag
                      ? qsTr("Настройка КОАГУЛЯЦИИ для выхода %1").arg(modeEditor.socketName)
                      : qsTr("Настройка РЕЗАНИЯ для выхода %1").arg(modeEditor.socketName)
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
                onPressed: cancelEditorAndClose()

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
            anchors.bottom: centerView === "power" ? footer.top : parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 16
            spacing: 9

            Rectangle {
                id: modeRect
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                color: "transparent"

                Label {
                    text: qsTr("Выберите режим")
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: 24
                    font.bold: true
                    color: uiMidGray
                }

                ItemList {
                    id: modeListView
                    anchors.top: parent.top
                    anchors.topMargin: 28
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: modeButtons.top
                    curIndex: modeEditor.currentModeIndex
                    imageSourceTemplate: "image://modes/" + modeImagePrefix() + "%1"
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
                    noAutoScrollItemId: ESHF.NO_MODE
                    itemFontPixelSize: 22
                }

                RowLayout {
                    id: modeButtons
                    height: 56
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 10

                    Button {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                    Button {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "#C7CEDA"
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 320
                color: "transparent"

                Label {
                    text: qsTr("Выберите инструмент")
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: 24
                    font.bold: true
                    color: uiMidGray
                }

                ItemList {
                    id: instrListView
                    anchors.top: parent.top
                    anchors.topMargin: 28
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: instrButtons.top
                    curIndex: modeEditor.currentInstrIndex
                    noImage: true
                    hideNoImageSymbol: true
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
                    noAutoScrollItemId: 1000
                    itemFontPixelSize: 22
                }

                RowLayout {
                    id: instrButtons
                    height: 56
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 10

                    Button {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                        onPressed: instrListView.scrollUp()
                    }
                    Button {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                        onPressed: instrListView.scrollDown()
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: "#C7CEDA"
            }

            Rectangle {
                id: centerPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 24
                color: "transparent"

                ColumnLayout {
                    anchors {
                        fill: parent
                        leftMargin: 10
                        rightMargin: 10
                    }
                    spacing: modeEditor.isEndo ? 10 : 14
                    visible: centerView === "power"

                    Label {
                        Layout.fillWidth: true
                        text: modeTitleText()
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 38
                        font.bold: true
                        color: root.fotekBlue
                    }

                    Image {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 400
                        Layout.preferredHeight: 140
                        visible: instrumentSelected()
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        source: instrumentIconSource()
                    }

                    Item {
                        height: 50
                        visible: !modeEditor.isEndo && !isBiCoagMode() && !isTermoMode() && !instrumentSelected()
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Установите мощность")
                        horizontalAlignment: Text.AlignHCenter
                        visible: modeSelected() && !modeEditor.isEndo
                        color: uiMidGray
                        font.pixelSize: 24
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: controlButtonHeight
                        spacing: 16
                        visible: modeSelected() && !modeEditor.isEndo
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 24 }
                        Button {
                            Layout.preferredWidth: powerStepButtonWidth
                            Layout.preferredHeight: controlButtonHeight
                            text: qsTr("−")
                            enabled: modeEditor.currentMode.maxpower > 0
                            flat: true
                            background: Rectangle {
                                radius: 16
                                color: enabled ? powerStepButtonBg : "#E8ECF2"
                                border.width: enabled ? 2 : 1
                                border.color: enabled ? root.fotekOrange : "#C7CEDA"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: enabled ? powerStepButtonText : uiMidGray
                                font.pixelSize: 54
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onPressed: startMainPowerRepeat(false)
                            onReleased: stopMainPowerRepeat()
                            onCanceled: stopMainPowerRepeat()
                        }
                        Label {
                            Layout.preferredWidth: 140
                            text: modeEditor.currentPower
                            font.pixelSize: 52
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: root.fotekBlue
                        }
                        Button {
                            Layout.preferredWidth: powerStepButtonWidth
                            Layout.preferredHeight: controlButtonHeight
                            text: qsTr("+")
                            enabled: modeEditor.currentMode.maxpower > 0
                            flat: true
                            background: Rectangle {
                                radius: 16
                                color: enabled ? powerStepButtonBg : "#E8ECF2"
                                border.width: enabled ? 2 : 1
                                border.color: enabled ? root.fotekOrange : "#C7CEDA"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: enabled ? powerStepButtonText : uiMidGray
                                font.pixelSize: 44
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onPressed: startMainPowerRepeat(true)
                            onReleased: stopMainPowerRepeat()
                            onCanceled: stopMainPowerRepeat()
                        }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 24 }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Установите эффект резания")
                        visible: modeSelected() && modeEditor.isEndo
                        horizontalAlignment: Text.AlignHCenter
                        color: uiMidGray
                        font.pixelSize: 24
                        font.bold: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: controlButtonHeight
                        spacing: 16
                        visible: modeSelected() && modeEditor.isEndo

                        Item { Layout.fillWidth: true; Layout.minimumWidth: 24 }
                        Button {
                            Layout.preferredWidth: powerStepButtonWidth
                            Layout.preferredHeight: controlButtonHeight
                            text: qsTr("−")
                            flat: true
                            background: Rectangle {
                                radius: 16
                                color: powerStepButtonBg
                                border.width: 2
                                border.color: root.fotekOrange
                            }
                            contentItem: Text {
                                text: parent.text
                                color: powerStepButtonText
                                font.pixelSize: 48
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onPressed: setEndoPower(endoCutEffect() - 1, endoCoagEffect())
                        }
                        Label {
                            Layout.preferredWidth: 140
                            text: endoCutEffect()
                            color: root.fotekBlue
                            font.pixelSize: 52
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Button {
                            Layout.preferredWidth: powerStepButtonWidth
                            Layout.preferredHeight: controlButtonHeight
                            text: qsTr("+")
                            flat: true
                            background: Rectangle {
                                radius: 16
                                color: powerStepButtonBg
                                border.width: 2
                                border.color: root.fotekOrange
                            }
                            contentItem: Text {
                                text: parent.text
                                color: powerStepButtonText
                                font.pixelSize: 48
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onPressed: setEndoPower(endoCutEffect() + 1, endoCoagEffect())
                        }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 24 }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Установите эффект коагуляции")
                        visible: modeSelected() && modeEditor.isEndo
                        horizontalAlignment: Text.AlignHCenter
                        color: uiMidGray
                        font.pixelSize: 24
                        font.bold: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: controlButtonHeight
                        spacing: 16
                        visible: modeSelected() && modeEditor.isEndo

                        Item { Layout.fillWidth: true; Layout.minimumWidth: 24 }
                        Button {
                            Layout.preferredWidth: powerStepButtonWidth
                            Layout.preferredHeight: controlButtonHeight
                            text: qsTr("−")
                            flat: true
                            background: Rectangle {
                                radius: 16
                                color: powerStepButtonBg
                                border.width: 2
                                border.color: root.fotekOrange
                            }
                            contentItem: Text {
                                text: parent.text
                                color: powerStepButtonText
                                font.pixelSize: 48
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onPressed: setEndoPower(endoCutEffect(), endoCoagEffect() - 1)
                        }
                        Label {
                            Layout.preferredWidth: 140
                            text: endoCoagEffect()
                            color: root.fotekBlue
                            font.pixelSize: 52
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        Button {
                            Layout.preferredWidth: powerStepButtonWidth
                            Layout.preferredHeight: controlButtonHeight
                            text: qsTr("+")
                            flat: true
                            background: Rectangle {
                                radius: 16
                                color: powerStepButtonBg
                                border.width: 2
                                border.color: root.fotekOrange
                            }
                            contentItem: Text {
                                text: parent.text
                                color: powerStepButtonText
                                font.pixelSize: 48
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onPressed: setEndoPower(endoCutEffect(), endoCoagEffect() + 1)
                        }
                        Item { Layout.fillWidth: true; Layout.minimumWidth: 24 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: modeSelected() && isCoag
                                 && ((socId <= 1 && isBiCoagMode())
                                     || (socId <= 1 && isTermoMode())
                                     || (socId >= 2 && socId <= 3 && isSoftMode()))

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: controlButtonHeight
                            spacing: 12
                            visible: socId <= 1 && isBiCoagMode()

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlButtonHeight
                                visible: allowsBiAutoStop()
                                text: qsTr("АВТОСТОП")
                                flat: true
                                background: Rectangle {
                                    radius: 14
                                    border.width: socketAutoMode() === 1 ? 2 : 1
                                    border.color: socketAutoMode() === 1 ? root.autoBtnOnBorder : root.autoBtnOffBorder
                                    color: socketAutoMode() === 1 ? root.autoBtnOnFill : root.autoBtnOffFill
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: socketAutoMode() === 1 ? "black" : root.autoBtnOffText
                                    font.pixelSize: 24
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onPressed: requestAutoMode(1, qsTr("В режиме АВТОСТОП инструмент активируется с помощью педали.\nПо завершении коагуляции процесс прекращается автоматически"))
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlButtonHeight
                                text: qsTr("АВТОСТАРТ/СТОП")
                                flat: true
                                background: Rectangle {
                                    radius: 14
                                    border.width: socketAutoMode() === 2 ? 2 : 1
                                    border.color: socketAutoMode() === 2 ? root.autoBtnOnBorder : root.autoBtnOffBorder
                                    color: socketAutoMode() === 2 ? root.autoBtnOnFill : root.autoBtnOffFill
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: socketAutoMode() === 2 ? "black" : root.autoBtnOffText
                                    font.pixelSize: 24
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onPressed: requestAutoMode(2, qsTr("В режиме АВТОСТАРТ/СТОП активация происходит автоматически без нажатия педали!"))
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: controlButtonHeight
                            spacing: 12
                            visible: (socId >= 2 && socId <= 3 && isSoftMode())
                                     || (socId <= 1 && isTermoMode())

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlButtonHeight
                                text: qsTr("АВТОСТОП")
                                flat: true
                                background: Rectangle {
                                    radius: 14
                                    border.width: socketAutoMode() === 1 ? 2 : 1
                                    border.color: socketAutoMode() === 1 ? root.autoBtnOnBorder : root.autoBtnOffBorder
                                    color: socketAutoMode() === 1 ? root.autoBtnOnFill : root.autoBtnOffFill
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: socketAutoMode() === 1 ? "black" : root.autoBtnOffText
                                    font.pixelSize: 24
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onPressed: requestAutoMode(1, isTermoMode()
                                           ? qsTr("В режиме АВТОСТОП инструмент активируется с помощью педали.\nПо завершении коагуляции процесс прекращается автоматически")
                                           : qsTr("В режиме АВТОСТОП инструмент активируется с помощью педали или держателя инструментов.\nПо завершении коагуляции процесс прекращается автоматически"))
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Задержка автозапуска")
                            visible: socId <= 1 && isBiCoagMode() && socketAutoMode() === 2
                            horizontalAlignment: Text.AlignHCenter
                            color: uiMidGray
                            font.pixelSize: 24
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: controlButtonHeight
                            spacing: 8
                            visible: socId <= 1 && isBiCoagMode() && socketAutoMode() === 2

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlButtonHeight
                                text: qsTr("0 сек")
                                flat: true
                                background: Rectangle {
                                    radius: 12
                                    border.width: periphHandle.autoDelayMs === 0 ? 2 : 1
                                    border.color: periphHandle.autoDelayMs === 0 ? root.autoBtnOnBorder : root.autoBtnOffBorder
                                    color: periphHandle.autoDelayMs === 0 ? root.autoBtnOnFill : root.autoBtnOffFill
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: periphHandle.autoDelayMs === 0 ? "black" : root.autoBtnOffText
                                    font.pixelSize: 22
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onPressed: periphHandle.setAutoDelayMs(0)
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlButtonHeight
                                text: qsTr("0.5 сек")
                                flat: true
                                background: Rectangle {
                                    radius: 12
                                    border.width: periphHandle.autoDelayMs === 500 ? 2 : 1
                                    border.color: periphHandle.autoDelayMs === 500 ? root.autoBtnOnBorder : root.autoBtnOffBorder
                                    color: periphHandle.autoDelayMs === 500 ? root.autoBtnOnFill : root.autoBtnOffFill
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: periphHandle.autoDelayMs === 500 ? "black" : root.autoBtnOffText
                                    font.pixelSize: 22
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onPressed: periphHandle.setAutoDelayMs(500)
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlButtonHeight
                                text: qsTr("1.0 сек")
                                flat: true
                                background: Rectangle {
                                    radius: 12
                                    border.width: periphHandle.autoDelayMs === 1000 ? 2 : 1
                                    border.color: periphHandle.autoDelayMs === 1000 ? root.autoBtnOnBorder : root.autoBtnOffBorder
                                    color: periphHandle.autoDelayMs === 1000 ? root.autoBtnOnFill : root.autoBtnOffFill
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: periphHandle.autoDelayMs === 1000 ? "black" : root.autoBtnOffText
                                    font.pixelSize: 22
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onPressed: periphHandle.setAutoDelayMs(1000)
                            }

                            Button {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlButtonHeight
                                text: qsTr("1.5 сек")
                                flat: true
                                background: Rectangle {
                                    radius: 12
                                    border.width: periphHandle.autoDelayMs === 1500 ? 2 : 1
                                    border.color: periphHandle.autoDelayMs === 1500 ? root.autoBtnOnBorder : root.autoBtnOffBorder
                                    color: periphHandle.autoDelayMs === 1500 ? root.autoBtnOnFill : root.autoBtnOffFill
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: periphHandle.autoDelayMs === 1500 ? "black" : root.autoBtnOffText
                                    font.pixelSize: 22
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onPressed: periphHandle.setAutoDelayMs(1500)
                            }
                        }
                    }

                    Label {
                        id: endoRateLabel
                        Layout.fillWidth: true
                        text: endoPulseRateText()
                        visible: modeSelected() && modeEditor.isEndo
                        horizontalAlignment: Text.AlignHCenter
                        color: "black"
                        font.pixelSize: 24
                        font.bold: true
                    }
                    EndoChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
//                        Layout.topMargin: 8
                        visible: modeSelected() && modeEditor.isEndo
                        cutEffect: endoCutEffect()
                        coagEffect: endoCoagEffect()
                        modeName: modeTitleText()
                    }
                    Item { Layout.fillHeight: true }
                }

                Rectangle {
                    id: previewRect
                    z: 2
                    anchors.fill: parent
                    anchors.margins: 8
                    radius: 20
                    color: "white"
                    border.width: 2
                    border.color: root.fotekOrange
                    visible: centerView !== "power"

                    ColumnLayout {
                        id: previewContent
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 12

                        Label {
                            Layout.fillWidth: true
                            text: centerView === "modePreview" ? modeTitleText() : currentInstrName()
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            color: root.fotekBlue
                            font.pixelSize: 28
                            font.bold: true
                        }

                        Image {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 150
                            Layout.preferredHeight: 150
                            visible: centerView === "modePreview"
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: modeEditor.currentModeIndex >= 0 && modeEditor.currentModeIndex < modeIds.length
                                    ? ("image://modes/" + modeImagePrefix() + "%1").arg(modeIds[modeEditor.currentModeIndex]) : ""
                        }

                        Image {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 320
                            visible: centerView === "instrPreview"
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            source: modeEditor.currentInstrIndex >= 0 && modeEditor.currentInstrIndex < instrNums.length
                                    ? ("image://instruments/instrum%1").arg(instrNums[modeEditor.currentInstrIndex]) : ""
                        }

                        Rectangle {
                            id: descriptionContainer
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: centerView === "modePreview" ? 120 : 0
                            color: "transparent"
                            clip: true

                            Flickable {
                                id: descriptionFlick
                                anchors.fill: parent
                                contentWidth: width
                                contentHeight: descriptionText.paintedHeight
                                boundsBehavior: Flickable.StopAtBounds
                                clip: true

                                Text {
                                    id: descriptionText
                                    width: descriptionContainer.width
                                    text: centerView === "modePreview"
                                          ? ("<b>" + modeEditor.modeBrief + "</b><br><br>" + modeEditor.modeDescript)
                                          : ("<b>" +
                                                (modeEditor.instrBrief === currentInstrName()
                                                 ? ""
                                                 : modeEditor.instrBrief)  + "</b>")  // Если описание совпадает с названием, то не выводим
                                    textFormat: Text.RichText
                                    wrapMode: Text.WordWrap
                                    color: root.fotekBlue
                                    font.pixelSize: 22
                                }

                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Рекомендуемый уровень")
                            visible: modeSelected() && instrumentSelected()
                            horizontalAlignment: Text.AlignHCenter
                            color: uiMidGray
                            font.pixelSize: 24
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: modeEditor.isEndo ? recommendedEndoButtonHeight : recommendedButtonHeight
                            visible: modeSelected() && instrumentSelected()
                            spacing: 8

                            PowerRect {
                                Layout.fillWidth: true
                                Layout.preferredHeight: modeEditor.isEndo ? recommendedEndoButtonHeight : recommendedButtonHeight
                                borderColor: "#A5D6A7"
                                idleFillColor: "#F3F5F9"
                                idleTextColor: root.fotekBlue
                                power: modeEditor.lowPowerBound
                                selected: modeEditor.currentPower === power
                                isEndo: modeEditor.isEndo
                                onPowerChosen: modeEditor.updateParameter("currentpower", pwr)
                            }
                            PowerRect {
                                Layout.fillWidth: true
                                Layout.preferredHeight: modeEditor.isEndo ? recommendedEndoButtonHeight : recommendedButtonHeight
                                borderColor: "#78D87C"
                                idleFillColor: "#F3F5F9"
                                idleTextColor: root.fotekBlue
                                power: modeEditor.midPowerBound
                                selected: modeEditor.currentPower === power
                                isEndo: modeEditor.isEndo
                                onPowerChosen: modeEditor.updateParameter("currentpower", pwr)
                            }
                            PowerRect {
                                Layout.fillWidth: true
                                Layout.preferredHeight: modeEditor.isEndo ? recommendedEndoButtonHeight : recommendedButtonHeight
                                borderColor: "#51D456"
                                idleFillColor: "#F3F5F9"
                                idleTextColor: root.fotekBlue
                                power: modeEditor.highPowerBound
                                selected: modeEditor.currentPower === power
                                isEndo: modeEditor.isEndo
                                onPowerChosen: modeEditor.updateParameter("currentpower", pwr)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 64
                            spacing: 16

                            DialogActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 64
                                text: qsTr("ОТМЕНА")
                                secondaryColor: "white"
                                secondaryBorderWidth: 2
                                secondaryBorderColor: root.fotekBlue
                                cornerRadius: 20
                                labelPixelSize: 28
                                labelColor: root.fotekBlue
                                onPressed: restoreCommittedSelection()
                            }
                            DialogActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 64
                                text: qsTr("ПРИНЯТЬ")
                                primary: true
                                primaryEnabledColor: root.fotekBlue
                                primaryDisabledColor: "#26409370"
                                primaryBorderWidth: 1
                                primaryBorderColor: "#1E3274"
                                cornerRadius: 20
                                labelPixelSize: 28
                                labelColor: "white"
                                onPressed: applyPreviewSelection()
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
            visible: centerView === "power"

            DialogActionButton {
                anchors.left: parent.left
                anchors.leftMargin: root.screenMargin
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                height: 62
                text: qsTr("ОТМЕНА")
                secondaryColor: "white"
                secondaryBorderWidth: 2
                secondaryBorderColor: root.fotekBlue
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: root.fotekBlue
                onPressed: cancelEditorAndClose()
            }

            DialogActionButton {
                anchors.right: parent.right
                anchors.rightMargin: root.screenMargin
                anchors.verticalCenter: parent.verticalCenter
                width: 180
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
                onPressed: attemptCommitAndClose()
            }
        }
    }

    Connections {
        target: modeListView
        function onNewIndexSelected(index) {
            if (internalIndexChange || openingInProgress) {
                return
            }
            if (index >= 0 && index < modeModel.count && modeModel.get(index).locked) {
                return
            }
            if (index === modeEditor.currentModeIndex && centerView === "modePreview") {
                return
            }
            modeEditor.currentModeIndex = index
            modeListView.curIndex = index
            instrListView.curIndex = modeEditor.currentInstrIndex
            modeListView.positionSelectedItem()
            instrListView.positionSelectedItem()
            centerView = "modePreview"
        }
    }

    Connections {
        target: instrListView
        function onNewIndexSelected(index) {
            if (internalIndexChange || openingInProgress) {
                return
            }
            if (index === modeEditor.currentInstrIndex && centerView === "instrPreview") {
                return
            }
            modeEditor.currentInstrIndex = index
            instrListView.curIndex = index
            instrListView.positionSelectedItem()
            centerView = "instrPreview"
        }
    }

    Connections {
        target: modeEditor
        function onCurrentModeIndexChanged() {
            internalIndexChange = true
            updateInstrModel()
            instrListView.curIndex = modeEditor.currentInstrIndex
            modeListView.positionSelectedItem()
            instrListView.positionSelectedItem()
            internalIndexChange = false
            if (!openingInProgress && socketAutoModeState !== 0) {
                setSocketAutoMode(0)
            }
            normalizeEndoCurrentPower()
        }
        function onCurrentParamsChanged() {
            normalizeEndoCurrentPower()
        }
    }

    Connections {
        target: periphHandle
        function onAutoModeChanged(socketId, mode) {
            if (socketId === socId) {
                socketAutoModeState = mode
            }
            syncAutoModeDirtyFromAuto()
        }
        function onAutoDelayMsChanged(delayMs) {
            syncAutoModeDirtyFromAuto()
        }
    }

    Popup {
        id: autoModeConfirmPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width * 0.72, 760)
        height: 320

        background: Rectangle {
            radius: 20
            color: "#F3F5F9"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 18

            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: autoModeConfirmText
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: root.fotekBlue
                font.pixelSize: 24
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                spacing: 12

                DialogActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    text: qsTr("ОТМЕНА")
                    secondaryColor: "white"
                    secondaryBorderWidth: 2
                    secondaryBorderColor: root.fotekBlue
                    cornerRadius: 20
                    labelPixelSize: 24
                    labelColor: root.fotekBlue
                    onPressed: {
                        pendingAutoMode = -1
                        autoModeConfirmPopup.close()
                    }
                }

                DialogActionButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    text: qsTr("ПРИНЯТЬ")
                    primary: true
                    primaryEnabledColor: root.fotekBlue
                    primaryBorderWidth: 1
                    primaryBorderColor: "#1E3274"
                    cornerRadius: 20
                    labelPixelSize: 24
                    labelColor: "white"
                    onPressed: {
                        if (pendingAutoMode >= 0) {
                            setSocketAutoMode(pendingAutoMode)
                        }
                        pendingAutoMode = -1
                        autoModeConfirmPopup.close()
                    }
                }
            }
        }
    }

    NeutralPowerWarningDialog {
        id: neutralPowerWarningDialog

        onContinueChosen: root.finishCommitAndClose()

        onReduceChosen: {
            modeEditor.updateParameter("currentpower", root.neutralMaxPower())
            root.finishCommitAndClose()
        }
    }
}
