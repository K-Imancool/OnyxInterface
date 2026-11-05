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
//inline constexpr const char *kOnyxAppVersion = "1.18"; // 30.10.26 грязный жест после ACT держится ~1 с, новый Begin при удержании не кликает
//inline constexpr const char *kOnyxAppVersion = "1.19"; // 30.10.26 не глотать дубликаты TouchBegin — из‑за них UI не получал касания
//inline constexpr const char *kOnyxAppVersion = "1.20"; // 30.10.26 первое касание после паузы — ориентир; EMI держит его без блокировки после ACT
//inline constexpr const char *kOnyxAppVersion = "1.21"; // 30.10.26 скачок при удержании не двигает точку; после ACT внимание до минуты или пока ложные касания не стихнут
//inline constexpr const char *kOnyxAppVersion = "1.22"; // 02.11.26 внимание после ACT не гаснет раньше минуты и только если ложных касаний нет
//inline constexpr const char *kOnyxAppVersion = "1.23"; // 02.11.26 цепочка EMI не подтверждается карантином; обрыв при живом втором слоте склеивается
//inline constexpr const char *kOnyxAppVersion = "1.24"; // 02.11.26 пока склейка держит палец, новый Begin не становится кликом
//inline constexpr const char *kOnyxAppVersion = "1.25"; // 02.11.26 точка при прыжках остаётся нажатой и отпускается через ~400 мс после последнего кадра
//inline constexpr const char *kOnyxAppVersion = "1.26"; // 02.11.26 залипшая точка отпускается после живого слота, призрак не жмёт меню под проверкой
//inline constexpr const char *kOnyxAppVersion = "1.27"; // 02.11.26 первая точка не берётся из чужого слота, кружок не зависает на старом месте
//inline constexpr const char *kOnyxAppVersion = "1.28"; // 02.11.26 touch-trace.log очищается при входе в проверку тача
//inline constexpr const char *kOnyxAppVersion = "1.29"; // 02.11.26 в проверке тача отброшенные точки рисуются бледным кругом
//inline constexpr const char *kOnyxAppVersion = "1.30"; // 02.11.26 лог тача: время, точка, ΔX ΔY Δt, затем статус
//inline constexpr const char *kOnyxAppVersion = "1.31"; // 03.11.26 эксперимент: склейка STITCH выключена
//inline constexpr const char *kOnyxAppVersion = "1.32"; // 03.11.26 после End в touch-trace.log пустая строка
//inline constexpr const char *kOnyxAppVersion = "1.33"; // 03.11.26 на внимании End ждёт 100 мс, широкий Move и ложный Begin не проходят
//inline constexpr const char *kOnyxAppVersion = "1.34"; // 03.11.26 на внимании второй далёкий тап за 100 мс не кликает
//inline constexpr const char *kOnyxAppVersion = "1.35"; // 03.11.26 на внимании чужой End и далёкий Begin палец не отпускают
//inline constexpr const char *kOnyxAppVersion = "1.36"; // 03.11.26 кружок проверки тача гаснет, когда нажатия уже нет
inline constexpr const char *kOnyxAppVersion = "1.37"; // 03.11.26 на внимании два кадра на месте становятся пальцем
