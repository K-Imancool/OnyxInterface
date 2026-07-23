import QtQuick 2.15
import QtQuick.Controls 2.15
import QtMultimedia 5.15
import StratifyLabs.UI 2.0

Rectangle {
    id: videoPlayerRoot
    anchors.fill: parent
    color: "black"
    
    signal closeRequested()
    
    property string videoFolder: AppPaths.videoDir
    property var videoFiles: []
    property int currentVideoIndex: -1
    property bool showLoadingOnPlay: false  // Показывать индикатор при нажатии Play
    
    Component.onCompleted: {
        loadVideoFiles()
        // Запускаем мониторинг системы
        sysMonitor.start()
    }
    
    Component.onDestruction: {
        // Останавливаем мониторинг при закрытии
        sysMonitor.stop()
    }
    
    function loadVideoFiles() {
        videoFiles = recomHandle.scanVideoFiles(videoFolder)
        if (videoFiles.length > 0) {
            currentVideoIndex = 0
            // НЕ запускаем автоматически, только загружаем
            loadVideo(0)
        }
    }
    
    function loadVideo(index) {
        if (index >= 0 && index < videoFiles.length) {
            currentVideoIndex = index
            var filePath = videoFolder + "/" + videoFiles[index].trim()
            var fileUrl = "file://" + filePath
            
            // НЕ показываем индикатор при загрузке - он покажется автоматически через isBuffering
            // при реальной загрузке/буферизации
            
            mediaPlayer.stop()
            mediaPlayer.source = ""
            mediaPlayer.source = fileUrl
            // НЕ вызываем play() автоматически
        }
    }
    
    function playVideo(index) {
        if (index >= 0 && index < videoFiles.length) {
            if (currentVideoIndex !== index) {
                // Если меняем видео - загружаем новое
                loadVideo(index)
            }
            // Показываем индикатор при нажатии Play
            showLoadingOnPlay = true
            // Запускаем воспроизведение сразу
            mediaPlayer.play()
            // Скрываем индикатор через небольшую задержку после начала воспроизведения
            hideLoadingTimer.restart()
        }
    }
    
    // Таймер для скрытия индикатора после начала воспроизведения
    Timer {
        id: hideLoadingTimer
        interval: 300  // 300ms после начала воспроизведения
        repeat: false
        onTriggered: {
            if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                showLoadingOnPlay = false
            }
        }
    }
    
    
    function formatTime(milliseconds) {
        var seconds = Math.floor(milliseconds / 1000)
        var minutes = Math.floor(seconds / 60)
        seconds = seconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }
    
    MediaPlayer {
        id: mediaPlayer
        autoPlay: false
        autoLoad: true  // Предзагрузка для уменьшения буферизации
        volume: 0.8  // Начальная громкость 80%
        muted: false  // Убеждаемся что звук не заглушен
        
        // Настройки для уменьшения сбрасывания буферов
        notifyInterval: 100  // Обновление позиции каждые 100мс вместо дефолтных 1000мс
        
        // Логирование для отладки аудио
        Component.onCompleted: {
            // console.log("MediaPlayer - hasAudio:", mediaPlayer.hasAudio)
            // console.log("MediaPlayer - volume:", mediaPlayer.volume)
            // console.log("MediaPlayer - muted:", mediaPlayer.muted)
        }
        
        onHasAudioChanged: {
            // console.log("Audio track detected:", hasAudio)
        }
        
        property bool isBuffering: {
            // Показываем индикатор при:
            // 1. Загрузке (Loading)
            // 2. Буферизации (Buffering)
            // 3. Явном запросе показа при нажатии Play
            var result = status === MediaPlayer.Loading || 
                        status === MediaPlayer.Buffering ||
                        videoPlayerRoot.showLoadingOnPlay
            return result
        }
        
        onPositionChanged: {
            // Когда position начинает изменяться - видео реально играет
            if (playbackState === MediaPlayer.PlayingState && position > 0) {
                videoPlayerRoot.showLoadingOnPlay = false
            }
        }
        
        onStatusChanged: {
            // Логирование загрузки медиа
            if (status === MediaPlayer.Loaded) {
                // console.log("Media loaded - hasAudio:", hasAudio, "hasVideo:", hasVideo)
            }
            
            // Обработка окончания воспроизведения
            if (status === MediaPlayer.EndOfMedia) {
                // Автоматически переходим к следующему видео
                if (currentVideoIndex < videoFiles.length - 1) {
                    playVideo(currentVideoIndex + 1)
                }
            } else if (status === MediaPlayer.InvalidMedia) {
                errorText.text = qsTr("Ошибка: Неподдерживаемый формат видео")
                errorText.visible = true
                videoPlayerRoot.showLoadingOnPlay = false
            } else {
                errorText.visible = false
            }
        }
        
        onPlaybackStateChanged: {
            if (playbackState === MediaPlayer.StoppedState || playbackState === MediaPlayer.PausedState) {
                // Видео остановлено/на паузе - скрываем индикатор
                videoPlayerRoot.showLoadingOnPlay = false
                hideLoadingTimer.stop()
            }
        }
        onError: {
            errorText.text = qsTr("Ошибка воспроизведения: ") + errorString
            errorText.visible = true
        }
    }
    
    VideoOutput {
        id: videoOutput
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: controlsArea.top
        }
        source: mediaPlayer
        fillMode: VideoOutput.PreserveAspectFit
        
        // Оптимизация для уменьшения нагрузки
        orientation: 0
        flushMode: VideoOutput.EmptyFrame  // Очищаем при остановке
        
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                    mediaPlayer.pause()
                } else {
                    mediaPlayer.play()
                }
            }
        }
        
        // Текст ошибки
        Text {
            id: errorText
            anchors.centerIn: parent
            text: ""
            color: "red"
            font.pixelSize: 24
            font.bold: true
            visible: false
            horizontalAlignment: Text.AlignHCenter
            width: parent.width - 40
            wrapMode: Text.WordWrap
        }
        
        // Индикатор загрузки/буферизации
        Rectangle {
            id: loadingIndicator
            anchors.centerIn: parent
            width: 200
            height: 150
            color: "#CC000000"
            radius: 10
            visible: mediaPlayer.isBuffering && !errorText.visible
            
            
            Column {
                anchors.centerIn: parent
                spacing: 15
                
                // Вращающийся индикатор загрузки
                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 60
                    height: 60
                    running: loadingIndicator.visible
                }
                
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Загрузка...")
                    color: "white"
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
        
        // Подсказка для пользователя
        Rectangle {
            anchors.centerIn: parent
            width: 300
            height: 150
            color: "#CC000000"
            radius: 10
            visible: videoFiles.length > 0 && 
                     mediaPlayer.playbackState !== MediaPlayer.PlayingState &&
                     !errorText.visible &&
                     !loadingIndicator.visible
            
            Column {
                anchors.centerIn: parent
                spacing: 20
                
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "▶"
                    color: "white"
                    font.pixelSize: 60
                    font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                }
                
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Нажмите ▶ для воспроизведения")
                    color: "white"
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
        
        // Информация о видео (для отладки)
        Text {
            id: debugInfo
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 10
            color: "white"
            font.pixelSize: 12
            text: "Files: " + videoFiles.length + " | Current: " + currentVideoIndex +
                  "\nSource: " + mediaPlayer.source +
                  "\nStatus: " + mediaPlayer.status +
                  "\nState: " + mediaPlayer.playbackState
            style: Text.Outline
            styleColor: "black"
            visible: false  // Отключаем отладочную информацию
        }
        
        // Информация о системе (CPU и температура)
        Rectangle {
            id: systemInfoPanel
            anchors {
                left: parent.left
                bottom: parent.bottom
                margins: 10
            }
            width: 200
            height: 80
            color: "#DD000000"
            radius: 8
            border.color: "#4CAF50"
            border.width: 2
            
            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5
                
                // Загрузка CPU
                Row {
                    width: parent.width
                    spacing: 8
                    
                    Text {
                        text: "CPU:"
                        color: "#4CAF50"
                        font.pixelSize: 16
                        font.bold: true
                        width: 50
                    }
                    
                    Text {
                        text: sysMonitor.cpuUsage.toFixed(1) + "%"
                        color: "white"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    
                    // Индикатор загрузки
                    Rectangle {
                        width: 60
                        height: 10
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#333333"
                        radius: 5
                        border.color: "#666666"
                        border.width: 1
                        
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: parent.width * Math.min(sysMonitor.cpuUsage / 100, 1.0)
                            radius: 5
                            color: {
                                if (sysMonitor.cpuUsage < 50) return "#4CAF50"
                                else if (sysMonitor.cpuUsage < 80) return "#FFC107"
                                else return "#F44336"
                            }
                        }
                    }
                }
                
                // Температура
                Row {
                    width: parent.width
                    spacing: 8
                    
                    Text {
                        text: "Temp:"
                        color: "#2196F3"
                        font.pixelSize: 16
                        font.bold: true
                        width: 50
                    }
                    
                    Text {
                        text: sysMonitor.temperature.toFixed(1) + "°C"
                        color: {
                            if (sysMonitor.temperature < 60) return "white"
                            else if (sysMonitor.temperature < 80) return "#FFC107"
                            else return "#F44336"
                        }
                        font.pixelSize: 18
                        font.bold: true
                    }
                    
                    // Температурный индикатор
                    Text {
                        text: {
                            if (sysMonitor.temperature < 60) return "❄"
                            else if (sysMonitor.temperature < 80) return "☀"
                            else return "🔥"
                        }
                        color: "white"
                        font.pixelSize: 16
                        font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }
    
    // Область управления
    Rectangle {
        id: controlsArea
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 160
        color: "#CC000000"
        z: 100  // Поднимаем над видео
        
        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10
            
            // Информация о текущем видео
            Text {
                id: videoTitle
                width: parent.width
                text: {
                    if (videoFiles.length === 0) {
                        return qsTr("Нет видеофайлов в папке ") + videoFolder
                    } else if (currentVideoIndex >= 0 && currentVideoIndex < videoFiles.length) {
                        return videoFiles[currentVideoIndex]
                    } else {
                        return qsTr("Нет видео")
                    }
                }
                color: "white"
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                maximumLineCount: 2
            }
            
            // Прогресс-бар
            Row {
                width: parent.width
                spacing: 10
                
                Text {
                    text: formatTime(mediaPlayer.position)
                    color: "white"
                    font.pixelSize: 14
                    width: 50
                }
                
                Slider {
                    id: progressSlider
                    width: parent.width - 120
                    from: 0
                    to: mediaPlayer.duration
                    value: mediaPlayer.position
                    onMoved: mediaPlayer.seek(value)
                }
                
                Text {
                    text: formatTime(mediaPlayer.duration)
                    color: "white"
                    font.pixelSize: 14
                    width: 50
                }
            }
            
            // Кнопки управления и громкость в одной строке
            Row {
                width: parent.width
                height: 60
                spacing: 20
                
                // Кнопки управления (слева)
                Item {
                    width: parent.width - 300 - parent.spacing
                    height: parent.height
                    
                    Row {
                        anchors.centerIn: parent
                        spacing: 20
                        
                        // Предыдущее видео
                        SButton {
                            width: 80
                            height: 60
                            text: "⏮"
                            style: "btn-outline-light"
                            font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                            enabled: currentVideoIndex > 0
                            onClicked: playVideo(currentVideoIndex - 1)
                        }
                        
                        // Play/Pause
                        SButton {
                            width: 80
                            height: 60
                            text: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "⏸" : "▶"
                            style: "btn-outline-light"
                            font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                            enabled: videoFiles.length > 0
                            onClicked: {
                                if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                                    mediaPlayer.pause()
                                } else {
                                    mediaPlayer.play()
                                }
                            }
                        }
                        
                        // Stop
                        SButton {
                            width: 80
                            height: 60
                            text: "⏹"
                            style: "btn-outline-light"
                            font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                            enabled: videoFiles.length > 0
                            onClicked: {
                                mediaPlayer.stop()
                                mediaPlayer.seek(0)
                            }
                        }
                        
                        // Следующее видео
                        SButton {
                            width: 80
                            height: 60
                            text: "⏭"
                            style: "btn-outline-light"
                            font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                            enabled: currentVideoIndex < videoFiles.length - 1
                            onClicked: playVideo(currentVideoIndex + 1)
                        }
                    }
                }
                
                // Регулировка громкости (справа)
                Item {
                    width: 300
                    height: parent.height
                    
                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10
                        
                        // Кнопка уменьшения громкости
                        SButton {
                            width: 50
                            height: 50
                            text: "🔉"
                            style: "btn-outline-light"
                            font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                            onClicked: {
                                var newVolume = Math.max(0, mediaPlayer.volume - 0.1)
                                mediaPlayer.volume = newVolume
                            }
                        }
                        
                        // Слайдер громкости
                        Item {
                            width: 180
                            height: 50
                            
                            Text {
                                id: volumeLabel
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text: "🔊"
                                color: "white"
                                font.pixelSize: 20
                                font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                                width: 30
                            }
                            
                            Slider {
                                id: volumeSlider
                                anchors.left: volumeLabel.right
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 10
                                from: 0.0
                                to: 1.0
                                stepSize: 0.01
                                
                                Component.onCompleted: {
                                    value = mediaPlayer.volume
                                }
                                
                                onValueChanged: {
                                    if (Math.abs(mediaPlayer.volume - value) > 0.01) {
                                        mediaPlayer.volume = value
                                    }
                                }
                                
                                Connections {
                                    target: mediaPlayer
                                    function onVolumeChanged() {
                                        if (Math.abs(volumeSlider.value - mediaPlayer.volume) > 0.01) {
                                            volumeSlider.value = mediaPlayer.volume
                                        }
                                    }
                                }
                            }
                        }
                        
                        // Кнопка увеличения громкости
                        SButton {
                            width: 50
                            height: 50
                            text: "🔊"
                            style: "btn-outline-light"
                            font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
                            onClicked: {
                                var newVolume = Math.min(1.0, mediaPlayer.volume + 0.1)
                                mediaPlayer.volume = newVolume
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Кнопка закрытия
    SButton {
        anchors {
            top: parent.top
            right: parent.right
            margins: 10
        }
        width: 80
        height: 60
        text: "✕"
        style: "btn-danger"
        font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
        z: 200  // Поверх всего
        onClicked: {
            mediaPlayer.stop()
            videoPlayerRoot.closeRequested()
        }
    }
    
    // Список видео (боковая панель)
    Rectangle {
        id: playlistPanel
        anchors {
            top: parent.top
            left: parent.left
            bottom: controlsArea.top
        }
        width: 300
        color: "#EE000000"
        visible: playlistButton.checked
        z: 150  // Поверх видео
        
        ListView {
            anchors.fill: parent
            anchors.margins: 10
            model: videoFiles
            spacing: 5
            clip: true
            
            delegate: Rectangle {
                width: parent.width - 20
                height: 50
                color: index === currentVideoIndex ? "#4CAF50" : "#424242"
                radius: 5
                border.color: "white"
                border.width: 1
                
                Text {
                    anchors.fill: parent
                    anchors.margins: 5
                    text: modelData
                    color: "white"
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
                
                MouseArea {
                    anchors.fill: parent
                    onClicked: playVideo(index)
                }
            }
        }
    }
    
    // Кнопка переключения плейлиста
    SButton {
        id: playlistButton
        anchors {
            top: parent.top
            left: parent.left
            margins: 10
        }
        width: 80
        height: 60
        text: "☰"
        style: "btn-outline-light"
        font.family: "Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Symbola, DejaVu Sans, sans-serif"
        checkable: true
        checked: false
        z: 200  // Поверх всего
    }
}

