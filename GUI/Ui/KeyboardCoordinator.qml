pragma Singleton
import QtQuick 2.15

// Резерв нижней полосы под ОТМЕНА/ПРИНЯТЬ, пока открыт диалог с клавиатурой.
// InputPanel поднимается на bottomReserve, чтобы не перекрывать кнопки.
QtObject {
    readonly property int actionBarHeight: 108
    readonly property int actionButtonWidth: 195
    readonly property int actionButtonHeight: 62
    readonly property int screenMargin: 20

    property int bottomReserve: 0
    property int _holders: 0

    function acquire(height) {
        var h = (height !== undefined && height > 0) ? height : actionBarHeight
        _holders += 1
        bottomReserve = h
    }

    function release() {
        _holders = Math.max(0, _holders - 1)
        if (_holders === 0)
            bottomReserve = 0
    }

    function keyboardGap(screenHeight) {
        if (!Qt.inputMethod.visible)
            return 0
        var h = screenHeight > 0 ? screenHeight : 800
        var reserve = bottomReserve
        var rect = Qt.inputMethod.keyboardRectangle
        if (rect.height > 0) {
            var fromTop = Math.max(0, Math.round(h - reserve - rect.y))
            if (fromTop > 40)
                return fromTop
            return Math.max(0, Math.round(rect.height))
        }
        // InputPanel: height ≈ width/4 на экране 1280 → ~320
        return Math.min(Math.round(h * 0.42), Math.max(0, h - reserve - 200))
    }
}
