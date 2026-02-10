#pragma once

#include "plant_data.h"

#define ALARM_RING_SIZE 16

void             alarm_ring_init(void);
void             alarm_ring_push(const alarm_entry_t *entry);
int              alarm_ring_get_active(alarm_entry_t *out, int max_count);
int              alarm_ring_get_history(alarm_entry_t *out, int max_count);
int              alarm_ring_active_count(void);
alarm_category_t alarm_ring_worst_active(void);
