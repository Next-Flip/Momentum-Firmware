#pragma once

#include <furi_hal_serial_types.h>
#include <furi_hal_version.h>
#include <stdint.h>
#include <toolbox/colors.h>

#define MOMENTUM_SETTINGS_PATH INT_PATH(".momentum_settings.txt")

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
    MenuStyleCoverFlow,
    MenuStyleCount,
} MenuStyle;

typedef enum {
    SpiDefault, // CS on pa4
    SpiExtra, // CS on pc3
    SpiCount,
} SpiHandle;

typedef enum {
    ScreenColorModeDefault,
    ScreenColorModeCustom,
    ScreenColorModeRainbow,
    ScreenColorModeRgbBacklight,
    ScreenColorModeCount,
} ScreenColorMode;

typedef union __attribute__((packed)) {
    struct {
        ScreenColorMode mode;
        RgbColor rgb;
    };
    uint32_t value;
} ScreenFrameColor;

typedef enum {
    BrowserPathOff,
    BrowserPathCurrent,
    BrowserPathBrief,
    BrowserPathFull,
    BrowserPathModeCount,
} BrowserPathMode;

typedef struct {
    char asset_pack[ASSET_PACKS_NAME_LEN];
    uint32_t anim_speed;
    int32_t cycle_anims;
    bool unlock_anims;
    MenuStyle menu_style;
    bool lock_on_boot;
    bool bad_pins_format;
    bool allow_locked_rpc_usb;
    bool allow_locked_rpc_ble;
    bool lockscreen_poweroff;
    bool lockscreen_time;
    bool lockscreen_seconds;
    bool lockscreen_date;
    bool lockscreen_statusbar;
    bool lockscreen_prompt;
    bool lockscreen_transparent;
    BatteryIcon battery_icon;
    bool status_icons;
    bool bar_borders;
    bool bar_background;
    bool sort_dirs_first;
    bool show_hidden_files;
    bool show_internal_tab;
    BrowserPathMode browser_path_mode;
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
    FuriHalVersionColor spoof_color;
    ScreenFrameColor rpc_color_fg;
    ScreenFrameColor rpc_color_bg;
} MomentumSettings;

void momentum_settings_save(void);
extern MomentumSettings momentum_settings;
