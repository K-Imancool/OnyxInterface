import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQml 2.15
import BackEnd 1.0


Repeater {

    id: repeatRoot

    required property int containerMargins
    required property int containerHeight
    required property int usedSpacing
    property var activationOverlay: null
    property bool activationUiAllowed: true

    signal socketEditorRequest(int socketId, int modeIndex, bool isCoag)
    signal fullSocketEditorRequest(int socketId)

    function isEndoSocketDimmed(socketIndex) {
        if (!theModel || !theModel.endoProgramView) {
            return false
        }
        if (theModel.subProgIdx === 0) {
            return socketIndex < 2
        }
        if (theModel.subProgIdx === 1) {
            return socketIndex >= 2
        }
        return false
    }

    function switchEndoPageByDimmedSocket(socketId) {
        if (!theModel || !theModel.endoProgramView) {
            return
        }
        if (theModel.subProgIdx === 0 && socketId < 2) {
            theModel.subProgIdx = 1
            return
        }
        if (theModel.subProgIdx === 1 && socketId >= 2) {
            theModel.subProgIdx = 0
            return
        }
        theModel.subProgIdx = theModel.subProgIdx === 0 ? 1 : 0
    }

    function calculateCollapsedHeight() {
        if (count <= 0) {
            return 0
        }
        var spacersHeight = count * repeatRoot.usedSpacing
        return (repeatRoot.containerHeight - (spacersHeight + repeatRoot.containerMargins * 2)) / count
    }

    clip: true

    delegate: StatesSocket {
        id: delegateSoc
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        Layout.preferredHeight: repeatRoot.calculateCollapsedHeight()
        activationOverlay: repeatRoot.activationOverlay
        activationUiAllowed: repeatRoot.activationUiAllowed

        title: model.socketname
        socketId: index
        socketState: model.socketstatus
        dimmed: repeatRoot.isEndoSocketDimmed(index)
        allowEditing: !repeatRoot.isEndoSocketDimmed(index)

        cutInstrumNum:     model.cutmodeinstrnum
        cutMaxPower:       model.cutmodemaxpower
        cutModeId:         model.cutmodeid
        cutModeName:       model.cutmodename
        cutInstrumName:    model.cutmodeinstrname
        cutHasAvailableModes: {
            var ids = model.cutmodeidlist || []
            for (var i = 0; i < ids.length; ++i) {
                if (Number(ids[i]) !== ESHF.NO_MODE) {
                    return true
                }
            }
            return false
        }

        coagInstrumNum:    model.coagmodeinstrnum
        coagMaxPower:      model.coagmodemaxpower
        coagModeId:        model.coagmodeid
        coagModeName:      model.coagmodename
        coagInstrumName:   model.coagmodeinstrname
        coagHasAvailableModes: {
            var ids = model.coagmodeidlist || []
            for (var i = 0; i < ids.length; ++i) {
                if (Number(ids[i]) !== ESHF.NO_MODE) {
                    return true
                }
            }
            return false
        }

        coagIsEndo:        model.coagmodeisendo
        cutIsEndo:         model.cutmodeisendo

        // Прямой биндинг с защитой от циклов через проверку в onNewPower
        cutModePower: model.cutmodepower
        coagModePower: model.coagmodepower

        Component.onCompleted: {
            
            if (delegateSoc.socketId === (count-1)) {
                //создали последний item - теперь все они доступны для расчёты высоты и
                //надо триггернуть пересчёт.
                //почему этой проблемы нет при первичной прогрузке - хз.
                //почему получилось только через костыль с действительным изменение чего-то,
                //но не через сигналы - хз
                theModel.recalcCollapsed()
            }
        }

        Connections {
            target: delegateSoc
            function onSocketEditorRequest(socketid, iscoag) {
                repeatRoot.socketEditorRequest(socketid,
                                               iscoag ? model.coagmodeindex : model.cutmodeindex,
                                               iscoag)
            }
            function onDimmedSocketClicked(socketid) {
                repeatRoot.switchEndoPageByDimmedSocket(socketid)
            }
            function onFullSocketEditorRequest(socketid) {
                repeatRoot.fullSocketEditorRequest(socketid)
            }
        }
    }
}

