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

    property int socId: -1
    readonly property var modeEditor: Editor

    readonly property color fotekBlue: "#264093"
    readonly property color fotekOrange: "#faa731"
    readonly property color cutAccent: "#F4D13D"
    readonly property color coagAccent: "#0B4FB3"
    readonly property color uiMidGray: "#5A6478"
    readonly property int screenMargin: 20
    readonly property int controlButtonHeight: 90
    readonly property int endoPowerRowHeight: 124
    readonly property int modeRowHeight: 100
    readonly property int instrRowHeight: 120
    readonly property int panelCornerRadius: 20
    readonly property int powerStepButtonWidth: 112
    readonly property color autoBtnOnFill: fotekOrange
    readonly property color autoBtnOnBorder: "#1E3274"
    readonly property color autoBtnOffFill: "white"
    readonly property color autoBtnOffBorder: "#C7CEDA"
    readonly property color autoBtnOffText: fotekBlue

    property bool activeIsCoag: false
    property string socketTitle: ""
    property bool cutDirty: false
    property bool coagDirty: false
    property bool autoModeDirty: false
    property int socketAutoModeState: 0
    property var autoModeBaseline: [0, 0, 0, 0]
    property int autoDelayBaseline: 0
    property int pendingAutoMode: -1
    property string autoModeConfirmText: ""
    property bool pendingSubEditorCoag: false
    property bool powerRepeatIncrease: false
    property bool powerRepeatIsCoag: false
    property var modePicker
    property var instrPicker

    property var cutLive: ({
                               modeIndex: -1,
                               instrIndex: -1,
                               power: 0,
                               modeName: "",
                               modeId: -1,
                               modeNum: "0",
                               instrName: "",
                               instrNum: "1000",
                               isEndo: false,
                               maxPower: 0,
                               lowPower: 0,
                               midPower: 0,
                               highPower: 0
                           })
    property var coagLive: ({
                                modeIndex: -1,
                                instrIndex: -1,
                                power: 0,
                                modeName: "",
                                modeId: -1,
                                modeNum: "0",
                                instrName: "",
                                instrNum: "1000",
                                isEndo: false,
                                maxPower: 0,
                                lowPower: 0,
                                midPower: 0,
                                highPower: 0
                            })
    property var cutBaseline: ({})
    property var coagBaseline: ({})
    property bool cutHasAvailableModes: true
    property bool coagHasAvailableModes: true

    readonly property bool hasAnyChanges: cutDirty || coagDirty || autoModeDirty

    background: Rectangle {
        color: "#F3F5F9"
    }

    function sideRef(isCoag) {
        return isCoag ? coagLive : cutLive
    }

    function cloneSideState(source) {
        return {
            modeIndex: source.modeIndex,
            instrIndex: source.instrIndex,
            power: source.power,
            modeName: source.modeName,
            modeId: source.modeId,
            modeNum: source.modeNum,
            instrName: source.instrName,
            instrNum: source.instrNum,
            isEndo: source.isEndo,
            maxPower: source.maxPower,
            lowPower: source.lowPower,
            midPower: source.midPower,
            highPower: source.highPower
        }
    }

    function captureSideFromEditor() {
        var mode = modeEditor.currentMode
        var modeNums = modeEditor.modeNamesNums()
        var instrNums = modeEditor.instrListNums()
        var modeIdx = modeEditor.currentModeIndex
        var instrIdx = modeEditor.currentInstrIndex

        return {
            modeIndex: modeIdx,
            instrIndex: instrIdx,
            power: modeEditor.currentPower,
            modeName: mode && mode.name !== undefined && mode.name !== null ? mode.name : "",
            modeId: mode && mode.id !== undefined && mode.id !== null ? parseInt(mode.id) : -1,
            modeNum: modeIdx >= 0 && modeIdx < modeNums.length ? modeNums[modeIdx] : "0",
            instrName: instrIdx >= 0 && instrIdx < modeEditor.instrList.length
                    ? modeEditor.instrList[instrIdx] : qsTr("Другой инструмент"),
            instrNum: instrIdx >= 0 && instrIdx < instrNums.length ? instrNums[instrIdx] : "1000",
            isEndo: modeEditor.isEndo,
            maxPower: mode && mode.maxpower !== undefined ? parseInt(mode.maxpower) : 0,
            lowPower: modeEditor.lowPowerBound,
            midPower: modeEditor.midPowerBound,
            highPower: modeEditor.highPowerBound
        }
    }

    function normalizeSidePowerIfNeeded(side) {
        var result = cloneSideState(side)
        if (result.modeId === 1000 || result.modeId < 0)
            return result
        if (parseInt(result.instrNum) !== 1000)
            return result
        if (result.power > 0)
            return result
        result.power = result.isEndo ? 11 : 1
        return result
    }

    function copyEditorToSide(isCoag) {
        var side = captureSideFromEditor()
        var normalized = normalizeSidePowerIfNeeded(side)
        if (normalized.power !== side.power)
            modeEditor.updateParameter("currentpower", normalized.power)
        if (isCoag)
            coagLive = normalized
        else
            cutLive = normalized
    }

    function safeModeIndex(modeIndex) {
        return modeIndex >= 0 ? modeIndex : 0
    }

    function applySideToEditor(isCoag) {
        var side = sideRef(isCoag)
        var targetMode = safeModeIndex(side.modeIndex)
        // initialize() may snap to the committed model mode; always re-apply live side after it.
        modeEditor.initialize(socId, targetMode, isCoag)
        modeEditor.currentModeIndex = targetMode
        if (side.instrIndex >= 0)
            modeEditor.currentInstrIndex = side.instrIndex
        modeEditor.updateParameter("currentpower", side.power)
        if (side.isEndo)
            normalizeEndoPowerForSide(isCoag)
    }

    function activateSide(isCoag) {
        if (activeIsCoag === isCoag)
            return
        copyEditorToSide(activeIsCoag)
        activeIsCoag = isCoag
        applySideToEditor(isCoag)
    }

    function refreshDirtyFlags() {
        cutDirty = sideDiffersFromBaseline(false)
        coagDirty = sideDiffersFromBaseline(true)
    }

    function sideDiffersFromBaseline(isCoag) {
        var live = sideRef(isCoag)
        var base = isCoag ? coagBaseline : cutBaseline
        return live.modeIndex !== base.modeIndex
                || live.instrIndex !== base.instrIndex
                || live.power !== base.power
    }

    function modeImagePrefix() {
        var socketName = socketTitle ? String(socketTitle).toUpperCase() : ""
        if (socketName.indexOf("МОНО") !== -1 || socketName.indexOf("MONO") !== -1)
            return "monomode"
        if (socketName.indexOf("БИ") !== -1 || socketName.indexOf("BI") !== -1)
            return "bimode"
        return socId <= 1 ? "bimode" : "monomode"
    }

    function modeSelectedInSide(isCoag) {
        var side = sideRef(isCoag)
        if (side.modeIndex < 0)
            return false
        if (side.modeId === 1000)
            return false
        return side.modeName.length > 0
    }

    function instrumentSelectedInSide(isCoag) {
        var side = sideRef(isCoag)
        return parseInt(side.instrNum) !== 1000
    }

    function instrumentIconSource(isCoag) {
        if (!instrumentSelectedInSide(isCoag))
            return ""
        var side = sideRef(isCoag)
        return "image://instruments/"
                + (isCoag ? "coaginstr" : "cutinstr")
                + side.instrNum
    }

    function endoCutEffect(isCoag) {
        return Math.floor(sideRef(isCoag).power / 10)
    }

    function endoCoagEffect(isCoag) {
        return sideRef(isCoag).power % 10
    }

    function setEndoPower(isCoag, cutEffect, coagEffect) {
        activateSide(isCoag)
        var cut = Math.max(1, Math.min(3, cutEffect))
        var coag = Math.max(1, Math.min(3, coagEffect))
        modeEditor.updateParameter("currentpower", cut * 10 + coag)
        copyEditorToSide(isCoag)
        refreshDirtyFlags()
    }

    function normalizeEndoPowerForSide(isCoag) {
        var side = cloneSideState(sideRef(isCoag))
        if (!side.isEndo)
            return
        var cut = Math.max(1, Math.min(3, Math.floor(side.power / 10)))
        var coag = Math.max(1, Math.min(3, side.power % 10))
        var packed = cut * 10 + coag
        if (packed !== side.power) {
            modeEditor.updateParameter("currentpower", packed)
            side.power = packed
            if (isCoag)
                coagLive = side
            else
                cutLive = side
        }
    }

    function nextPowerUp(power, maxPower) {
        var value = power
        if (value < 20) value += 1
        else if (value < 50) value += 2
        else if (value < 100) value += 5
        else if (value < 200) value += 10
        else if (value < 400) value += 25
        return Math.min(value, maxPower)
    }

    function nextPowerDown(power) {
        var value = power
        if (value <= 1) value = 1
        else if (value <= 20) value -= 1
        else if (value <= 50) value -= 2
        else if (value <= 100) value -= 5
        else if (value <= 200) value -= 10
        else if (value <= 400) value -= 25
        return Math.max(value, 1)
    }

    function stepMainPower(isCoag, increase) {
        activateSide(isCoag)
        var side = sideRef(isCoag)
        if (!modeSelectedInSide(isCoag) || side.isEndo || side.maxPower <= 0)
            return
        if (increase)
            modeEditor.updateParameter("currentpower", nextPowerUp(side.power, side.maxPower))
        else
            modeEditor.updateParameter("currentpower", nextPowerDown(side.power))
        copyEditorToSide(isCoag)
        refreshDirtyFlags()
    }

    function startMainPowerRepeat(isCoag, increase) {
        powerRepeatIsCoag = isCoag
        powerRepeatIncrease = increase
        stepMainPower(isCoag, increase)
        powerRepeatDelay.restart()
    }

    function stopMainPowerRepeat() {
        powerRepeatDelay.stop()
        powerRepeatTick.stop()
    }

    function setRecommendedPower(isCoag, power) {
        if (power === 0)
            return
        activateSide(isCoag)
        modeEditor.updateParameter("currentpower", power)
        copyEditorToSide(isCoag)
        refreshDirtyFlags()
    }

    function isBiCoagModeInSide(isCoag) {
        var modeId = sideRef(isCoag).modeId
        return modeId === 5 ||
                modeId === 6 ||
                modeId === 27 ||
                modeId === 61 ||
                modeId === 62 ||
                modeId === 63 ||
                modeId === 64
    }

    function isSoftModeInSide(isCoag) {
        return sideRef(isCoag).modeId === 21
    }

    function endoPulseRateText(modeId) {
        switch (parseInt(modeId)) {
        case 13: // ЭНДОНОЖ-1
        case 16: // ЭНДОПЕТЛЯ-
            return qsTr("Подача импульсов РЕДКАЯ")
        case 14: // ЭНДОНОЖ-2
        case 17: // ЭНДОПЕТЛЯ-2
            return qsTr("Подача импульсов СРЕДНЯЯ")
        case 15: // ЭНДОНОЖ-3
        case 18: // ЭНДОПЕТЛЯ-3
            return qsTr("Подача импульсов ЧАСТАЯ")
        default:
            return qsTr("Подача импульсов СРЕДНЯЯ")
        }
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
        if (!diff && periphHandle.autoDelayMs !== autoDelayBaseline)
            diff = true
        autoModeDirty = diff
    }

    function restoreAutoModesFromBaseline() {
        for (var i = 0; i < 4; i++)
            periphHandle.setAutoMode(i, autoModeBaseline[i])
        periphHandle.setAutoDelayMs(autoDelayBaseline)
        socketAutoModeState = periphHandle.autoMode(socId)
    }

    function setSocketAutoMode(mode) {
        socketAutoModeState = mode
        periphHandle.setAutoMode(socId, mode)
    }

    function requestAutoMode(mode, confirmationText) {
        if (socketAutoModeState === mode) {
            setSocketAutoMode(0)
            return
        }
        if (!Overlay.overlay)
            return
        pendingAutoMode = mode
        autoModeConfirmText = confirmationText
        autoModeConfirmPopup.parent = Overlay.overlay
        autoModeConfirmPopup.x = Math.round((Overlay.overlay.width - autoModeConfirmPopup.width) / 2)
        autoModeConfirmPopup.y = Math.round((Overlay.overlay.height - autoModeConfirmPopup.height) / 2)
        autoModeConfirmPopup.open()
    }

    function isMonopolarSocket() {
        var socketName = socketTitle ? String(socketTitle).toUpperCase() : ""
        return socketName.indexOf("МОНО") !== -1 || socketName.indexOf("MONO") !== -1
    }

    function neutralMaxPower() {
        return neutralPowerWarningDialog.maxPowerForNeutralSize(periphHandle.neutralSize)
    }

    function needsNeutralPowerWarning() {
        if (!isMonopolarSocket())
            return false
        if (cutDirty && cutLive.power > neutralMaxPower())
            return true
        if (coagDirty && coagLive.power > neutralMaxPower())
            return true
        return false
    }

    function liveHasArgonMode() {
        if (typeof theModel === "undefined" || !theModel)
            return false
        return theModel.isArgonMode(cutLive.modeId) || theModel.isArgonMode(coagLive.modeId)
    }

    function needsArgonConflictWarning() {
        if (socId !== 2 && socId !== 3)
            return false
        if (!liveHasArgonMode())
            return false
        if (typeof theModel === "undefined" || !theModel)
            return false
        var conflict = theModel.otherMonoArgonConflict(socId)
        return conflict && conflict.conflict === true
    }

    function continueCommitAfterArgonCheck() {
        if (needsNeutralPowerWarning()) {
            neutralPowerWarningDialog.socketName = socketTitle
            neutralPowerWarningDialog.maxNeutralPower = neutralMaxPower()
            neutralPowerWarningDialog.showWarning()
            return
        }
        finishCommitAndClose()
    }

    function hasAvailableModesFromEditor() {
        var nums = modeEditor.modeNamesNums()
        for (var i = 0; i < nums.length; ++i) {
            if (parseInt(nums[i]) !== 1000)
                return true
        }
        return false
    }

    function prepareEditorData() {
        if (socId < 0)
            return

        modeEditor.initialize(socId, 0, false)
        cutHasAvailableModes = hasAvailableModesFromEditor()
        cutLive = normalizeSidePowerIfNeeded(captureSideFromEditor())
        cutBaseline = cloneSideState(cutLive)
        socketTitle = modeEditor.socketName

        modeEditor.initialize(socId, 0, true)
        coagHasAvailableModes = hasAvailableModesFromEditor()
        coagLive = normalizeSidePowerIfNeeded(captureSideFromEditor())
        coagBaseline = cloneSideState(coagLive)

        activeIsCoag = false
        applySideToEditor(false)
        socketAutoModeState = periphHandle.autoMode(socId)
        captureAutoModeBaseline()
        autoModeDirty = false
        refreshDirtyFlags()
    }

    function cancelEditorAndClose() {
        restoreAutoModesFromBaseline()
        modeEditor.rollBack()
        root.close()
    }

    function commitSide(isCoag) {
        var side = sideRef(isCoag)
        modeEditor.initialize(socId, safeModeIndex(side.modeIndex), isCoag)
        modeEditor.currentModeIndex = side.modeIndex >= 0 ? side.modeIndex : modeEditor.currentModeIndex
        modeEditor.currentInstrIndex = side.instrIndex >= 0 ? side.instrIndex : modeEditor.currentInstrIndex
        modeEditor.updateParameter("currentpower", side.power)
        modeEditor.commitChanges()
    }

    function finishCommitAndClose() {
        // Не вызывать copyEditorToSide: attemptCommitAndClose уже синхронизировал
        // активную сторону, а onReduceChosen мог понизить power в cutLive/coagLive —
        // повторный захват из modeEditor затёр бы снижение.
        if (cutDirty)
            commitSide(false)
        if (coagDirty)
            commitSide(true)
        recomHandle.saveCurrentState()
        root.close()
    }

    function attemptCommitAndClose() {
        if (neutralPowerWarningDialog.opened || argonConflictWarningDialog.opened)
            return
        copyEditorToSide(activeIsCoag)
        if (needsArgonConflictWarning()) {
            var conflict = theModel.otherMonoArgonConflict(socId)
            argonConflictWarningDialog.socketName = conflict.socketName
            argonConflictWarningDialog.conflictSocketId = conflict.socketId
            argonConflictWarningDialog.showWarning()
            return
        }
        continueCommitAfterArgonCheck()
    }

    function acceptEditorAndClose() {
        if (!root.hasAnyChanges) {
            root.close()
            return
        }
        attemptCommitAndClose()
    }

    function openModePicker(isCoag) {
        if (isCoag && !coagHasAvailableModes)
            return
        if (!isCoag && !cutHasAvailableModes)
            return
        if (!modePicker)
            return
        activateSide(isCoag)
        applySideToEditor(isCoag)
        pendingSubEditorCoag = isCoag
        var side = sideRef(isCoag)
        modePicker.deferCommit = true
        modePicker.socId = socId
        modePicker.modeIndex = safeModeIndex(side.modeIndex)
        modePicker.isCoag = isCoag
        modePicker.open()
    }

    function openInstrPicker(isCoag) {
        if (isCoag && !coagHasAvailableModes)
            return
        if (!isCoag && !cutHasAvailableModes)
            return
        if (!instrPicker)
            return
        activateSide(isCoag)
        applySideToEditor(isCoag)
        pendingSubEditorCoag = isCoag
        var side = sideRef(isCoag)
        instrPicker.deferCommit = true
        instrPicker.socId = socId
        instrPicker.modeIndex = safeModeIndex(side.modeIndex)
        instrPicker.instrIndex = side.instrIndex
        instrPicker.isCoag = isCoag
        instrPicker.open()
    }

    function handleSubEditorClosed(accepted) {
        if (accepted) {
            copyEditorToSide(pendingSubEditorCoag)
            refreshDirtyFlags()
        } else {
            applySideToEditor(pendingSubEditorCoag)
        }
    }

    function ledOutputForSocket(id) {
        switch (id) {
        case 0: return LinkStm.OUT_BI1
        case 1: return LinkStm.OUT_BI2
        case 2: return LinkStm.OUT_MONO1
        case 3: return LinkStm.OUT_MONO2
        }
        return LinkStm.OUT_ALL
    }

    onOpened: {
        prepareEditorData()
        if (!cutHasAvailableModes && !coagHasAvailableModes) {
            Qt.callLater(function() { root.close() })
            return
        }
        if (socId >= 0 && socId <= 3)
            appControl.setLedOutput(ledOutputForSocket(socId), LinkStm.LED_WHITE)
    }

    onClosed: {
        appControl.setLedOutput(LinkStm.OUT_ALL, LinkStm.LED_OFF)
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
        onTriggered: stepMainPower(powerRepeatIsCoag, powerRepeatIncrease)
    }

    Rectangle {
        anchors.fill: parent
        color: "#F3F5F9"

        RowLayout {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 78
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: cutAccent

                Label {
                    anchors.centerIn: parent
                    text: qsTr("РЕЗАНИЕ")
                    font.pixelSize: 28
                    font.bold: true
                    color: "black"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "white"
                border.width: 1
                border.color: "#C7CEDA"

                Label {
                    anchors.fill: parent
                    anchors.margins: 8
                    text: socketTitle.length > 0
                          ? qsTr("ВЫХОД %1").arg(socketTitle)
                          : qsTr("ВЫХОД")
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 30
                    font.bold: true
                    color: fotekBlue
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: coagAccent

                Label {
                    anchors.centerIn: parent
                    text: qsTr("КОАГУЛЯЦИЯ")
                    font.pixelSize: 28
                    font.bold: true
                    color: "white"
                }
            }
        }

        ColumnLayout {
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 14
            anchors.topMargin: 34
            spacing: 20

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 20

                FullSocketModeButton {
                    Layout.row: 0
                    Layout.column: 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: modeRowHeight
                    visible: cutHasAvailableModes
                    editorRoot: root
                    isCoagSide: false
                    sideState: cutLive
                    accentColor: cutAccent
                    accentTextColor: "black"
                }

                FullSocketStepHint {
                    Layout.row: 0
                    Layout.column: 1
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: modeRowHeight
                    visible: cutHasAvailableModes || coagHasAvailableModes
                    line1: qsTr("Выберите")
                    line2: qsTr("режим")
                    textColor: uiMidGray
                    rowHeight: modeRowHeight
                }

                FullSocketModeButton {
                    Layout.row: 0
                    Layout.column: 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: modeRowHeight
                    visible: coagHasAvailableModes
                    editorRoot: root
                    isCoagSide: true
                    sideState: coagLive
                    accentColor: coagAccent
                    accentTextColor: "white"
                }

                FullSocketInstrButton {
                    Layout.row: 1
                    Layout.column: 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: instrRowHeight
                    visible: cutHasAvailableModes
                    editorRoot: root
                    isCoagSide: false
                    sideState: cutLive
                }

                FullSocketStepHint {
                    Layout.row: 1
                    Layout.column: 1
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: instrRowHeight
                    visible: cutHasAvailableModes || coagHasAvailableModes
                    line1: qsTr("Выберите")
                    line2: qsTr("инструмент")
                    textColor: uiMidGray
                    rowHeight: instrRowHeight
                }

                FullSocketInstrButton {
                    Layout.row: 1
                    Layout.column: 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: instrRowHeight
                    visible: coagHasAvailableModes
                    editorRoot: root
                    isCoagSide: true
                    sideState: coagLive
                }

                FullSocketPowerRow {
                    Layout.row: 2
                    Layout.column: 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: cutLive.isEndo ? endoPowerRowHeight : controlButtonHeight
                    editorRoot: root
                    isCoagSide: false
                    sideState: cutLive
                }

                FullSocketStepHint {
                    Layout.row: 2
                    Layout.column: 1
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: (cutLive.isEndo || coagLive.isEndo) ? endoPowerRowHeight : controlButtonHeight
                    line1: qsTr("Установите")
                    line2: qsTr("мощность")
                    textColor: uiMidGray
                    rowHeight: (cutLive.isEndo || coagLive.isEndo) ? endoPowerRowHeight : controlButtonHeight
                }

                FullSocketPowerRow {
                    Layout.row: 2
                    Layout.column: 2
                    Layout.fillWidth: true
                    Layout.preferredHeight: coagLive.isEndo ? endoPowerRowHeight : controlButtonHeight
                    editorRoot: root
                    isCoagSide: true
                    sideState: coagLive
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                FullSocketSidePanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    editorRoot: root
                    isCoagSide: false
                    sideState: cutLive
                }

                Item {
                    Layout.preferredWidth: 170
                    Layout.fillHeight: true
                }

                FullSocketSidePanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    editorRoot: root
                    isCoagSide: true
                    sideState: coagLive
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
                onPressed: cancelEditorAndClose()
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
                primaryEnabledColor: root.hasAnyChanges ? root.fotekBlue : "#26409370"
                primaryDisabledColor: "#26409370"
                primaryBorderWidth: 1
                primaryBorderColor: "#1E3274"
                cornerRadius: 20
                labelPixelSize: 30
                labelColor: "white"
                onPressed: acceptEditorAndClose()
            }
        }
    }

    Connections {
        target: periphHandle
        function onAutoModeChanged(socketId, mode) {
            if (socketId === root.socId)
                socketAutoModeState = mode
            syncAutoModeDirtyFromAuto()
        }
        function onAutoDelayMsChanged(delayMs) {
            syncAutoModeDirtyFromAuto()
        }
    }

    Popup {
        id: autoModeConfirmPopup
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
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
                font.pixelSize: 28
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
                        if (pendingAutoMode >= 0)
                            setSocketAutoMode(pendingAutoMode)
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
            if (root.cutLive.power > root.neutralMaxPower()) {
                var cut = root.cloneSideState(root.cutLive)
                cut.power = root.neutralMaxPower()
                root.cutLive = cut
                root.cutDirty = true
            }
            if (root.coagLive.power > root.neutralMaxPower()) {
                var coag = root.cloneSideState(root.coagLive)
                coag.power = root.neutralMaxPower()
                root.coagLive = coag
                root.coagDirty = true
            }
            root.finishCommitAndClose()
        }
    }

    ArgonConflictWarningDialog {
        id: argonConflictWarningDialog

        onAcceptChosen: {
            if (typeof theModel !== "undefined" && theModel
                    && argonConflictWarningDialog.conflictSocketId >= 0) {
                theModel.clearArgonModes(argonConflictWarningDialog.conflictSocketId)
            }
            root.continueCommitAfterArgonCheck()
        }

        onCancelChosen: {
            // Остаёмся в редакторе текущего выхода
        }
    }
}
