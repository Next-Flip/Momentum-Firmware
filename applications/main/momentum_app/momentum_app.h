#pragma once

#include <gui/gui.h>
#include <storage/storage.h>
#include <desktop/desktop.h>
#include <dialogs/dialogs.h>
#include <expansion/expansion.h>
#include <notification/notification_app.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <power/power_service/power.h>

#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>

#include <momentum/asset_packs.h>
#include <loader/loader_menu.h>
#include <lib/subghz/subghz_setting.h>
#include <rgb_backlight.h>
#include <momentum/namespoof.h>
#include <dolphin/dolphin.h>
#include <dolphin/dolphin_i.h>
#include <dolphin/helpers/dolphin_state.h>
#include <momentum/settings.h>
#include <desktop/views/desktop_view_slideshow.h>

#include <applications.h>
#include <assets_icons.h>
#include <flipper_application/flipper_application.h>
#include <furi.h>
#include <gui/view.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/toolbox/value_index.h>
#include <m-array.h>
#include <toolbox/stream/file_stream.h>

#include "scenes/momentum_app_scene.h"

ARRAY_DEF(CharList, char*)

#define DOLPHIN_MAX_XP (DOLPHIN_LEVELS[DOLPHIN_LEVEL_COUNT - 1] + 1)

typedef struct {
    Gui* gui;
    Storage* storage;
    Desktop* desktop;
    Dolphin* dolphin;
    DialogsApp* dialogs;
    Expansion* expansion;
    NotificationApp* notification;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    VariableItemList* var_item_list;
    Submenu* submenu;
    TextInput* text_input;
    ByteInput* byte_input;
    NumberInput* number_input;
    Popup* popup;
    DialogEx* dialog_ex;

    CharList_t asset_pack_names;
    uint8_t asset_pack_index;
    CharList_t mainmenu_app_labels;
    CharList_t mainmenu_app_exes;
    uint8_t mainmenu_app_index;
    DesktopSettings desktop_settings;
    bool subghz_use_defaults;
    FrequencyList_t subghz_static_freqs;
    uint8_t subghz_static_index;
    FrequencyList_t subghz_hopper_freqs;
    uint8_t subghz_hopper_index;
    bool subghz_extend;
    bool subghz_bypass;
    RgbColor lcd_color;
    RgbColor vgm_color;
    char device_name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    uint32_t dolphin_xp;
    uint32_t dolphin_angry;
    FuriString* version_tag;

    bool save_mainmenu_apps;
    bool save_desktop;
    bool save_subghz_freqs;
    bool save_subghz;
    bool save_name;
    bool save_xp;
    bool save_angry;
    bool save_dolphin;
    bool save_backlight;
    bool save_settings;
    bool apply_pack;
    bool show_slideshow;
    bool require_reboot;
} MomentumApp;

typedef enum {
    MomentumAppViewVarItemList,
    MomentumAppViewSubmenu,
    MomentumAppViewTextInput,
    MomentumAppViewByteInput,
    MomentumAppViewNumberInput,
    MomentumAppViewPopup,
    MomentumAppViewDialogEx,
} MomentumAppView;

bool momentum_app_apply(MomentumApp* app);

void momentum_app_load_mainmenu_apps(MomentumApp* app);
void momentum_app_empty_mainmenu_apps(MomentumApp* app);
