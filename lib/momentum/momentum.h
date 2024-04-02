#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_serial_types.h>
#include <toolbox/colors.h>
#include <gui/canvas.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOMENTUM_SETTINGS_PATH CFG_PATH("momentum_settings.txt")
#define ASSET_PACKS_PATH EXT_PATH("asset_packs")
#define MAINMENU_APPS_PATH CFG_PATH("mainmenu_apps.txt")
#define ASSET_PACKS_NAME_LEN 32

typedef enum {
    BatteryIconOff,
    BatteryIconBar,
    BatteryIconPercent,
    BatteryIconInvertedPercent,
    BatteryIconRetro3,
    BatteryIconRetro5,
    BatteryIconBarPercent,
    BatteryIconCount,
} BatteryIcon;

typedef enum {
    MenuStyleList,
    MenuStyleWii,
    MenuStyleDsi,
    MenuStylePs4,
    MenuStyleVertical,
    MenuStyleC64,
    MenuStyleCompact,
    MenuStyleMNTM,
    MenuStyleCount,
} MenuStyle;

typedef enum {
    SpiDefault, // cs on pa4
    SpiExtra, // cs on pc3
    SpiCount,
} SpiHandle;

typedef enum {
    VgmColorModeDefault,
    VgmColorModeCustom,
    VgmColorModeRainbow,
    VgmColorModeRgbBacklight,
    VgmColorModeCount,
} VgmColorMode;

typedef struct {
    char asset_pack[ASSET_PACKS_NAME_LEN];
    uint32_t anim_speed;
    int32_t cycle_anims;
    bool unlock_anims;
    MenuStyle menu_style;
    bool lock_on_boot;
    bool bad_pins_format;
    bool allow_locked_rpc_commands;
    bool lockscreen_poweroff;
    bool lockscreen_time;
    bool lockscreen_seconds;
    bool lockscreen_date;
    bool lockscreen_statusbar;
    bool lockscreen_prompt;
    bool lockscreen_transparent;
    BatteryIcon battery_icon;
    bool statusbar_clock;
    bool status_icons;
    bool bar_borders;
    bool bar_background;
    bool sort_dirs_first;
    bool show_hidden_files;
    bool show_internal_tab;
    uint32_t favorite_timeout;
    bool dark_mode;
    bool rgb_backlight;
    uint32_t butthurt_timer;
    uint32_t charge_cap;
    SpiHandle spi_cc1101_handle;
    SpiHandle spi_nrf24_handle;
    FuriHalSerialId uart_esp_channel;
    FuriHalSerialId uart_nmea_channel;
    bool file_naming_prefix_after;
    VgmColorMode vgm_color_mode;
    Rgb565Color vgm_color_fg;
    Rgb565Color vgm_color_bg;
} MomentumSettings;

typedef struct {
    uint8_t* fonts[FontTotalNumber];
    CanvasFontParameters* font_params[FontTotalNumber];
} AssetPacks;

void momentum_settings_load(void);
void momentum_settings_save(void);
extern MomentumSettings momentum_settings;

void asset_packs_init(void);
void asset_packs_free(void);
extern AssetPacks asset_packs;

#ifdef __cplusplus
}
#endif
