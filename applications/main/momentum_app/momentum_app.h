#pragma once

#include <applications.h>
#include <assets_icons.h>
#include <dialogs/dialogs.h>
#include <dolphin/dolphin_i.h>
#include <dolphin/dolphin.h>
#include <dolphin/helpers/dolphin_state.h>
#include <expansion/expansion.h>
#include <flipper_application/flipper_application.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/toolbox/value_index.h>
#include <loader/loader_menu.h>
#include <m-array.h>
#include <momentum/asset_packs.h>
#include <momentum/namespoof.h>
#include <momentum/settings.h>
#include <notification/notification_app.h>
#include <power/power_service/power.h>
#include <rgb_backlight.h>
#include <scenes/momentum_app_scene.h>
#include <toolbox/stream/file_stream.h>

ARRAY_DEF(CharList, char*)

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    Expansion* expansion;
    NotificationApp* notification;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    VariableItemList* var_item_list;
    Submenu* submenu;
    TextInput* text_input;
    ByteInput* byte_input;
    Popup* popup;
    DialogEx* dialog_ex;

    CharList_t asset_pack_names;
    uint8_t asset_pack_index;
    CharList_t mainmenu_app_labels;
    CharList_t mainmenu_app_exes;
    uint8_t mainmenu_app_index;
    bool subghz_use_defaults;
    FrequencyList_t subghz_static_freqs;
    uint8_t subghz_static_index;
    FrequencyList_t subghz_hopper_freqs;
    uint8_t subghz_hopper_index;
    char subghz_freq_buffer[7];
    bool subghz_extend;
    bool subghz_bypass;
    RgbColor lcd_color;
    RgbColor vgm_color;
    char device_name[FURI_HAL_VERSION_ARRAY_NAME_LENGTH];
    int32_t dolphin_level;
    int32_t dolphin_angry;
    FuriString* version_tag;

    bool save_mainmenu_apps;
    bool save_subghz_freqs;
    bool save_subghz;
    bool save_name;
    bool save_level;
    bool save_angry;
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
    MomentumAppViewPopup,
    MomentumAppViewDialogEx,
} MomentumAppView;

bool momentum_app_apply(MomentumApp* app);
