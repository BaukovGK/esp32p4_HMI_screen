#include "alarm_ring.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static alarm_entry_t s_ring[ALARM_RING_SIZE];
static int s_head = 0;     /* next write position */
static int s_count = 0;    /* total entries in ring */
static SemaphoreHandle_t s_mutex = NULL;

void alarm_ring_init(void)
{
    memset(s_ring, 0, sizeof(s_ring));
    s_head = 0;
    s_count = 0;
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);
}

void alarm_ring_push(const alarm_entry_t *entry)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* If this is a deactivation, find and update existing entry */
    if (!entry->active) {
        for (int i = 0; i < s_count && i < ALARM_RING_SIZE; i++) {
            int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
            if (s_ring[idx].active && strcmp(s_ring[idx].code, entry->code) == 0) {
                s_ring[idx].active = false;
                break;
            }
        }
    }

    /* Always add to ring as history */
    s_ring[s_head] = *entry;
    s_head = (s_head + 1) % ALARM_RING_SIZE;
    if (s_count < ALARM_RING_SIZE) s_count++;

    xSemaphoreGive(s_mutex);
}

int alarm_ring_get_active(alarm_entry_t *out, int max_count)
{
    int found = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count && i < ALARM_RING_SIZE && found < max_count; i++) {
        int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
        if (s_ring[idx].active) {
            out[found++] = s_ring[idx];
        }
    }
    xSemaphoreGive(s_mutex);
    return found;
}

int alarm_ring_get_history(alarm_entry_t *out, int max_count)
{
    int copied = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count && i < ALARM_RING_SIZE && copied < max_count; i++) {
        int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
        out[copied++] = s_ring[idx];
    }
    xSemaphoreGive(s_mutex);
    return copied;
}

int alarm_ring_active_count(void)
{
    int count = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count && i < ALARM_RING_SIZE; i++) {
        int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
        if (s_ring[idx].active) count++;
    }
    xSemaphoreGive(s_mutex);
    return count;
}

alarm_category_t alarm_ring_worst_active(void)
{
    alarm_category_t worst = ALARM_CAT_INFO;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count && i < ALARM_RING_SIZE; i++) {
        int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
        if (s_ring[idx].active && s_ring[idx].cat > worst) {
            worst = s_ring[idx].cat;
        }
    }
    xSemaphoreGive(s_mutex);
    return worst;
}
