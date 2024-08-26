#include "desktop_keybinds.h"
#include "desktop_keybinds_filename.h"
#include "desktop_i.h"

#include <applications/main/archive/helpers/archive_helpers_ext.h>
#include <flipper_format/flipper_format.h>
#include <storage/storage.h>
#include <saved_struct.h>
#include <input/input.h>

#define TAG "DesktopKeybinds"

#define OLD_DESKTOP_KEYBINDS_VER   (1)
#define OLD_DESKTOP_KEYBINDS_MAGIC (0x14)
#define OLD_DESKTOP_KEYBINDS_PATH  DESKTOP_KEYBINDS_PATH_MIGRATE
#define OLD_MAX_KEYBIND_LENGTH     (64)

typedef struct {
    char data[OLD_MAX_KEYBIND_LENGTH];
} OldKeybind;

typedef OldKeybind OldKeybinds[DesktopKeybindTypeMAX][DesktopKeybindKeyMAX];

void desktop_keybinds_migrate(Desktop* desktop) {
    if(!storage_common_exists(desktop->storage, DESKTOP_KEYBINDS_PATH)) {
        OldKeybinds old;
        const bool success = saved_struct_load(
            OLD_DESKTOP_KEYBINDS_PATH,
            &old,
            sizeof(old),
            OLD_DESKTOP_KEYBINDS_MAGIC,
            OLD_DESKTOP_KEYBINDS_VER);

        if(success) {
            DesktopKeybinds new;
            for(DesktopKeybindType type = 0; type < DesktopKeybindTypeMAX; type++) {
                for(DesktopKeybindKey key = 0; key < DesktopKeybindKeyMAX; key++) {
                    FuriString* keybind = furi_string_alloc_set(old[type][key].data);
                    if(furi_string_empty(keybind)) {
                        furi_string_set_str(keybind, "_");
                    } else if(furi_string_equal(keybind, EXT_PATH("apps/Misc/nightstand.fap"))) {
                        furi_string_set(keybind, "Clock");
                    } else if(furi_string_equal(keybind, "RFID")) {
                        furi_string_set(keybind, "125 kHz RFID");
                    } else if(furi_string_equal(keybind, "SubGHz")) {
                        furi_string_set(keybind, "Sub-GHz");
                    } else if(furi_string_equal(keybind, "Xtreme")) {
                        furi_string_set(keybind, "Momentum");
                    }
                    new[type][key] = keybind;
                }
            }
            desktop_keybinds_save(desktop, &new);
            desktop_keybinds_free(&new);
        }
    }

    storage_common_remove(desktop->storage, OLD_DESKTOP_KEYBINDS_PATH);
}

const char* desktop_keybinds_defaults[DesktopKeybindTypeMAX][DesktopKeybindKeyMAX] = {
    [DesktopKeybindTypePress] =
        {
            [DesktopKeybindKeyUp] = "Lock Menu",
            [DesktopKeybindKeyDown] = "Archive",
            [DesktopKeybindKeyRight] = "Passport",
            [DesktopKeybindKeyLeft] = "Clock",
        },
    [DesktopKeybindTypeHold] =
        {
            [DesktopKeybindKeyUp] = "_",
            [DesktopKeybindKeyDown] = "_",
            [DesktopKeybindKeyRight] = "Device Info",
            [DesktopKeybindKeyLeft] = "Lock with PIN",
        },
};

const char* desktop_keybind_types[DesktopKeybindTypeMAX] = {
    [DesktopKeybindTypePress] = "Press",
    [DesktopKeybindTypeHold] = "Hold",
};

const char* desktop_keybind_keys[DesktopKeybindKeyMAX] = {
    [DesktopKeybindKeyUp] = "Up",
    [DesktopKeybindKeyDown] = "Down",
    [DesktopKeybindKeyRight] = "Right",
    [DesktopKeybindKeyLeft] = "Left",
};

static FuriString*
    desktop_keybinds_load_one(Desktop* desktop, DesktopKeybindType type, DesktopKeybindKey key) {
    bool success = false;
    FuriString* keybind = furi_string_alloc();
    FlipperFormat* file = flipper_format_file_alloc(desktop->storage);

    if(flipper_format_file_open_existing(file, DESKTOP_KEYBINDS_PATH)) {
        FuriString* keybind_name = furi_string_alloc_printf(
            "%s%s", desktop_keybind_types[type], desktop_keybind_keys[key]);
        success = flipper_format_read_string(file, furi_string_get_cstr(keybind_name), keybind);
        furi_string_free(keybind_name);
    }

    flipper_format_free(file);
    if(!success) {
        FURI_LOG_W(TAG, "Failed to load file, using defaults");
        furi_string_set(keybind, desktop_keybinds_defaults[type][key]);
    }
    return keybind;
}

void desktop_keybinds_load(Desktop* desktop, DesktopKeybinds* keybinds) {
    for(DesktopKeybindType type = 0; type < DesktopKeybindTypeMAX; type++) {
        for(DesktopKeybindKey key = 0; key < DesktopKeybindKeyMAX; key++) {
            const char* default_keybind = desktop_keybinds_defaults[type][key];
            if((*keybinds)[type][key]) {
                furi_string_set((*keybinds)[type][key], default_keybind);
            } else {
                (*keybinds)[type][key] = furi_string_alloc_set(default_keybind);
            }
        }
    }

    FlipperFormat* file = flipper_format_file_alloc(desktop->storage);
    FuriString* keybind_name = furi_string_alloc();

    if(flipper_format_file_open_existing(file, DESKTOP_KEYBINDS_PATH)) {
        for(DesktopKeybindType type = 0; type < DesktopKeybindTypeMAX; type++) {
            for(DesktopKeybindKey key = 0; key < DesktopKeybindKeyMAX; key++) {
                furi_string_printf(
                    keybind_name, "%s%s", desktop_keybind_types[type], desktop_keybind_keys[key]);
                if(!flipper_format_read_string(
                       file, furi_string_get_cstr(keybind_name), (*keybinds)[type][key])) {
                    furi_string_set((*keybinds)[type][key], desktop_keybinds_defaults[type][key]);
                    goto fail;
                }
            }
        }
    } else {
    fail:
        FURI_LOG_W(TAG, "Failed to load file, using defaults");
    }

    furi_string_free(keybind_name);
    flipper_format_free(file);
}

void desktop_keybinds_save(Desktop* desktop, const DesktopKeybinds* keybinds) {
    FlipperFormat* file = flipper_format_file_alloc(desktop->storage);
    FuriString* keybind_name = furi_string_alloc();

    if(flipper_format_file_open_always(file, DESKTOP_KEYBINDS_PATH)) {
        for(DesktopKeybindType type = 0; type < DesktopKeybindTypeMAX; type++) {
            for(DesktopKeybindKey key = 0; key < DesktopKeybindKeyMAX; key++) {
                furi_string_printf(
                    keybind_name, "%s%s", desktop_keybind_types[type], desktop_keybind_keys[key]);
                if(!flipper_format_write_string_cstr(
                       file,
                       furi_string_get_cstr(keybind_name),
                       furi_string_get_cstr((*keybinds)[type][key]))) {
                    goto fail;
                }
            }
        }
    } else {
    fail:
        FURI_LOG_E(TAG, "Failed to save file");
    }

    furi_string_free(keybind_name);
    flipper_format_free(file);
}

void desktop_keybinds_free(DesktopKeybinds* keybinds) {
    for(DesktopKeybindType type = 0; type < DesktopKeybindTypeMAX; type++) {
        for(DesktopKeybindKey key = 0; key < DesktopKeybindKeyMAX; key++) {
            furi_string_free((*keybinds)[type][key]);
        }
    }
}

static const DesktopKeybindType keybind_types[] = {
    [InputTypeShort] = DesktopKeybindTypePress,
    [InputTypeLong] = DesktopKeybindTypeHold,
};

static const DesktopKeybindKey keybind_keys[] = {
    [InputKeyUp] = DesktopKeybindKeyUp,
    [InputKeyDown] = DesktopKeybindKeyDown,
    [InputKeyRight] = DesktopKeybindKeyRight,
    [InputKeyLeft] = DesktopKeybindKeyLeft,
};

void desktop_run_keybind(Desktop* desktop, InputType _type, InputKey _key) {
    if(_type != InputTypeShort && _type != InputTypeLong) return;
    if(_key != InputKeyUp && _key != InputKeyDown && _key != InputKeyRight && _key != InputKeyLeft)
        return;

    DesktopKeybindType type = keybind_types[_type];
    DesktopKeybindKey key = keybind_keys[_key];
    FuriString* keybind = desktop_keybinds_load_one(desktop, type, key);

    if(furi_string_equal(keybind, "_")) {
    } else if(furi_string_equal(keybind, "Apps Menu")) {
        loader_start_detached_with_gui_error(desktop->loader, LOADER_APPLICATIONS_NAME, NULL);
    } else if(furi_string_equal(keybind, "Archive")) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopMainEventOpenArchive);
    } else if(furi_string_equal(keybind, "Clock")) {
        loader_start_detached_with_gui_error(
            desktop->loader, EXT_PATH("apps/Tools/nightstand.fap"), "");
    } else if(furi_string_equal(keybind, "Device Info")) {
        loader_start_detached_with_gui_error(desktop->loader, "Power", "about_battery");
    } else if(furi_string_equal(keybind, "Lock Menu")) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopMainEventOpenLockMenu);
    } else if(furi_string_equal(keybind, "Lock Keypad")) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopMainEventLockKeypad);
    } else if(furi_string_equal(keybind, "Lock with PIN")) {
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopMainEventLockWithPin);
    } else if(furi_string_equal(keybind, "Wipe Device")) {
        loader_start_detached_with_gui_error(desktop->loader, "Storage", "wipe");
    } else {
        if(storage_common_exists(desktop->storage, furi_string_get_cstr(keybind))) {
            run_with_default_app(furi_string_get_cstr(keybind));
        } else {
            loader_start_detached_with_gui_error(
                desktop->loader, furi_string_get_cstr(keybind), NULL);
        }
    }

    furi_string_free(keybind);
}
