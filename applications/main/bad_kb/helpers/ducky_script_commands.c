#include "../bad_kb_app_i.h"
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_speaker.h>
#include "ble_hid.h"
#include "ducky_script.h"
#include "ducky_script_i.h"

#define TAG "BadKb"

typedef int32_t (*DuckyCmdCallback)(BadKbScript* bad_kb, const char* line, int32_t param);

typedef struct {
    char* name;
    DuckyCmdCallback callback;
    int32_t param;
} DuckyCmd;

// Helper function
// Sends a keypress over USB HID or BT HID depending on the script settings
// Returns false if send fails, true otherwise
static bool ducky_send_over_current(BadKbScript* bad_kb, uint16_t button) {
    bool result = true;
    if(bad_kb->bt) {
        result = result && ble_profile_hid_kb_press(bad_kb->app->ble_hid, button);
        furi_delay_ms(bt_timeout);
        result = result && ble_profile_hid_kb_release(bad_kb->app->ble_hid, button);
    } else {
        result = result && furi_hal_hid_kb_press(button);
        result = result && furi_hal_hid_kb_release(button);
    }
    return result;
}

static int32_t ducky_fnc_delay(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint32_t delay_val = 0;
    bool state = ducky_get_number(line, &delay_val);
    if((state) && (delay_val > 0)) {
        return (int32_t)delay_val;
    }

    return ducky_error(bad_kb, "Invalid number %s", line);
}

static int32_t ducky_fnc_defdelay(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_kb->defdelay);
    if(!state) {
        return ducky_error(bad_kb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_strdelay(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_kb->stringdelay);
    if(!state) {
        return ducky_error(bad_kb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_defstrdelay(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_kb->defstringdelay);
    if(!state) {
        return ducky_error(bad_kb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_string(BadKbScript* bad_kb, const char* line, int32_t param) {
    line = &line[ducky_get_command_len(line) + 1];
    furi_string_set_str(bad_kb->string_print, line);
    if(param == 1) {
        furi_string_cat(bad_kb->string_print, "\n");
    }

    if(bad_kb->stringdelay == 0 &&
       bad_kb->defstringdelay == 0) { // stringdelay not set - run command immediately
        bool state = ducky_string(bad_kb, furi_string_get_cstr(bad_kb->string_print));
        if(!state) {
            return ducky_error(bad_kb, "Invalid string %s", line);
        }
    } else { // stringdelay is set - run command in thread to keep handling external events
        return SCRIPT_STATE_STRING_START;
    }

    return 0;
}

static int32_t ducky_fnc_repeat(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    bool state = ducky_get_number(line, &bad_kb->repeat_cnt);
    if((!state) || (bad_kb->repeat_cnt == 0)) {
        return ducky_error(bad_kb, "Invalid number %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_sysrq(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_kb, line, true);
    if(bad_kb->bt) {
        ble_profile_hid_kb_press(
            bad_kb->app->ble_hid, KEY_MOD_LEFT_ALT | HID_KEYBOARD_PRINT_SCREEN);
        ble_profile_hid_kb_press(bad_kb->app->ble_hid, key);
        furi_delay_ms(bt_timeout);
        ble_profile_hid_kb_release_all(bad_kb->app->ble_hid);
    } else {
        furi_hal_hid_kb_press(KEY_MOD_LEFT_ALT | HID_KEYBOARD_PRINT_SCREEN);
        furi_hal_hid_kb_press(key);
        furi_hal_hid_kb_release_all();
    }
    return 0;
}

static int32_t ducky_fnc_altchar(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    ducky_numlock_on(bad_kb);
    bool state = ducky_altchar(bad_kb, line);
    if(!state) {
        return ducky_error(bad_kb, "Invalid altchar %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_altstring(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    ducky_numlock_on(bad_kb);
    bool state = ducky_altstring(bad_kb, line);
    if(!state) {
        return ducky_error(bad_kb, "Invalid altstring %s", line);
    }
    return 0;
}

static int32_t ducky_fnc_hold(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_kb, line, true);
    if(key == HID_KEYBOARD_NONE) {
        return ducky_error(bad_kb, "No keycode defined for %s", line);
    }
    bad_kb->key_hold_nb++;
    if(bad_kb->key_hold_nb > (HID_KB_MAX_KEYS - 1)) {
        return ducky_error(bad_kb, "Too many keys are hold");
    }
    if(bad_kb->bt) {
        ble_profile_hid_kb_press(bad_kb->app->ble_hid, key);
    } else {
        furi_hal_hid_kb_press(key);
    }
    return 0;
}

static int32_t ducky_fnc_release(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_kb, line, true);
    if(key == HID_KEYBOARD_NONE) {
        return ducky_error(bad_kb, "No keycode defined for %s", line);
    }
    if(bad_kb->key_hold_nb == 0) {
        return ducky_error(bad_kb, "No keys are hold");
    }
    bad_kb->key_hold_nb--;
    if(bad_kb->bt) {
        ble_profile_hid_kb_release(bad_kb->app->ble_hid, key);
    } else {
        furi_hal_hid_kb_release(key);
    }
    return 0;
}

static int32_t ducky_fnc_media(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_media_keycode_by_name(line);
    if(key == HID_CONSUMER_UNASSIGNED) {
        return ducky_error(bad_kb, "No keycode defined for %s", line);
    }
    ducky_send_over_current(bad_kb, key);
    return 0;
}

static int32_t ducky_fnc_globe(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    line = &line[ducky_get_command_len(line) + 1];
    uint16_t key = ducky_get_keycode(bad_kb, line, true);
    if(key == HID_KEYBOARD_NONE) {
        return ducky_error(bad_kb, "No keycode defined for %s", line);
    }

    if(bad_kb->bt) {
        ble_profile_hid_consumer_key_press(bad_kb->app->ble_hid, HID_CONSUMER_FN_GLOBE);
        ble_profile_hid_kb_press(bad_kb->app->ble_hid, key);
        furi_delay_ms(bt_timeout);
        ble_profile_hid_kb_release(bad_kb->app->ble_hid, key);
        ble_profile_hid_consumer_key_release(bad_kb->app->ble_hid, HID_CONSUMER_FN_GLOBE);
    } else {
        furi_hal_hid_consumer_key_press(HID_CONSUMER_FN_GLOBE);
        furi_hal_hid_kb_press(key);
        furi_hal_hid_kb_release(key);
        furi_hal_hid_consumer_key_release(HID_CONSUMER_FN_GLOBE);
    }
    return 0;
}

static int32_t ducky_fnc_waitforbutton(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);
    UNUSED(bad_kb);
    UNUSED(line);

    return SCRIPT_STATE_WAIT_FOR_BTN;
}

#define SPKR_HANDLE_TIMEOUT 200
static int32_t ducky_fnc_beep(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    // Remove the command from the line buffer
    line = &line[ducky_get_command_len(line) + 1];

    uint32_t frequency = 0;
    uint32_t duration_ms = 0;

    if(!ducky_get_number(line, &frequency)) {
        return ducky_error(bad_kb, "Invalid beep frequency");
    }

    uint32_t freq_string_len = strcspn(line, " ");

    if(freq_string_len == strlen(line)) {
        return ducky_error(bad_kb, "Malformed beep command");
    }

    // Remove the frequency from the line buffer
    line = &line[strcspn(line, " ")];

    if(!ducky_get_number(line, &duration_ms)) {
        return ducky_error(bad_kb, "Invalid beep duration");
    }

    if(!furi_hal_speaker_acquire(SPKR_HANDLE_TIMEOUT)) {
        // There's no good reason anything should be using the speaker at this point
        // But we also don't want to stop the script because of it
        FURI_LOG_E(TAG, "Failed to acquire speaker handle");
        return 0;
    }
    furi_hal_speaker_start(frequency, bad_kb->speaker_volume);
    furi_delay_ms(duration_ms);
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
    return 0;
}

static int32_t ducky_fnc_setcaps(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    // It seems the internal led state isn't updated often enough
    // So we just keep our own
    if(bad_kb->led_state == LEDS_NOT_UPDATED)
        bad_kb->led_state = furi_hal_hid_get_led_state();

    line = &line[ducky_get_command_len(line) + 1];

    if(strncmp(line, VALUE_ON, strlen(line)) == 0) {
        if(!(bad_kb->led_state & HID_KB_LED_CAPS)) {
            ducky_send_over_current(bad_kb, HID_KEYBOARD_CAPS_LOCK);
            // Set the capslock bit
            bad_kb->led_state |= HID_KB_LED_CAPS;
        }
    } else if(strncmp(line, VALUE_OFF, strlen(line)) == 0) {
        if(bad_kb->led_state & HID_KB_LED_CAPS) {
            ducky_send_over_current(bad_kb, HID_KEYBOARD_CAPS_LOCK);
            // Clear the capslock bit
            bad_kb->led_state &= (~HID_KB_LED_CAPS);
        }
    } else {
        return ducky_error(bad_kb, "Cannot set caps to value \"%s\"", line);
    }
    return 0;
}

static int32_t ducky_fnc_setvolume(BadKbScript* bad_kb, const char* line, int32_t param) {
    UNUSED(param);

    // Remove the command from the line buffer
    line = &line[ducky_get_command_len(line) + 1];

    float new_volume = 0.0f;
    char* strtof_end = NULL;

    new_volume = strtof(line, &strtof_end);

    if(strtof_end == line || new_volume > 1.0f || new_volume < 0.0f) {
        return ducky_error(bad_kb, "Invalid volume value");
    }

    bad_kb->speaker_volume = new_volume;
    return 0;
}

#define LETTER_COUNT 26
static int32_t ducky_fnc_random(BadKbScript* bad_kb, const char* line, int32_t param) {
    static const char* SPECIALS = "!@#$%^&*()-_.";
    UNUSED(bad_kb);
    UNUSED(line);

    uint8_t random_val = furi_hal_random_get();
    char char_to_write = '\0';

    switch(param) {
        case RandLetter:
            // Flip a coin to decide if it's upper or lowercase (lol)
            const char start = furi_hal_random_get() % 2 == 0 ? 'a' : 'A';
            char_to_write = start + random_val % LETTER_COUNT;
            break;

        case RandLetterLower:
            char_to_write = 'a' + random_val % LETTER_COUNT;
            break;

        case RandLetterUpper:
            char_to_write = 'A' + random_val % LETTER_COUNT;
            break;

        case RandDigit:
            char_to_write = '0' + random_val % 10;
            break;

        case RandSpecial:
            char_to_write = SPECIALS[random_val % strlen(SPECIALS)];
            break;

        case RandAnyChar:
            switch (furi_hal_random_get() % 3) {
            case 0:
                const char start = furi_hal_random_get() % 2 == 0 ? 'a' : 'A';
                char_to_write = start + random_val % LETTER_COUNT;
                break;

            case 1:
                char_to_write = '0' + random_val % 10;
                break;

            case 2:
                char_to_write = SPECIALS[random_val % strlen(SPECIALS)];
                break;
            }
            break;

        default:
            // Shouldn't ever happen, if it does, it's a logic error
            furi_crash("Invalid parameter passed to ducky_fnc_random");
            return 0;
    }
    const uint16_t keycode = BADKB_ASCII_TO_KEY(bad_kb, char_to_write);
    ducky_send_over_current(bad_kb, keycode);
    return 0;
}

static const DuckyCmd ducky_commands[] = {
    {"REM", NULL, -1},
    {"ID", NULL, -1},
    {"BT_ID", NULL, -1},
    {"DELAY", ducky_fnc_delay, -1},
    {"STRING", ducky_fnc_string, 0},
    {"STRINGLN", ducky_fnc_string, 1},
    {"DEFAULT_DELAY", ducky_fnc_defdelay, -1},
    {"DEFAULTDELAY", ducky_fnc_defdelay, -1},
    {"STRINGDELAY", ducky_fnc_strdelay, -1},
    {"STRING_DELAY", ducky_fnc_strdelay, -1},
    {"DEFAULT_STRING_DELAY", ducky_fnc_defstrdelay, -1},
    {"DEFAULTSTRINGDELAY", ducky_fnc_defstrdelay, -1},
    {"REPEAT", ducky_fnc_repeat, -1},
    {"SYSRQ", ducky_fnc_sysrq, -1},
    {"ALTCHAR", ducky_fnc_altchar, -1},
    {"ALTSTRING", ducky_fnc_altstring, -1},
    {"ALTCODE", ducky_fnc_altstring, -1},
    {"HOLD", ducky_fnc_hold, -1},
    {"RELEASE", ducky_fnc_release, -1},
    {"WAIT_FOR_BUTTON_PRESS", ducky_fnc_waitforbutton, -1},
    {"MEDIA", ducky_fnc_media, -1},
    {"GLOBE", ducky_fnc_globe, -1},
    {"BEEP", ducky_fnc_beep, -1},
    {"VOLUME", ducky_fnc_setvolume, -1},
    {"CAPS", ducky_fnc_setcaps, -1},
    {"RANDOM_LETTER", ducky_fnc_random, RandLetter},
    {"RANDOM_UPPERCASE_LETTER", ducky_fnc_random, RandLetterUpper},
    {"RANDOM_LOWERCASE_LETTER", ducky_fnc_random, RandLetterLower},
    {"RANDOM_NUMBER", ducky_fnc_random, RandDigit},
    {"RANDOM_SPECIAL", ducky_fnc_random, RandSpecial},
    {"RANDOM_CHAR", ducky_fnc_random, RandAnyChar},
};

#define WORKER_TAG TAG "Worker"

int32_t ducky_execute_cmd(BadKbScript* bad_kb, const char* line) {
    size_t cmd_word_len = strcspn(line, " ");
    for(size_t i = 0; i < COUNT_OF(ducky_commands); i++) {
        size_t cmd_compare_len = strlen(ducky_commands[i].name);

        if(cmd_compare_len != cmd_word_len) {
            continue;
        }

        if(strncmp(line, ducky_commands[i].name, cmd_compare_len) == 0) {
            if(ducky_commands[i].callback == NULL) {
                return 0;
            } else {
                return ((ducky_commands[i].callback)(bad_kb, line, ducky_commands[i].param));
            }
        }
    }

    return SCRIPT_STATE_CMD_UNKNOWN;
}
