#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAINMENU_APPS_PATH INT_PATH(".mainmenu_apps.txt")

typedef struct LoaderMenu LoaderMenu;

LoaderMenu* loader_menu_alloc(void (*closed_cb)(void*), void* context, bool settings_only);

void loader_menu_free(LoaderMenu* loader_menu);

#ifdef __cplusplus
}
#endif
