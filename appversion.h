#pragma once

// Единственное место версии ПО UserInterface (не путать с main.cpp субмодулей).
// Менять только здесь — коммит попадёт в репозиторий UserInterface.
//inline constexpr const char *kOnyxAppVersion = "1.4"; // 23.07.26 переход к FullSocketEditor
//inline constexpr const char *kOnyxAppVersion = "1.5"; // 21.08.26
//inline constexpr const char *kOnyxAppVersion = "1.6"; // 17.09.26 Правки визуала в основном
//inline constexpr const char *kOnyxAppVersion = "1.7"; // 21.09.26 Добавлено удаление видео в сервисное меню
//inline constexpr const char *kOnyxAppVersion = "1.8"; // 21.09.26 Обмен по uart поправлен
//inline constexpr const char *kOnyxAppVersion = "1.9"; // 23.09.26 Исправление обработки кнопок + и -

//inline constexpr const char *kOnyxAppVersion = "1.10"; // 24.09.26 Дифференциация ошибок связи и мелочи по дизайну
//inline constexpr const char *kOnyxAppVersion = "1.11"; // 24.09.26 Контроль тача
//inline constexpr const char *kOnyxAppVersion = "1.12"; // 09.10.26 Фильтрация прыжков тача
//inline constexpr const char *kOnyxAppVersion = "1.13"; // 09.10.26 убрана анимация активации
//inline constexpr const char *kOnyxAppVersion = "1.14"; // 20.10.26 улучшено восстановление тача после сбоя во время активации
//inline constexpr const char *kOnyxAppVersion = "1.15"; // 26.10.26 увеличена пауза между касаниями в разных местах по горизонтали (constexpr qint64 kPostReleaseGuardMs = 400; TouchUsbMonitor.cpp)
//inline constexpr const char *kOnyxAppVersion = "1.16"; // 29.10.26 проверка WINSTARTP Mouse при старте UI и сброс USB-порта, если мыши нет
//inline constexpr const char *kOnyxAppVersion = "1.17"; // 30.10.26 склейка ложного отпускания тача после активации, без запрета тапов «+»/«−»

//inline constexpr const char *kOnyxAppVersion = "1.18"; // 06.11.26 с изменённой цветовой гаммой
//inline constexpr const char *kOnyxAppVersion = "1.19"; // 09.11.26 WINSTARTP Mouse не остаётся глухим после ожидания отпускания
//inline constexpr const char *kOnyxAppVersion = "1.20"; // 09.11.26 фильтрация тача выключена, порт и призраки в проверке остаются
//inline constexpr const char *kOnyxAppVersion = "1.21"; // 17.11.26 Изменены уровни звука, коррекция по цветам
//inline constexpr const char *kOnyxAppVersion = "1.22"; // 19.11.26 E9: принять W.R.Inc. HID-Mouse как валидный вход тача
//inline constexpr const char *kOnyxAppVersion = "1.23"; // 19.11.26 удалён тестовый квадрат попыток восстановления тача
//inline constexpr const char *kOnyxAppVersion = "1.24"; // 20.11.26 перекомпоновка левой боковой панели, добавлена стрелка "НАЗАД" в основном интерфейсе
inline constexpr const char *kOnyxAppVersion = "1.25"; // 24.11.26 исправление цветов баллонов и одноклавишной педали
