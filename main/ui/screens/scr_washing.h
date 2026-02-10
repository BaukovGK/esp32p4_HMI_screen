#pragma once

#include "lvgl.h"
#include "plant_data.h"

lv_obj_t *scr_washing_create(lv_obj_t *parent);
void      scr_washing_update(lv_obj_t *container, const plant_data_t *data);
