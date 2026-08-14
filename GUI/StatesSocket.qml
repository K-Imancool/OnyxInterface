import QtQuick 2.15
import QtQuick.Controls 2.15
import BackEnd 1.0

Rectangle {
    id: socketRoot
    color: "transparent"
    property var activationOverlay: null
    property bool activationUiAllowed: true
    property bool dimmed: false
    property bool allowEditing: true

    // Константы состояний сокета (соответствуют Onyx::SocStatus)
    readonly property int socketStateOff: 0          // S_OFF - Отключен
    readonly property int socketStateDisabled: 1    // S_DISABLED - Выключен, активация запрещена
    readonly property int socketStateEnabled: 2     // S_ENABLED - Включен, активация разрешена
    readonly property int socketStateActiveCoag: 3  // S_ACTIVE_COAG - Активирован, коагуляция
    readonly property int socketStateActiveCut: 4   // S_ACTIVE_CUT - Активирован, резание
    readonly property int socketStateError: 5        // S_ERROR - Ошибка, активация запрещена

    property string title
    property int socketId
    property int socketState

    property string cutModeName
    property int cutModePower
    property int cutModeId
    property int cutMaxPower
    property int cutInstrumNum
    property string cutInstrumName: qsTr("Другой инструмент")
    property bool cutIsEndo: false
    property bool cutHasAvailableModes: true

    property string coagModeName
    property int coagModePower
    property int coagModeId
    property int _coagAutoDisplayRev: 0
    property int coagMaxPower
    property int coagInstrumNum
    property string coagInstrumName: qsTr("Другой инструмент")
    property bool coagIsEndo: false
    property bool coagHasAvailableModes: true

    signal socketEditorRequest(int socketId, bool isCoag)
    signal fullSocketEditorRequest(int socketId)
    signal dimmedSocketClicked(int socketId)

    // Сокращённое имя режима для главного экрана при активном авторежиме.
    // Пустая строка — сокращение не нужно, вызывающий код добавит суффикс сам.
    function shortenedModeNameForAuto(modeId, autoMode) {
        if (modeId === ESHF.BI_COAG) { // БИ-КОАГ
            if (autoMode === 1)
                return qsTr("БИ КОАГ\nАВТОСТОП")
            if (autoMode === 2)
                return qsTr("БИ КОАГ\nА-СТ-СТОП")
        }
        else if (modeId === ESHF.BI_COAG_DISS) { // БИ-КОАГ-ДИССЕКТ
            if (autoMode === 1)
                return qsTr("БИ КОАГ ДИСС\nАВТОСТОП")
            if (autoMode === 2)
                return qsTr("БИ КОАГ ДИСС\nА-СТ-СТОП")
        }
        else if (modeId === ESHF.BI_COAG_MICRO) { // БИ-КОАГ-МИКРО
            if (autoMode === 1)
                return qsTr("БИ КОАГ МИКРО\nАВТОСТОП")
            if (autoMode === 2)
                return qsTr("БИ КОАГ МИКР\nА-СТ-СТОП")
        }
        else if (modeId === ESHF.SOFT) { // МЯГКАЯ
            if (autoMode === 1)
                return qsTr("МЯГКАЯ\nАВТОСТОП")
        }
        return ""
    }

    readonly property string coagModeNameForDisplay: {
        var _ = _coagAutoDisplayRev
        var a = periphHandle.autoMode(socketId)
        var base = coagModeName
        var shortened = shortenedModeNameForAuto(coagModeId, a)
        if (shortened.length > 0)
            return shortened
        return base
    }

    onSocketStateChanged: {
        if (!activationOverlay) {
            return
        }
        if (!activationUiAllowed) {
            if (activationOverlay.activeSocketId === socketId) {
                activationOverlay.close()
            }
            return
        }
        if (socketState === socketStateActiveCoag) {
            activationOverlay.activeSocketId = socketId
            activationOverlay.socketName = title
            activationOverlay.isCoag = true
            activationOverlay.modeName = coagModeNameForDisplay
            activationOverlay.power = coagModePower
            activationOverlay.isEndo = coagIsEndo
            activationOverlay.open()
        } else if (socketState === socketStateActiveCut) {
            activationOverlay.activeSocketId = socketId
            activationOverlay.socketName = title
            activationOverlay.isCoag = false
            activationOverlay.modeName = cutModeName
            activationOverlay.power = cutModePower
            activationOverlay.isEndo = cutIsEndo
            activationOverlay.open()
        } else if (activationOverlay.activeSocketId === socketId) {
            activationOverlay.close()
        }
    }

    HalfSocket {
        id:         leftRect
        isCoag:     false
        socketName: socketRoot.title
        modeName:   socketRoot.cutModeName
        modePower:  socketRoot.cutModePower
        modeId:     socketRoot.cutModeId
        maxPower:   socketRoot.cutMaxPower
        instrumNum: socketRoot.cutInstrumNum
        instrumName:socketRoot.cutInstrumName
        isEndo:     socketRoot.cutIsEndo
        hasAvailableModes: socketRoot.cutHasAvailableModes
        anchors.left: parent.left
        anchors.right: centerSeparator.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (socketRoot.allowEditing) {
                    if (!socketRoot.cutHasAvailableModes)
                        return
                    socketRoot.fullSocketEditorRequest(socketRoot.socketId)
                } else {
                    socketRoot.dimmedSocketClicked(socketRoot.socketId)
                }
            }
        }
    }
    HalfSocket {
        id:         rightRect
        isCoag:     true
        socketName: socketRoot.title
        modeName:   socketRoot.coagModeNameForDisplay
        modePower:  socketRoot.coagModePower
        modeId:     socketRoot.coagModeId
        maxPower:   socketRoot.coagMaxPower
        instrumNum: socketRoot.coagInstrumNum
        instrumName:socketRoot.coagInstrumName
        isEndo:     socketRoot.coagIsEndo
        hasAvailableModes: socketRoot.coagHasAvailableModes
        anchors.left: centerSeparator.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (socketRoot.allowEditing) {
                    if (!socketRoot.coagHasAvailableModes)
                        return
                    socketRoot.fullSocketEditorRequest(socketRoot.socketId)
                } else {
                    socketRoot.dimmedSocketClicked(socketRoot.socketId)
                }
            }
        }
    }

    Rectangle {
        id: centerSeparator
        width: 2
        color: "#202020"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        z: 2
    }

    Rectangle {
        id: dimOverlay
        anchors.fill: parent
        visible: socketRoot.dimmed
        color: "#66000000"
        z: 3
    }

    Connections {
        target: periphHandle
        function onAutoModeChanged(socketId, mode) {
            socketRoot._coagAutoDisplayRev++
        }
    }

}
