/* AURELIA OS - built-in application registry */
#include <aurelia/app.h>
#include <aurelia/kstring.h>

extern const app_desc_t app_terminal;
extern const app_desc_t app_explorer;
extern const app_desc_t app_settings;
extern const app_desc_t app_notes;
extern const app_desc_t app_editor;
extern const app_desc_t app_calculator;
extern const app_desc_t app_monitor;
extern const app_desc_t app_clock;
extern const app_desc_t app_calendar;
extern const app_desc_t app_store;
extern const app_desc_t app_browser;
extern const app_desc_t app_about;
extern const app_desc_t app_logs;
extern const app_desc_t app_imageviewer;
extern const app_desc_t app_player;

static const app_desc_t *const g_apps[] = {
    &app_terminal,
    &app_explorer,
    &app_settings,
    &app_notes,
    &app_editor,
    &app_calculator,
    &app_monitor,
    &app_clock,
    &app_calendar,
    &app_store,
    &app_browser,
    &app_imageviewer,
    &app_player,
    &app_logs,
    &app_about,
};

u32 app_count(void) { return ARRAY_SIZE(g_apps); }

const app_desc_t *app_at(u32 index)
{
    return index < ARRAY_SIZE(g_apps) ? g_apps[index] : NULLPTR;
}

const app_desc_t *app_find(const char *id)
{
    if (!id) {
        return NULLPTR;
    }
    for (u32 i = 0; i < ARRAY_SIZE(g_apps); i++) {
        if (strcmp(g_apps[i]->id, id) == 0) {
            return g_apps[i];
        }
    }
    return NULLPTR;
}
