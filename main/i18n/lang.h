#pragma once

#include "lang_strings.h"

typedef enum {
    LANG_EN = 0,
    LANG_RU,
    LANG_COUNT,
} lang_id_t;

void        lang_init(lang_id_t default_lang);
void        lang_set(lang_id_t lang);
lang_id_t   lang_get(void);
const char *lang_str(str_id_t id);

/* String tables (defined in lang_en.c and lang_ru.c) */
extern const char *lang_en_strings[];
extern const char *lang_ru_strings[];
