/**
 * @file nvs_worker.c
 * @brief Реализация фонового воркера для асинхронной записи в NVS.
 *
 * См. nvs_worker.h для архитектуры. Решает проблему Top-11 из CODE_REVIEW:
 * блокирующий flash-write (10..100 мс) переносится из UI-task в фоновую task.
 *
 * Замечание про portMAX_DELAY:
 *   xQueueReceive(s_queue, ..., portMAX_DELAY) внутри самого воркера —
 *   это нормально. Воркер ничего не делает, пока нет работы; портить ему
 *   wake-up таймаут смысла нет (правило "конечные таймауты" относится к
 *   задачам, которые держат разделяемые ресурсы; воркер таких ресурсов
 *   между нажатиями не держит).
 */
#ifndef LVGL_LIVE_PREVIEW

#include "nvs_worker.h"
#include "plant_data_internal.h"

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "nvs_worker";

#define NVS_WORKER_QUEUE_LEN     8                       /* серия "Применить" */
#define NVS_WORKER_STACK_BYTES   4096
#define NVS_WORKER_PRIORITY      (tskIDLE_PRIORITY + 1)  /* низкий */
#define NVS_WORKER_ENQUEUE_MS    50                       /* timeout xQueueSend */

static QueueHandle_t s_queue = NULL;
static TaskHandle_t  s_task  = NULL;

/**
 * Тело воркер-задачи: бесконечный цикл, разбирающий очередь.
 * Каждая итерация: ждём задачу, запоминаем стартовое время,
 * выполняем соответствующий *_blocking, логируем длительность.
 */
static void nvs_worker_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "worker task started (queue=%d, stack=%d, prio=%d)",
             NVS_WORKER_QUEUE_LEN, NVS_WORKER_STACK_BYTES, NVS_WORKER_PRIORITY);

    for (;;) {
        nvs_task_type_t type;
        /* portMAX_DELAY здесь корректно: это сама worker-task, она и должна спать. */
        if (xQueueReceive(s_queue, &type, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        const int64_t t0_us = esp_timer_get_time();
        const char *label = "?";

        switch (type) {
        case NVS_TASK_SAVE_PRESSURE:
            label = "pressure";
            plant_data_save_settings_pressure_blocking();
            break;
        case NVS_TASK_SAVE_DOSER:
            label = "doser";
            plant_data_save_settings_doser_blocking();
            break;
        case NVS_TASK_SAVE_WASHING:
            label = "washing";
            plant_data_save_settings_washing_blocking();
            break;
        case NVS_TASK_SAVE_TIMEOUTS:
            label = "timeouts";
            plant_data_save_settings_timeouts_blocking();
            break;
        default:
            ESP_LOGW(TAG, "unknown task type %d (dropped)", (int)type);
            continue;
        }

        const int64_t dt_ms = (esp_timer_get_time() - t0_us) / 1000;
        ESP_LOGI(TAG, "saved %s in %lld ms (pending=%u)",
                 label, dt_ms, (unsigned)uxQueueMessagesWaiting(s_queue));
    }
}

esp_err_t nvs_worker_init(void)
{
    if (s_task != NULL || s_queue != NULL) {
        ESP_LOGW(TAG, "already initialised");
        return ESP_ERR_INVALID_STATE;
    }

    s_queue = xQueueCreate(NVS_WORKER_QUEUE_LEN, sizeof(nvs_task_type_t));
    if (s_queue == NULL) {
        ESP_LOGE(TAG, "xQueueCreate failed");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreate(
        nvs_worker_task,
        "nvs_worker",
        NVS_WORKER_STACK_BYTES,
        NULL,
        NVS_WORKER_PRIORITY,
        &s_task);

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed");
        vQueueDelete(s_queue);
        s_queue = NULL;
        s_task  = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "init OK");
    return ESP_OK;
}

esp_err_t nvs_worker_enqueue(nvs_task_type_t type)
{
    if (type < 0 || type >= NVS_TASK_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_queue == NULL) {
        ESP_LOGW(TAG, "enqueue before init (type=%d)", (int)type);
        return ESP_ERR_INVALID_STATE;
    }

    /* UI не должен ждать — небольшой grace-period (50 мс), потом drop. */
    if (xQueueSend(s_queue, &type, pdMS_TO_TICKS(NVS_WORKER_ENQUEUE_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "queue full (type=%d) — task dropped", (int)type);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

uint32_t nvs_worker_pending_count(void)
{
    if (s_queue == NULL) return 0;
    return (uint32_t)uxQueueMessagesWaiting(s_queue);
}

#endif /* !LVGL_LIVE_PREVIEW */
