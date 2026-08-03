import QtQuick 2.15
import QtQuick.Controls 2.15
import BackEnd 1.0
import org.freedesktop.gstreamer.GLVideoItem 1.0
import StratifyLabs.UI 2.0

Rectangle {
    id: videoPlayerRoot

    anchors.fill: parent
    color: "black"

    signal closeRequested()

    readonly property int playlistWidth: 320
    readonly property int controlsHeight: 160
    readonly property int controlButtonSize: 54
    readonly property int controlIconSize: 32
    readonly property color sliderActiveColor: "#F2F2F2"
    readonly property color sliderInactiveColor: "#4A4A4A"
    readonly property bool playerControlsVisible:
        !fullScreen
        || mediaPlayer.playbackState !== GStreamerVideoPlayer.PlayingState
    property string videoFolder: AppPaths.videoDir
    property var videoFiles: []
    property int currentVideoIndex: -1
    property bool fullScreen: false
    property bool showLoadingOnPlay: false

    Component.onCompleted: loadVideoFiles()

    Component.onDestruction: {
        hideLoadingTimer.stop()
        showLoadingOnPlay = false
        mediaPlayer.shutdown()
    }

    // Перехват касаний в пустых зонах оверлея, иначе они проходят
    // сквозь Rectangle к кнопкам MainMenu под Loader.
    MouseArea {
        anchors.fill: parent
    }

    function loadVideoFiles() {
        videoFiles = mediaPlayer.scanVideoFiles(videoFolder)
        currentVideoIndex = -1

        if (videoFiles.length > 0) {
            loadVideo(0)
        }
    }

    function loadVideo(index) {
        if (index < 0 || index >= videoFiles.length) {
            return
        }

        currentVideoIndex = index
        showLoadingOnPlay = false
        hideLoadingTimer.stop()
        errorText.visible = false

        mediaPlayer.setLocalFile(videoFolder + "/" + videoFiles[index])
    }

    function togglePlayback() {
        if (currentVideoIndex < 0 || currentVideoIndex >= videoFiles.length) {
            return
        }

        if (mediaPlayer.playbackState === GStreamerVideoPlayer.PlayingState) {
            mediaPlayer.pause()
            return
        }

        showLoadingOnPlay = true
        mediaPlayer.play()
        hideLoadingTimer.restart()
    }

    function selectPreviousVideo() {
        if (currentVideoIndex > 0) {
            loadVideo(currentVideoIndex - 1)
        }
    }

    function selectNextVideo() {
        if (currentVideoIndex < videoFiles.length - 1) {
            loadVideo(currentVideoIndex + 1)
        }
    }

    function closePlayer() {
        mediaPlayer.shutdown()
        showLoadingOnPlay = false
        closeRequested()
    }

    function formatTime(milliseconds) {
        var totalSeconds = Math.max(0, Math.floor(milliseconds / 1000))
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60

        if (hours > 0) {
            return hours + ":"
                    + (minutes < 10 ? "0" : "") + minutes + ":"
                    + (seconds < 10 ? "0" : "") + seconds
        }

        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    Timer {
        id: hideLoadingTimer

        interval: 3000
        repeat: false
        onTriggered: videoPlayerRoot.showLoadingOnPlay = false
    }

    Rectangle {
        id: playlistPanel

        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
        }
        width: videoPlayerRoot.playlistWidth
        color: "#202124"
        visible: !videoPlayerRoot.fullScreen

        Text {
            id: playlistTitle

            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                margins: 18
            }
            height: 44
            text: qsTr("Видеозаписи")
            color: "white"
            font.pixelSize: 26
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            anchors {
                top: playlistTitle.bottom
                left: parent.left
                right: parent.right
                margins: 18
            }
            height: 1
            color: "#5F6368"
        }

        ListView {
            id: playlistView

            anchors {
                top: playlistTitle.bottom
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                topMargin: 16
                bottomMargin: 14
                leftMargin: 12
                rightMargin: 12
            }
            model: videoFiles
            spacing: 7
            clip: true
            currentIndex: currentVideoIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                width: playlistView.width
                height: 62
                radius: 7
                color: index === currentVideoIndex ? "#264093" : "#35363A"
                border.color: index === currentVideoIndex ? "#FAA731" : "#5F6368"
                border.width: index === currentVideoIndex ? 2 : 1

                Text {
                    anchors {
                        fill: parent
                        leftMargin: 12
                        rightMargin: 12
                    }
                    text: modelData
                    color: "white"
                    font.pixelSize: 17
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: videoPlayerRoot.loadVideo(index)
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                text: qsTr("В папке нет видеофайлов")
                color: "#BDC1C6"
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: videoFiles.length === 0
            }
        }
    }

    Item {
        id: playerArea

        anchors {
            top: parent.top
            bottom: parent.bottom
            left: videoPlayerRoot.fullScreen ? parent.left : playlistPanel.right
            right: parent.right
        }

        Item {
            id: videoOutput

            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                bottom: videoPlayerRoot.fullScreen ? parent.bottom : controlsArea.top
            }

            GstGLVideoItem {
                id: videoSurface

                objectName: "videoSurface"
                anchors.fill: parent
            }

            MouseArea {
                anchors.fill: parent
                onClicked: videoPlayerRoot.togglePlayback()
            }

            Text {
                id: errorText

                anchors.centerIn: parent
                width: parent.width - 80
                text: ""
                color: "#FF5252"
                font.pixelSize: 24
                font.bold: true
                visible: false
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Rectangle {
                id: loadingIndicator

                anchors.centerIn: parent
                width: 200
                height: 145
                color: "#D9000000"
                radius: 10
                visible: (mediaPlayer.buffering
                          || mediaPlayer.status === GStreamerVideoPlayer.Loading
                          || videoPlayerRoot.showLoadingOnPlay)
                         && !errorText.visible

                Column {
                    anchors.centerIn: parent
                    spacing: 14

                    BusyIndicator {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 58
                        height: 58
                        running: loadingIndicator.visible
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Загрузка...")
                        color: "white"
                        font.pixelSize: 18
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 330
                height: 145
                color: "#B3000000"
                radius: 10
                visible: videoFiles.length > 0
                         && mediaPlayer.playbackState !== GStreamerVideoPlayer.PlayingState
                         && !errorText.visible
                         && !loadingIndicator.visible

                Column {
                    anchors.centerIn: parent
                    spacing: 16

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "▶"
                        color: "white"
                        font.pixelSize: 56
                        font.family: "DejaVu Sans"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Нажмите для воспроизведения")
                        color: "white"
                        font.pixelSize: 18
                    }
                }
            }
        }

        Rectangle {
            id: controlsArea

            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }
            height: videoPlayerRoot.controlsHeight
            color: videoPlayerRoot.fullScreen ? "#D9000000" : "#202124"
            visible: videoPlayerRoot.playerControlsVisible
            z: 100

            Column {
                anchors {
                    fill: parent
                    margins: 10
                }
                spacing: 8

                Text {
                    width: parent.width
                    height: 34
                    text: currentVideoIndex >= 0 && currentVideoIndex < videoFiles.length
                          ? videoFiles[currentVideoIndex]
                          : qsTr("Видео не выбрано")
                    color: "white"
                    font.pixelSize: 18
                    font.bold: true
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                Row {
                    width: parent.width
                    height: 32
                    spacing: 10

                    Text {
                        width: 54
                        text: formatTime(progressSlider.pressed
                                         ? progressSlider.value
                                         : mediaPlayer.position)
                        color: "white"
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }

                    Slider {
                        id: progressSlider

                        width: parent.width - 128
                        height: parent.height
                        from: 0
                        to: Math.max(0, mediaPlayer.duration)
                        value: 0
                        enabled: mediaPlayer.seekable && mediaPlayer.duration > 0
                        onPressedChanged: {
                            if (!pressed) {
                                mediaPlayer.seek(value)
                            }
                        }

                        background: Rectangle {
                            x: progressSlider.leftPadding
                            y: progressSlider.topPadding
                               + progressSlider.availableHeight / 2 - height / 2
                            width: progressSlider.availableWidth
                            height: 6
                            radius: height / 2
                            color: videoPlayerRoot.sliderInactiveColor

                            Rectangle {
                                width: progressSlider.visualPosition * parent.width
                                height: parent.height
                                radius: parent.radius
                                color: videoPlayerRoot.sliderActiveColor
                            }
                        }

                        handle: Rectangle {
                            x: progressSlider.leftPadding
                               + progressSlider.visualPosition
                                 * (progressSlider.availableWidth - width)
                            y: progressSlider.topPadding
                               + progressSlider.availableHeight / 2 - height / 2
                            width: 18
                            height: 18
                            radius: width / 2
                            color: videoPlayerRoot.sliderActiveColor
                            border.color: "#707070"
                            border.width: 1
                        }
                    }

                    Text {
                        width: 54
                        text: formatTime(mediaPlayer.duration)
                        color: "white"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Row {
                    width: parent.width
                    height: 58
                    spacing: 12

                    Item {
                        width: parent.width - volumeControls.width - parent.spacing
                        height: parent.height

                        Row {
                            anchors.centerIn: parent
                            spacing: 20

                            SButton {
                                width: videoPlayerRoot.controlButtonSize
                                height: videoPlayerRoot.controlButtonSize
                                text: ""
                                style: "btn-outline-light"
                                enabled: currentVideoIndex > 0
                                onClicked: videoPlayerRoot.selectPreviousVideo()

                                Text {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: 5
                                    text: "⏮"
                                    color: "white"
//                                    opacity: parent.enabled ? 1.0 : 0.45
                                    font.family: "DejaVu Sans"
                                    font.pixelSize: videoPlayerRoot.controlIconSize
                                }
                            }

                            SButton {
                                width: videoPlayerRoot.controlButtonSize
                                height: videoPlayerRoot.controlButtonSize
                                text: ""
                                style: "btn-outline-light"
                                enabled: currentVideoIndex >= 0
                                onClicked: videoPlayerRoot.togglePlayback()

                                Text {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset:
                                        mediaPlayer.playbackState === GStreamerVideoPlayer.PlayingState
                                        ? 5 : -2
                                    text: mediaPlayer.playbackState === GStreamerVideoPlayer.PlayingState
                                          ? "⏸" : "▶"
                                    color: "white"
//                                    opacity: parent.enabled ? 1.0 : 0.45
                                    font.family: "DejaVu Sans"
                                    font.pixelSize: videoPlayerRoot.controlIconSize
                                }
                            }

                            SButton {
                                width: videoPlayerRoot.controlButtonSize
                                height: videoPlayerRoot.controlButtonSize
                                text: ""
                                style: "btn-outline-light"
                                enabled: currentVideoIndex >= 0
                                onClicked: {
                                    mediaPlayer.stop()
                                    progressSlider.value = 0
                                }

                                Text {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: 5
                                    text: "⏹"
                                    color: "white"
//                                    opacity: parent.enabled ? 1.0 : 0.45
                                    font.family: "DejaVu Sans"
                                    font.pixelSize: videoPlayerRoot.controlIconSize
                                }
                            }

                            SButton {
                                width: videoPlayerRoot.controlButtonSize
                                height: videoPlayerRoot.controlButtonSize
                                text: ""
                                style: "btn-outline-light"
                                enabled: currentVideoIndex >= 0
                                         && currentVideoIndex < videoFiles.length - 1
                                onClicked: videoPlayerRoot.selectNextVideo()

                                Text {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: 5
                                    text: "⏭"
                                    color: "white"
//                                    opacity: parent.enabled ? 1.0 : 0.45
                                    font.family: "DejaVu Sans"
                                    font.pixelSize: videoPlayerRoot.controlIconSize
                                }
                            }
                        }
                    }

                    Row {
                        id: volumeControls

                        width: 290
                        height: parent.height
                        spacing: 8

                        SButton {
                            width: videoPlayerRoot.controlButtonSize
                            height: videoPlayerRoot.controlButtonSize
                            anchors.verticalCenter: parent.verticalCenter
                            text: "−"
                            style: "btn-outline-light"
                            font.family: "DejaVu Sans"
                            font.pixelSize: 56
                            onClicked: mediaPlayer.volume = Math.max(0, mediaPlayer.volume - 0.1)
                        }

                        Text {
                            width: 28
                            anchors.verticalCenter: parent.verticalCenter
                            text: mediaPlayer.muted || mediaPlayer.volume === 0 ? "🔇" : "🔈"
                            color: "white"
                            font.family: "Symbola"
                            font.pixelSize: 32
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Slider {
                            id: volumeSlider

                            width: 130
                            height: parent.height
                            from: 0
                            to: 1
                            stepSize: 0.01
                            value: mediaPlayer.volume
                            onMoved: mediaPlayer.volume = value

                            background: Rectangle {
                                x: volumeSlider.leftPadding
                                y: volumeSlider.topPadding
                                   + volumeSlider.availableHeight / 2 - height / 2
                                width: volumeSlider.availableWidth
                                height: 6
                                radius: height / 2
                                color: videoPlayerRoot.sliderInactiveColor

                                Rectangle {
                                    width: volumeSlider.visualPosition * parent.width
                                    height: parent.height
                                    radius: parent.radius
                                    color: videoPlayerRoot.sliderActiveColor
                                }
                            }

                            handle: Rectangle {
                                x: volumeSlider.leftPadding
                                   + volumeSlider.visualPosition
                                     * (volumeSlider.availableWidth - width)
                                y: volumeSlider.topPadding
                                   + volumeSlider.availableHeight / 2 - height / 2
                                width: 18
                                height: 18
                                radius: width / 2
                                color: videoPlayerRoot.sliderActiveColor
                                border.color: "#707070"
                                border.width: 1
                            }
                        }

                        SButton {
                            width: videoPlayerRoot.controlButtonSize
                            height: videoPlayerRoot.controlButtonSize
                            anchors.verticalCenter: parent.verticalCenter
                            text: "+"
                            style: "btn-outline-light"
                            font.family: "DejaVu Sans"
                            font.pixelSize: 56
                            onClicked: mediaPlayer.volume = Math.min(1, mediaPlayer.volume + 0.1)
                        }
                    }
                }
            }
        }

        Row {
            anchors {
                top: parent.top
                right: parent.right
                margins: 10
            }
            height: 58
            spacing: 10
            visible: videoPlayerRoot.playerControlsVisible
            z: 200

            SButton {
                width: 230
                height: 58
                text: qsTr("НА ВЕСЬ ЭКРАН")
                style: "btn-outline-light"
                visible: !videoPlayerRoot.fullScreen
                onClicked: videoPlayerRoot.fullScreen = true
            }

            SButton {
                width: 70
                height: 58
                text: "✕"
//                text: videoPlayerRoot.fullScreen ? qsTr("СВЕРНУТЬ") : "✕"
                style: videoPlayerRoot.fullScreen
                       ? "btn-outline-light" : "btn-warning"
//                         ? "btn-outline-light" : "btn-danger"
                font.family: "DejaVu Sans"
                opacity: videoPlayerRoot.fullScreen ? 0.2 : 1.0
                onClicked: {
                    if (videoPlayerRoot.fullScreen) {
                        videoPlayerRoot.fullScreen = false
                    } else {
                        videoPlayerRoot.closePlayer()
                    }
                }
            }
        }
    }

    // Объявлен после GstGLVideoItem, чтобы backend завершил pipeline
    // до уничтожения поверхности при выгрузке Loader.
    GStreamerVideoPlayer {
        id: mediaPlayer

        videoItem: videoSurface
        volume: 0.8
        muted: false

        onPositionChanged: {
            if (!progressSlider.pressed) {
                progressSlider.value = position
            }
            if (playbackState === GStreamerVideoPlayer.PlayingState
                    && position > 0) {
                videoPlayerRoot.showLoadingOnPlay = false
                hideLoadingTimer.stop()
            }
        }

        onStatusChanged: {
            if (status === GStreamerVideoPlayer.Loaded) {
                errorText.visible = false
            } else if (status === GStreamerVideoPlayer.EndOfMedia) {
                videoPlayerRoot.showLoadingOnPlay = false
                hideLoadingTimer.stop()
            }
        }

        onPlaybackStateChanged: {
            if (playbackState !== GStreamerVideoPlayer.PlayingState) {
                videoPlayerRoot.showLoadingOnPlay = false
                hideLoadingTimer.stop()
            }
        }

        onErrorOccurred: function(message) {
            errorText.text = qsTr("Ошибка воспроизведения: ") + message
            errorText.visible = true
            videoPlayerRoot.showLoadingOnPlay = false
            hideLoadingTimer.stop()
        }
    }
}
