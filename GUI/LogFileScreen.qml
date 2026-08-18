import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import StratifyLabs.UI 2.0

Item {
    id: logScreen

    signal returnButtonPressed()
    signal logDownloadButtonPressed()

    readonly property int actionButtonHeight: 72
    readonly property int actionButtonWidth: 150
    readonly property int calendarPopupHeight: 750
    readonly property var monthNames: [
        qsTr("Январь"), qsTr("Февраль"), qsTr("Март"), qsTr("Апрель"),
        qsTr("Май"), qsTr("Июнь"), qsTr("Июль"), qsTr("Август"),
        qsTr("Сентябрь"), qsTr("Октябрь"), qsTr("Ноябрь"), qsTr("Декабрь")
    ]
    readonly property var weekdayLabels: [
        qsTr("Пн"), qsTr("Вт"), qsTr("Ср"), qsTr("Чт"),
        qsTr("Пт"), qsTr("Сб"), qsTr("Вс")
    ]

    property string currentFilter: "all"
    property string currentDate: ""
    property var logLines: []
    property var dateItems: []
    property int lineCount: 0
    property var calendarMonth: new Date()

    function formatDateKey(date) {
        if (!date || isNaN(date.getTime())) {
            return ""
        }
        var day = date.getDate()
        var month = date.getMonth() + 1
        var year = date.getFullYear()
        return ("0" + day).slice(-2) + "-"
                + ("0" + month).slice(-2) + "-"
                + year
    }

    function parseDateKey(dateKey) {
        var parts = dateKey.split("-")
        if (parts.length !== 3) {
            return new Date(NaN)
        }
        return new Date(parseInt(parts[2], 10),
                        parseInt(parts[1], 10) - 1,
                        parseInt(parts[0], 10))
    }

    function hasLogForDate(date) {
        return logScreen.dateItems.indexOf(logScreen.formatDateKey(date)) >= 0
    }

    function cellDate(index) {
        var year = logScreen.calendarMonth.getFullYear()
        var month = logScreen.calendarMonth.getMonth()
        var first = new Date(year, month, 1)
        var offset = (first.getDay() + 6) % 7
        return new Date(year, month, index - offset + 1)
    }

    function isInCurrentMonth(date) {
        return date.getMonth() === logScreen.calendarMonth.getMonth()
                && date.getFullYear() === logScreen.calendarMonth.getFullYear()
    }

    function monthYearLabel() {
        var monthIndex = logScreen.calendarMonth.getMonth()
        return logScreen.monthNames[monthIndex] + " " + logScreen.calendarMonth.getFullYear()
    }

    function syncCalendarMonth() {
        var parsed = logScreen.parseDateKey(logScreen.currentDate)
        if (!isNaN(parsed.getTime())) {
            logScreen.calendarMonth = new Date(parsed.getFullYear(), parsed.getMonth(), 1)
        }
    }

    function refreshDates() {
        if (typeof deviceLog === "undefined" || !deviceLog) {
            logScreen.dateItems = []
            return
        }

        logScreen.dateItems = deviceLog.availableLogDates()
        if (logScreen.currentDate.length === 0) {
            logScreen.currentDate = deviceLog.todayLogDate()
        } else if (logScreen.dateItems.indexOf(logScreen.currentDate) < 0) {
            logScreen.currentDate = logScreen.dateItems.length > 0
                    ? logScreen.dateItems[0]
                    : deviceLog.todayLogDate()
        }
        syncCalendarMonth()
    }

    function refreshLog() {
        if (typeof deviceLog === "undefined" || !deviceLog) {
            logScreen.logLines = [qsTr("Журнал недоступен")]
            logScreen.lineCount = 0
            return
        }

        if (logScreen.currentDate.length === 0) {
            logScreen.currentDate = deviceLog.todayLogDate()
        }

        logScreen.lineCount = deviceLog.logLineCount(logScreen.currentFilter, logScreen.currentDate)
        var lines = deviceLog.readLogLines(logScreen.currentFilter, logScreen.currentDate, 1000)
        logScreen.logLines = lines.length > 0 ? lines : [qsTr("Записей нет")]
        Qt.callLater(function() {
            if (logList.count > 0) {
                logList.positionViewAtEnd()
            }
        })
    }

    function selectFilter(filter) {
        logScreen.currentFilter = filter
        refreshLog()
    }

    function selectCalendarDate(date) {
        logScreen.currentDate = logScreen.formatDateKey(date)
        refreshLog()
        calendarPopup.close()
    }

    function shiftMonth(direction) {
        var year = logScreen.calendarMonth.getFullYear()
        var month = logScreen.calendarMonth.getMonth() + direction
        logScreen.calendarMonth = new Date(year, month, 1)
    }

    Component.onCompleted: {
        if (typeof deviceLog !== "undefined" && deviceLog) {
            logScreen.currentDate = deviceLog.todayLogDate()
            logScreen.calendarMonth = logScreen.parseDateKey(logScreen.currentDate)
        }
        refreshDates()
        refreshLog()
    }

    Rectangle {
        anchors.fill: parent
        color: "darkslategray"
    }

    SLabel {
        id: logTitle
        style: "label-primary lg"
        text: qsTr("Журнал событий (лог-файл)")
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
    }

    ColumnLayout {
        id: contentColumn
        anchors {
            top: logTitle.bottom
            left: parent.left
            right: parent.right
            bottom: retButton.top
            margins: 20
        }
        spacing: 12


        Row {
            Layout.fillWidth: true
            spacing: 20

            Text {
                Layout.fillWidth: true
                color: "#d0e8e8"
                font.pixelSize: 22
                text: qsTr("Фильтр:")
            }

            SButton {
                Layout.fillWidth: true
                height: logScreen.actionButtonHeight
                style: logScreen.currentFilter === "errors" ? "btn-danger lg" : "btn-primary lg"
                opacity: logScreen.currentFilter === "errors" ? 1.0 : 0.65
                text: qsTr("Ошибки")
                onPressed: logScreen.selectFilter("errors")
            }

            SButton {
                Layout.fillWidth: true
                height: logScreen.actionButtonHeight
                style: logScreen.currentFilter === "boots" ? "btn-success lg" : "btn-primary lg"
                opacity: logScreen.currentFilter === "boots" ? 1.0 : 0.65
                text: qsTr("Включения")
                onPressed: logScreen.selectFilter("boots")
            }

            SButton {
                Layout.fillWidth: true
                height: logScreen.actionButtonHeight
                style: logScreen.currentFilter === "all" ? "btn-success lg" : "btn-primary lg"
                opacity: logScreen.currentFilter === "all" ? 1.0 : 0.65
                text: qsTr("Всё")
                onPressed: logScreen.selectFilter("all")
            }

            Rectangle {
                width: 2
                height: logScreen.actionButtonHeight
                color: "white"
            }

            Text {
                Layout.fillWidth: true
                color: "#d0e8e8"
                font.pixelSize: 22
                text: qsTr("Дата:")
            }

            SButton {
                id: openCalendarButton
                Layout.fillWidth: true
                height: logScreen.actionButtonHeight
                style: "btn-outline-primary lg"
                text: logScreen.currentDate.length > 0 ? ("< " + logScreen.currentDate + " >") : qsTr("Не выбрана")
                onPressed: {
                    logScreen.syncCalendarMonth()
                    calendarPopup.open()
                }
            }

//            SButton {
//                id: refreshCalendarButton
//                Layout.fillWidth: true
//                height: logScreen.actionButtonHeight
//                style: "btn-primary lg"
//                text: qsTr("Обновить")
//                onPressed: {
//                    logScreen.refreshDates()
//                    logScreen.refreshLog()
//                }
//            }
        }

        Text {
            Layout.fillWidth: true
            color: "#d0e8e8"
            font.pixelSize: 22
            text: qsTr("Записей: %1").arg(logScreen.lineCount)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#102828"
            radius: 8
            border.color: "#5f7f7f"
            border.width: 1

            ListView {
                id: logList
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 8
                model: logScreen.logLines

                onCountChanged: Qt.callLater(function() {
                    if (logList.count > 0) {
                        logList.positionViewAtEnd()
                    }
                })

                delegate: Text {
                    width: logList.width
                    text: modelData
                    color: "white"
                    font.pixelSize: 20
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Popup {
        id: calendarPopup
        x: Math.round((logScreen.width - width) / 2)
        y: Math.round((logScreen.height - height) / 2)
        width: Math.min(logScreen.width - 60, 620)
        height: Math.min(logScreen.height - 80, logScreen.calendarPopupHeight)
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1a3535"
            radius: 8
            border.color: "#6a9a9a"
            border.width: 1
        }

        contentItem: Column {
            id: calendarPopupContent
            width: calendarPopup.width - 32
            spacing: 10

            Row {
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width - closeCalendarButton.width - 8
                    height: logScreen.actionButtonHeight
                    color: "#b8d8d8"
                    font.pixelSize: 24
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Выбор даты")
                }

                SButton {
                    id: closeCalendarButton
                    width: 140
                    height: logScreen.actionButtonHeight
                    style: "btn-secondary"
                    text: qsTr("Закрыть")
                    onPressed: calendarPopup.close()
                }
            }

            Row {
                width: parent.width
                spacing: 8

                SButton {
                    width: 64
                    height: logScreen.actionButtonHeight
                    style: "btn-primary lg"
                    text: "<"
                    onPressed: logScreen.shiftMonth(-1)
                }

                Text {
                    width: parent.width - 128
                    height: logScreen.actionButtonHeight
                    color: "white"
                    font.pixelSize: 24
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: logScreen.monthYearLabel()
                }

                SButton {
                    width: 64
                    height: logScreen.actionButtonHeight
                    style: "btn-primary lg"
                    text: ">"
                    onPressed: logScreen.shiftMonth(1)
                }
            }

            Grid {
                width: parent.width
                columns: 7
                rowSpacing: 2
                columnSpacing: 2

                Repeater {
                    model: logScreen.weekdayLabels
                    Text {
                        width: (parent.width - parent.columnSpacing * 6) / 7
                        height: 28
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: "#8aacac"
                        font.pixelSize: 18
                        text: modelData
                    }
                }
            }

            Grid {
                id: monthGrid
                width: parent.width
                columns: 7
                rows: 6
                rowSpacing: 2
                columnSpacing: 2

                readonly property real cellWidth: (width - columnSpacing * 6) / 7
                readonly property real cellHeight: 42

                Repeater {
                    model: 42

                    delegate: Rectangle {
                        readonly property var dayDate: logScreen.cellDate(index)
                        readonly property string dayKey: logScreen.formatDateKey(dayDate)
                        readonly property bool hasLog: logScreen.hasLogForDate(dayDate)
                        readonly property bool isSelected: dayKey === logScreen.currentDate
                        readonly property bool inCurrentMonth: logScreen.isInCurrentMonth(dayDate)

                        width: monthGrid.cellWidth
                        height: monthGrid.cellHeight
                        radius: 6
                        color: isSelected ? "#3a8a8a" : "transparent"
                        opacity: inCurrentMonth ? 1.0 : 0.28

                        Text {
                            anchors.centerIn: parent
                            text: dayDate.getDate()
                            color: hasLog ? "#9dff9d" : "#e8f4f4"
                            font.pixelSize: 20
                            font.bold: isSelected || hasLog
                        }

                        Rectangle {
                            anchors {
                                bottom: parent.bottom
                                bottomMargin: 4
                                horizontalCenter: parent.horizontalCenter
                            }
                            width: 8
                            height: 8
                            radius: 4
                            color: "#6fdc6f"
                            visible: hasLog && inCurrentMonth
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: logScreen.selectCalendarDate(dayDate)
                        }
                    }
                }
            }

            Text {
                width: parent.width
                color: "#d0e8e8"
                font.pixelSize: 20
                text: qsTr("Выбрано: %1").arg(logScreen.currentDate.length > 0
                                               ? logScreen.currentDate
                                               : "—")
            }
        }
    }

    DialogActionButton {
        id: retButton
        width: 180
        height: 72
        text: qsTr("НАЗАД")
        secondaryColor: "#264093"
        secondaryBorderWidth: 1
        secondaryBorderColor: "#1E3274"
        cornerRadius: 20
        labelPixelSize: 30
        onPressed: logScreen.returnButtonPressed()
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 15
        }
    }

    DialogActionButton {
        id: downloadLogButton
        width: 380
        height: 72
        text: qsTr("СКАЧАТЬ ЛОГ-ФАЙЛ")
        secondaryColor: "#264093"
        secondaryBorderWidth: 1
        secondaryBorderColor: "#1E3274"
        cornerRadius: 20
        labelPixelSize: 30
        onPressed: logScreen.logDownloadButtonPressed()
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 15
        }
    }
}
