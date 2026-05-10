/**
 * @file sim/stubs/esp_log.h
 * @brief Stub-заголовок для замены реального esp_log.h в SDL2-симуляторе.
 *
 * Реальный esp_log.h доступен только в ESP-IDF. На macOS/Linux мы
 * подменяем макросы ESP_LOG* на printf-варианты для отладки.
 */
#ifndef ESP_LOG_H_STUB
#define ESP_LOG_H_STUB

#include <stdio.h>

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stdout, "I %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)

typedef int esp_log_level_t;
#define ESP_LOG_NONE 0
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3

static inline void esp_log_level_set(const char *t, esp_log_level_t l) { (void)t; (void)l; }

#endif /* ESP_LOG_H_STUB */
