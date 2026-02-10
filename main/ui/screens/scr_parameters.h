#pragma once

#include "lvgl.h"
#include "plant_data.h"

lv_obj_t *scr_parameters_create(lv_obj_t *parent);
void      scr_parameters_update(lv_obj_t *container, const plant_data_t *data);
