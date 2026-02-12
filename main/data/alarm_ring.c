/**
 * @file alarm_ring.c
 * @brief Реализация кольцевого буфера аварийных сообщений.
 *
 * Кольцевой буфер фиксированного размера ALARM_RING_SIZE (16 записей).
 * Структура данных:
 *   s_ring[ALARM_RING_SIZE] - массив записей alarm_entry_t
 *   s_head                  - индекс следующей позиции для записи (0..15)
 *   s_count                 - общее количество записей (0..ALARM_RING_SIZE)
 *
 * Механика кольцевого буфера:
 *   - Запись всегда идёт в позицию s_head, затем s_head = (s_head+1) % SIZE.
 *   - При s_count < SIZE счётчик инкрементируется; после заполнения
 *     новые записи перезаписывают самые старые (s_count остаётся = SIZE).
 *   - Чтение идёт от (s_head-1) назад — от новейшей к старейшей записи.
 *
 * Потокобезопасность: собственный мьютекс s_mutex, независимый от plant_data.
 * Все публичные функции захватывают мьютекс на время операции.
 *
 * Файл исключается из сборки LVGL-превью (#ifndef LVGL_LIVE_PREVIEW).
 */
#ifndef LVGL_LIVE_PREVIEW
#include "alarm_ring.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

// Кольцевой буфер записей аварий (статический массив фиксированного размера)
static alarm_entry_t s_ring[ALARM_RING_SIZE];

// Индекс следующей позиции для записи (голова буфера)
static int s_head = 0;

// Количество записей в буфере (0..ALARM_RING_SIZE, после заполнения не растёт)
static int s_count = 0;

// Мьютекс для потокобезопасного доступа (отдельный от plant_data)
static SemaphoreHandle_t s_mutex = NULL;

// Монотонный счётчик изменений (инкрементируется при каждом push)
static uint32_t s_generation = 0;

/**
 * Инициализация кольцевого буфера.
 * Обнуляет массив и указатели, создаёт мьютекс.
 * Вызывается один раз при старте приложения.
 */
void alarm_ring_init(void)
{
    memset(s_ring, 0, sizeof(s_ring));
    s_head = 0;
    s_count = 0;
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex); // фатальная ошибка если мьютекс не создан
}

void alarm_ring_clear(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(s_ring, 0, sizeof(s_ring));
    s_head = 0;
    s_count = 0;
    s_generation++;
    xSemaphoreGive(s_mutex);
}

/**
 * Добавить запись аварии в кольцевой буфер.
 *
 * Логика деактивации: если entry->active == false, перед добавлением
 * производится поиск по буферу от новых к старым записям. Первая найденная
 * активная запись с совпадающим кодом (strcmp) деактивируется (active = false).
 * Это позволяет корректно снимать аварии при получении сообщения о деактивации.
 *
 * После деактивации (или если entry->active == true) запись добавляется
 * в буфер как элемент истории в позицию s_head.
 */
void alarm_ring_push(const alarm_entry_t *entry)
{
    if (!entry) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // Если пришла деактивация — найти и снять активную аварию с таким же кодом
    if (!entry->active) {
        for (int i = 0; i < s_count && i < ALARM_RING_SIZE; i++) {
            // Обход от новейшей записи (s_head-1) к старейшей
            int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
            if (s_ring[idx].active && strcmp(s_ring[idx].code, entry->code) == 0) {
                s_ring[idx].active = false; // деактивируем найденную запись
                break; // снимаем только первую (самую новую) совпавшую
            }
        }
    }

    // Запись в текущую позицию головы буфера
    s_ring[s_head] = *entry;
    // Сдвиг головы вперёд с заворотом (кольцевая адресация)
    s_head = (s_head + 1) % ALARM_RING_SIZE;
    // Инкремент счётчика, пока буфер не заполнен
    if (s_count < ALARM_RING_SIZE) s_count++;
    // Инкремент счётчика поколений для детектирования изменений в UI
    s_generation++;

    xSemaphoreGive(s_mutex);
}

/**
 * Получить активные аварии (от новых к старым).
 * Обходит буфер от s_head-1 назад, копирует только записи с active == true.
 * @param out       массив для результатов
 * @param max_count максимальное количество результатов
 * @return количество скопированных активных аварий
 */
int alarm_ring_get_active(alarm_entry_t *out, int max_count)
{
    int found = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count && i < ALARM_RING_SIZE && found < max_count; i++) {
        // Индекс: от новейшей записи к старейшей
        int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
        if (s_ring[idx].active) {
            out[found++] = s_ring[idx]; // структурное копирование
        }
    }
    xSemaphoreGive(s_mutex);
    return found;
}

/**
 * Получить полную историю аварий (от новых к старым).
 * Копирует все записи, включая деактивированные.
 * @param out       массив для результатов
 * @param max_count максимальное количество результатов
 * @return количество скопированных записей
 */
int alarm_ring_get_history(alarm_entry_t *out, int max_count)
{
    int copied = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count && i < ALARM_RING_SIZE && copied < max_count; i++) {
        // Индекс: от новейшей записи к старейшей
        int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
        out[copied++] = s_ring[idx];
    }
    xSemaphoreGive(s_mutex);
    return copied;
}

/**
 * Подсчитать количество активных аварий.
 * Полный обход буфера с подсчётом записей с active == true.
 * @return количество активных аварий
 */
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

/**
 * Определить наихудшую категорию среди активных аварий.
 * Перечисление alarm_category_t упорядочено по возрастанию критичности:
 * INFO(0) < WARNING(1) < ALARM(2) < CRITICAL(3).
 * Функция возвращает максимальное значение cat среди активных записей.
 * @return наихудшая категория, или ALARM_CAT_INFO если активных аварий нет
 */
alarm_category_t alarm_ring_worst_active(void)
{
    alarm_category_t worst = ALARM_CAT_INFO; // по умолчанию — нет аварий
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count && i < ALARM_RING_SIZE; i++) {
        int idx = (s_head - 1 - i + ALARM_RING_SIZE) % ALARM_RING_SIZE;
        if (s_ring[idx].active && s_ring[idx].cat > worst) {
            worst = s_ring[idx].cat; // обновляем наихудшую категорию
        }
    }
    xSemaphoreGive(s_mutex);
    return worst;
}
/**
 * Возвращает текущее значение счётчика поколений.
 * Значение монотонно растёт при каждом alarm_ring_push().
 * UI сравнивает с сохранённым значением для определения необходимости перестроения.
 */
uint32_t alarm_ring_generation(void)
{
    uint32_t gen;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    gen = s_generation;
    xSemaphoreGive(s_mutex);
    return gen;
}
#endif /* !LVGL_LIVE_PREVIEW */
