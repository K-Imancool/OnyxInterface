# Видеопроигрыватель

## Описание
В главное меню добавлен видеопроигрыватель с полным набором элементов управления.

## Расположение видеофайлов
Видеопроигрыватель автоматически сканирует папку:
```
~/FOTEK/Video
```

## Поддерживаемые форматы
- MP4
- AVI
- MKV
- MOV
- WMV
- FLV
- WebM
- M4V
- MPEG/MPG

## Как использовать

### 1. Подготовка видеофайлов
Скопируйте видеофайлы в папку `~/FOTEK/Video`:
```bash
mkdir -p ~/FOTEK/Video
cp /path/to/your/video.mp4 ~/FOTEK/Video/
```

### 2. Запуск проигрывателя
1. Откройте главное меню приложения
2. Нажмите кнопку **"Видео"**
3. Видеопроигрыватель автоматически найдёт все видео в папке

## Элементы управления

### Основные кнопки
- **▶ / ⏸** - Воспроизведение/Пауза
- **⏹** - Остановка
- **⏮** - Предыдущее видео
- **⏭** - Следующее видео
- **✕** - Закрыть проигрыватель

### Дополнительные функции
- **☰** - Показать/скрыть список видео (плейлист)
- **Прогресс-бар** - Перемотка видео (тянуть ползунок)
- **Клик по видео** - Пауза/воспроизведение
- **Клик по видео в списке** - Переключение на выбранное видео

## Автоматическое воспроизведение
- При открытии проигрыватель автоматически начнёт воспроизводить первое видео из списка
- После окончания видео автоматически начнётся следующее

## Технические детали

### Компоненты
- `GUI/VideoPlayer.qml` - Основной компонент проигрывателя
- `GUI/MainMenu.qml` - Главное меню с кнопкой запуска
- `BackEnd/gstreamervideoplayer.cpp` - Воспроизведение и сканирование папки с видео

### C++ API
```cpp
Q_INVOKABLE QStringList scanVideoFiles(const QString &folderPath) const;
```
Метод автоматически сканирует папку и возвращает список видеофайлов.

## Устранение неполадок

### Ошибка "Internal data stream error"

Эта ошибка означает проблемы с кодеками или форматом видео.

**Решение:**

1. **Установите необходимые GStreamer плагины:**
```bash
sudo apt-get update
sudo apt-get install gstreamer1.0-plugins-base gstreamer1.0-plugins-good
sudo apt-get install gstreamer1.0-plugins-bad gstreamer1.0-libav
sudo apt-get install libqt5multimedia5 libqt5multimedia5-plugins
```

2. **Проверьте поддержку видео:**
```bash
cd /home/kikorik/QtFolder/projects/UserInterface
chmod +x scripts/check_video_support.sh
./scripts/check_video_support.sh
```

3. **Создайте тестовое видео:**
```bash
chmod +x scripts/create_test_video.sh
./scripts/create_test_video.sh
```

4. **Используйте совместимый формат:**
   - Лучше всего: **MP4 с кодеком H.264**
   - Конвертация существующего видео:
     ```bash
     ffmpeg -i input.avi -c:v libx264 -preset fast -crf 23 -c:a aac -b:a 128k output.mp4
     ```

### Видео не воспроизводятся
1. Убедитесь, что видеофайлы находятся в `~/FOTEK/Video`
2. Проверьте формат видео (должен быть в списке поддерживаемых)
3. Убедитесь, что установлены необходимые кодеки (см. выше)
4. Проверьте права доступа: `chmod 644 ~/FOTEK/Video/*.mp4`

### Пустой список видео
Проверьте консоль на наличие сообщения:
```
ProgHandle: Found N video files in /home/user/FOTEK/Video
```
Если N = 0, видеофайлы не найдены.

### Отладочная информация
При открытии видеопроигрывателя в консоли будут сообщения:
```
ProgHandle: Found X video files in /path/to/folder
Attempting to play video: file:///path/to/video.mp4
MediaPlayer status: X
MediaPlayer playbackState: X
```

Используйте эту информацию для диагностики проблем.

### Скрипт генерации плейлиста (опционально)
Если нужно создать текстовый плейлист:
```bash
cd /home/kikorik/QtFolder/projects/UserInterface
./scripts/generate_playlist.sh
```

## Настройка

### Изменение папки с видео
В файле `GUI/VideoPlayer.qml` измените:
```qml
property string videoFolder: "/home/kikorik/FOTEK/Video"
```

### Изменение размера кнопок
В файле `GUI/VideoPlayer.qml` найдите кнопки и измените:
```qml
width: 80   // Ширина кнопки
height: 60  // Высота кнопки
```

