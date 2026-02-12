/**
 * @file lang_strings.h
 * @brief Перечисление строковых идентификаторов (str_id_t) для модуля i18n.
 *
 * Каждое значение перечисления является индексом в массиве строк конкретного
 * языка (lang_en_strings[], lang_ru_strings[] и т.д.).
 *
 * Добавление новой строки:
 *   1. Добавить новый идентификатор в соответствующую группу ПЕРЕД STR_COUNT.
 *   2. Добавить перевод в каждый языковой файл (lang_ru.c, lang_en.c и т.д.)
 *      с использованием designated initializer: [STR_NEW_ID] = "...".
 *   3. Использовать lang_str(STR_NEW_ID) в UI-коде.
 *
 * Внимание: порядок значений определяет числовой индекс. Не менять порядок
 * существующих элементов -- это нарушит соответствие со строковыми таблицами.
 */
#pragma once

typedef enum {
    /* Названия режимов работы установки */
    STR_MODE_IDLE = 0,   // Ожидание
    STR_MODE_AUTO,       // Автоматический режим
    STR_MODE_WASHING,    // Промывка мембран
    STR_MODE_MANUAL,     // Ручной режим
    STR_MODE_FAULT,      // Аварийное состояние
    STR_MODE_UNKNOWN,    // Нет связи с контроллером

    /* Подсостояния автоматического режима */
    STR_AUTO_STARTING_PUMP1,  // Пуск насоса подачи исходной воды
    STR_AUTO_RAMP,            // Разгон (набор давления)
    STR_AUTO_STARTING_PUMP2,  // Пуск насоса 1-й ступени обратного осмоса
    STR_AUTO_FILLING_INTERM,  // Заполнение промежуточной ёмкости
    STR_AUTO_STARTING_PUMP3,  // Пуск насоса 2-й ступени обратного осмоса
    STR_AUTO_RUNNING,         // Штатная работа
    STR_AUTO_STOPPING,        // Останов установки

    /* Подсостояния режима промывки мембран */
    STR_WASH_WAIT_HEAT,    // Ожидание готовности нагревателя
    STR_WASH_HEATING,      // Нагрев промывочного раствора
    STR_WASH_WAIT_SUPPLY,  // Подготовка к подаче раствора
    STR_WASH_SUPPLY,       // Подача раствора через мембраны
    STR_WASH_WAIT_DRAIN,   // Подготовка к сливу
    STR_WASH_DRAIN,        // Слив промывочного раствора
    STR_WASH_DONE,         // Промывка завершена

    /* Навигация -- названия вкладок/экранов */
    STR_NAV_MNEMONIC,      // Мнемосхема (P&ID)
    STR_NAV_PARAMETERS,    // Параметры процесса
    STR_NAV_WASHING,       // Экран промывки
    STR_NAV_ALARMS,        // Журнал аварий
    STR_NAV_SETTINGS,      // Настройки
    STR_NAV_DIAGNOSTICS,   // Диагностика контроллера

    /* Кнопки управления */
    STR_BTN_START_AUTO,     // Пуск в автоматическом режиме
    STR_BTN_STOP,           // Стоп
    STR_BTN_MANUAL,         // Переход в ручной режим
    STR_BTN_START_WASHING,  // Запуск промывки
    STR_BTN_CONFIRM,        // Подтвердить действие
    STR_BTN_RESET_FAULT,    // Сброс аварии
    STR_BTN_APPLY,          // Применить настройки

    /* Метки оборудования на мнемосхеме */
    STR_LBL_FEED_PUMP,      // Насос подачи исходной воды
    STR_LBL_STAGE1_PUMP,    // Насос 1-й ступени ОО
    STR_LBL_STAGE2_PUMP,    // Насос 2-й ступени ОО
    STR_LBL_HEATER,         // Нагреватель промывочного раствора
    STR_LBL_DOSER,          // Дозатор антискаланта
    STR_LBL_SOURCE_TANK,    // Исходная ёмкость (бак сырой воды)
    STR_LBL_INTERM_TANK,    // Промежуточная ёмкость (между ступенями)
    STR_LBL_PERMEATE_TANK,  // Ёмкость пермеата (чистой воды)
    STR_LBL_MEMBRANE_1,     // Мембранный элемент 1
    STR_LBL_MEMBRANE_2,     // Мембранный элемент 2
    STR_LBL_FILTER,         // Механический фильтр

    /* Метки технологических параметров */
    STR_LBL_PRESSURE,       // Давление (бар)
    STR_LBL_TEMPERATURE,    // Температура (градусы Цельсия)
    STR_LBL_FLOW_RATE,      // Расход (м3/ч)
    STR_LBL_VOLUME,         // Объём (м3)
    STR_LBL_CONDUCTIVITY,   // Удельная электропроводность (мкСм/см)
    STR_LBL_RECOVERY,       // Степень извлечения (%)
    STR_LBL_SELECTIVITY,    // Селективность мембран (%)
    STR_LBL_FILTER_DP,      // Перепад давления на фильтре
    STR_LBL_STAGE1_FEED,    // Подача на 1-ю ступень

    /* Коды аварий */
    STR_ALARM_ESTOP,            // Аварийный стоп (кнопка E-Stop)
    STR_ALARM_SOURCE_EMPTY,     // Исходная ёмкость пуста
    STR_ALARM_INTERM_EMPTY,     // Промежуточная ёмкость пуста
    STR_ALARM_P1_HIGH,          // Высокое давление на датчике P1
    STR_ALARM_P3_HIGH,          // Высокое давление на датчике P3
    STR_ALARM_P4_HIGH,          // Высокое давление на датчике P4
    STR_ALARM_T_HIGH,           // Перегрев (превышение температуры)
    STR_ALARM_FILTER_DP,        // Засорение фильтра (высокий перепад)
    STR_ALARM_PUMP1_TIMEOUT,    // Таймаут подтверждения насоса подачи
    STR_ALARM_PUMP2_TIMEOUT,    // Таймаут подтверждения насоса 1-й ст.
    STR_ALARM_PUMP3_TIMEOUT,    // Таймаут подтверждения насоса 2-й ст.
    STR_ALARM_SENSOR_FAULT,     // Обрыв/неисправность датчика
    STR_ALARM_MQTT_DISCONNECT,  // Потеря связи MQTT
    STR_ALARM_MODBUS_OFFLINE,   // Контроллер Modbus не отвечает

    /* Разделы экрана настроек */
    STR_SET_PRESSURE,   // Уставки давления
    STR_SET_DOSER,      // Параметры дозатора
    STR_SET_WASHING,    // Параметры промывки
    STR_SET_TIMEOUTS,   // Таймауты и задержки
    STR_SET_MQTT,       // Настройки MQTT-соединения

    /* Диагностика контроллера */
    STR_DIAG_HEAP_FREE,    // Свободная память (heap)
    STR_DIAG_HEAP_MIN,     // Минимальная свободная память за сессию
    STR_DIAG_UPTIME,       // Время работы с момента включения
    STR_DIAG_MQTT_STATUS,  // Состояние MQTT-подключения
    STR_DIAG_MODBUS,       // Состояние шины Modbus

    /* Статусные строки */
    STR_STATUS_ONLINE,        // Устройство в сети
    STR_STATUS_OFFLINE,       // Устройство не в сети
    STR_STATUS_OK,            // Норма
    STR_STATUS_FAULT,         // Авария
    STR_STATUS_SENSOR_BREAK,  // Обрыв датчика
    STR_STATUS_NO_DATA,       // Нет данных от контроллера

    /* Диалог подтверждения действий */
    STR_CONFIRM_TITLE,       // Заголовок диалога
    STR_CONFIRM_START_AUTO,  // Текст: запустить авторежим?
    STR_CONFIRM_STOP,        // Текст: остановить установку?
    STR_CONFIRM_MANUAL,      // Текст: перейти в ручной режим?
    STR_CONFIRM_WASHING,     // Текст: запустить промывку?
    STR_BTN_YES,             // Кнопка "Да"
    STR_BTN_NO,              // Кнопка "Нет"

    /* Единицы измерения */
    STR_UNIT_BAR,      // бар (давление)
    STR_UNIT_CELSIUS,  // градусы Цельсия
    STR_UNIT_M3H,      // кубометры в час (расход)
    STR_UNIT_M3,       // кубометры (объём)
    STR_UNIT_USCM,     // микросименс на сантиметр (УЭП)
    STR_UNIT_PERCENT,  // проценты
    STR_UNIT_SECONDS,  // секунды
    STR_UNIT_MINUTES,  // минуты
    STR_UNIT_BYTES,    // байты (для диагностики памяти)

    STR_COUNT  // Общее количество строк (служебное значение)
} str_id_t;
