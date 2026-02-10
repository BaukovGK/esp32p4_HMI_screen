#include "lang.h"
#include <stddef.h>

static lang_id_t s_current_lang = LANG_RU;

static const char **s_tables[LANG_COUNT] = {
    [LANG_EN] = NULL,   /* set in lang_init */
    [LANG_RU] = NULL,
};

void lang_init(lang_id_t default_lang)
{
    s_tables[LANG_EN] = lang_en_strings;
    s_tables[LANG_RU] = lang_ru_strings;
    s_current_lang = default_lang;
}

void lang_set(lang_id_t lang)
{
    if (lang < LANG_COUNT) {
        s_current_lang = lang;
    }
}

lang_id_t lang_get(void)
{
    return s_current_lang;
}

const char *lang_str(str_id_t id)
{
    if (id >= STR_COUNT) return "???";
    const char *s = s_tables[s_current_lang][id];
    return s ? s : "???";
}
