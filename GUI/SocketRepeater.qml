import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0


Repeater {
// ListView {
    id: repeatRoot

    required property int containerMargins
    required property int containerHeight
    required property int usedSpacing

    property int collapsedFixedHeight: 85

    signal modeDialogRequest(int socketId, int modeIndex, bool isCoag)
    signal instrumDialogRequest(int socketId, int modeIndex, bool isCoag)

    function calculateExpandedHeight() {
        var totalFixedHeight = 0
        var expandedCount = 0
        var spacersHeight = (count) * repeatRoot.usedSpacing;
        for (var i = 0; i < count; i++) {
            if (!(itemAt(i) instanceof StatesSocket)) {
                console.log("oops")
                // console.log("Object type:", getObjectType(itemAt(i)));
                continue
            }
            if (itemAt(i).state === "expanded") {
                expandedCount++
            } else {
                totalFixedHeight += repeatRoot.collapsedFixedHeight
            }
        }
        return expandedCount > 0 ?
            (repeatRoot.containerHeight -
                (totalFixedHeight + spacersHeight + repeatRoot.containerMargins*2))
                    / expandedCount
            : 0
    }

    function calculateCollapsedHeight() {
        var totalFixedHeight = 0
        var expandedCount = 0
        var spacersHeight = (count) * repeatRoot.usedSpacing;
        // console.log(count, "col")
        for (var i = 0; i < count; i++) {
            if (!(itemAt(i) instanceof StatesSocket)) {
                // console.log("Object type:", getObjectType(itemAt(i)));
                continue
            }
            if (itemAt(i).state === "expanded") {
                expandedCount++
            }
        }
        if (expandedCount > 0) {
            return repeatRoot.collapsedFixedHeight
        } else {
            return (repeatRoot.containerHeight - (spacersHeight + repeatRoot.containerMargins*2))/count
        }
    }

    clip: true

    delegate: StatesSocket {
        id: delegateSoc
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        Layout.preferredHeight: state === "expanded" ?
                                    repeatRoot.calculateExpandedHeight() :
                                    repeatRoot.calculateCollapsedHeight()
        state: model.socketdisplaymode
        title: model.socketname
        socketId: index

        cutInstrumId:      model.cutmodeinstrid
        cutMaxPower:       model.cutmodemaxpower
        cutModePower:      model.cutmodepower
        cutModeId:         model.cutmodeid
        cutModeName:       model.cutmodename
        cutInstrumName:    model.cutmodeinstrname

        coagInstrumId:     model.coagmodeinstrid
        coagMaxPower:      model.coagmodemaxpower
        coagModePower:     model.coagmodepower
        coagModeId:        model.coagmodeid
        coagModeName:      model.coagmodename
        coagInstrumName:   model.coagmodeinstrname

        Component.onCompleted: {
            console.log("made sock item", delegateSoc.state)
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
            function onInstrumEditDialogRequest(socketid, iscoag) {

                repeatRoot.instrumDialogRequest(socketid,
                                                iscoag ? model.coagmodeindex : model.cutmodeindex,
                                                iscoag)
                console.log("prosim dislog instrumenta ")
            }
            function onModeEditDialogRequest(socketid, iscoag) {
                console.log("просим dialog rezhima",
                            iscoag ? model.coagmodeindex : model.cutmodeindex,
                            iscoag ? model.coagmodename : model.cutmodename, )
                repeatRoot.modeDialogRequest(socketid,
                                            iscoag ? model.coagmodeindex : model.cutmodeindex,
                                            iscoag)
                console.log("открылся dialog rezhima",
                            iscoag ? model.coagmodeindex : model.cutmodeindex,
                            iscoag ? model.coagmodename : model.cutmodename, )
            }
            function onNewPower(socketid, pwr, iscoag) {
                theModel.qmlSetData(index,
                                    pwr,
                                    (iscoag ? "coagmodepower" : "cutmodepower"))
            }
            function onSocketCollapseRequest() {
                theModel.qmlSetData(index, 0, "socketdisplaymode")
            }
            function onSocketExpandRequest() {
                theModel.qmlSetData(index, 1, "socketdisplaymode")
            }
        }
    }
}

