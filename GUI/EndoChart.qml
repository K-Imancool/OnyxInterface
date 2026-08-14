import QtQuick 2.15

Rectangle {
    id: root
    color: "transparent"
    clip: true

    property int cutEffect: 1
    property int coagEffect: 1
    property string modeName: ""
    property int pulseLevel: -1

    readonly property int safeCut: Math.max(1, Math.min(3, cutEffect))
    readonly property int safeCoag: Math.max(1, Math.min(3, coagEffect))
    readonly property int modeLevel: {
        if (pulseLevel >= 0 && pulseLevel <= 3)
            return pulseLevel
        var match = modeName.match(/([0-3])\s*$/)
        return match ? parseInt(match[1]) : 2
    }
    readonly property bool usePulseAnimation: true
    readonly property bool isSingleShot: modeLevel === 0
    readonly property int cycleCount: isSingleShot ? 1 : (modeLevel + 1)
    readonly property int cutPulseMs: {
        if (modeLevel === 0) return 300
        if (modeLevel === 1) return 200
        if (modeLevel === 3) return 100
        return 150
    }
    readonly property int coagPulseMs: {
        if (modeLevel === 0) return 1400
        if (modeLevel === 1) return 1000
        if (modeLevel === 3) return 450
        return 700
    }
    readonly property real yellowHeight: height * 0.8
    readonly property real blueHeight: height * (safeCoag * 0.1)
    readonly property real sidePadding: Math.max(8, width * 0.04)
    readonly property real availableWidth: Math.max(0, width - sidePadding * 2)
    readonly property real chartDrawWidth: isSingleShot ? availableWidth / 2 : availableWidth
    readonly property real cycleWidth: cycleCount > 0 ? chartDrawWidth / cycleCount : 0
    readonly property real cutRatio: 0.12 + ((safeCut - 1) * 0.06)
    readonly property real yellowWidth: Math.max(4, cycleWidth * cutRatio)
    readonly property real blueWidth: Math.max(4, cycleWidth - yellowWidth)
    property int activeCycle: 0
    property bool activeCutPulse: true
    property bool singleShotArmed: false
    property bool singleShotFinished: false

    function resetPulseState() {
        activeCycle = 0
        activeCutPulse = true
        singleShotFinished = false
    }

    function startPulseAnimation() {
        resetPulseState()
        if (isSingleShot)
            singleShotArmed = true
    }

    Timer {
        id: pulseTimer
        running: root.usePulseAnimation && root.cycleCount > 0
                 && (!root.isSingleShot || (root.singleShotArmed && !root.singleShotFinished))
        repeat: true
        interval: root.activeCutPulse ? root.cutPulseMs : root.coagPulseMs
        onTriggered: {
            if (root.activeCutPulse) {
                root.activeCutPulse = false
            } else {
                if (root.isSingleShot) {
                    root.singleShotFinished = true
                    root.singleShotArmed = false
                    return
                }
                root.activeCutPulse = true
                root.activeCycle = (root.activeCycle + 1) % root.cycleCount
            }
        }
    }

    onCutEffectChanged: {
        if (root.isSingleShot)
            root.startPulseAnimation()
    }
    onCoagEffectChanged: {
        if (root.isSingleShot)
            root.startPulseAnimation()
    }
    onModeNameChanged: startPulseAnimation()
    onCycleCountChanged: startPulseAnimation()
    onPulseLevelChanged: startPulseAnimation()
    onVisibleChanged: {
        if (visible)
            startPulseAnimation()
    }

    Rectangle {
        x: root.sidePadding
        y: 0
        width: root.chartDrawWidth
        height: parent.height
        color: "transparent"
        border.width: 1
        border.color: "#55FFFFFF"
        z: 2
    }

    Repeater {
        model: root.cycleCount
        delegate: Item {
            width: root.cycleWidth
            height: root.height
            x: root.sidePadding + index * root.cycleWidth

            Rectangle {
                width: root.yellowWidth
                height: root.yellowHeight
                color: "#F9D648"
                opacity: {
                    if (!root.usePulseAnimation)
                        return 1.0
                    if (root.isSingleShot && root.singleShotFinished)
                        return 1.0
                    return (root.activeCutPulse && root.activeCycle === index) ? 1.0 : 0.35
                }
                anchors.left: parent.left
                anchors.bottom: parent.bottom
            }

            Rectangle {
                width: root.blueWidth
                height: root.blueHeight
                color: "#1565C0"
                opacity: {
                    if (!root.usePulseAnimation)
                        return 1.0
                    if (root.isSingleShot && root.singleShotFinished)
                        return 1.0
                    return (!root.activeCutPulse && root.activeCycle === index) ? 1.0 : 0.35
                }
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }
        }
    }

    Text {
        text: qsTr("рез")
        font.pixelSize: 13
        font.bold: true
        color: uiMidGray
        x: root.sidePadding + 2
        y: Math.max(0, root.height - root.yellowHeight - height - 2)
        z: 3
    }

    Text {
        text: qsTr("коаг")
        font.pixelSize: 13
        font.bold: true
        color: uiMidGray
        x: root.sidePadding + root.yellowWidth + 2
        y: Math.max(0, root.height - root.blueHeight - height - 2)
        z: 3
    }
}
