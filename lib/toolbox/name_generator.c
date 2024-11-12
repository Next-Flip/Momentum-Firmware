#include "name_generator.h"

#include <stdio.h>
#include <stdint.h>
#include <furi_hal_rtc.h>
#include <stdlib.h>
#include <stdbool.h>
#include <furi.h>
#include <momentum/momentum.h>

const char* const name_generator_left[] = {
    "super",  "big",   "little", "liquid",  "qq",       "cheeky", "thick",
    "sneaky", "silly", "quick",  "quantum", "kurwa",    "feral",  "smart",
    "yappy",  "ultra", "whois",  "random",  "looshite",
};

const char* const name_generator_right[] = {
    "bidet",   "sus",    "fed",    "moroder", "bobr",  "chomik", "sidorovich",
    "stalker", "yapper", "bnuuy",  "jezyk",   "juzyk", "cult",   "pp",
    "zalaz",   "breeky", "bunker", "pingwin", "kot",
};

void name_generator_make_auto_datetime(
    char* name,
    size_t max_name_size,
    const char* prefix,
    DateTime* custom_time) {
    if(!furi_hal_rtc_is_flag_set(FuriHalRtcFlagRandomFilename)) {
        name_generator_make_detailed_datetime(
            name, max_name_size, prefix, custom_time, momentum_settings.file_naming_prefix_after);
    } else {
        name_generator_make_random_prefixed(
            name, max_name_size, prefix, momentum_settings.file_naming_prefix_after);
    }
}

void name_generator_make_auto(char* name, size_t max_name_size, const char* prefix) {
    name_generator_make_auto_datetime(name, max_name_size, prefix, NULL);
}

void name_generator_make_auto_basic(char* name, size_t max_name_size, const char* prefix) {
    if(!furi_hal_rtc_is_flag_set(FuriHalRtcFlagRandomFilename)) {
        name_generator_make_detailed_datetime(
            name, max_name_size, prefix, NULL, momentum_settings.file_naming_prefix_after);
    } else {
        name_generator_make_random(name, max_name_size);
    }
}

void name_generator_make_random_prefixed(
    char* name,
    size_t max_name_size,
    const char* prefix,
    bool prefix_after) {
    furi_check(name);
    furi_check(max_name_size);

    uint8_t name_generator_left_i = rand() % COUNT_OF(name_generator_left);
    uint8_t name_generator_right_i = rand() % COUNT_OF(name_generator_right);

    if(prefix) {
        if(prefix_after) {
            snprintf(
                name,
                max_name_size,
                "%s-%s_%s",
                name_generator_left[name_generator_left_i],
                name_generator_right[name_generator_right_i],
                prefix);
        } else {
            snprintf(
                name,
                max_name_size,
                "%s_%s-%s",
                prefix,
                name_generator_left[name_generator_left_i],
                name_generator_right[name_generator_right_i]);
        }
    } else {
        snprintf(
            name,
            max_name_size,
            "%s-%s",
            name_generator_left[name_generator_left_i],
            name_generator_right[name_generator_right_i]);
    }

    // Set first symbol to upper case
    if(islower((int)name[0])) name[0] = name[0] - 0x20;
}

void name_generator_make_random(char* name, size_t max_name_size) {
    name_generator_make_random_prefixed(
        name, max_name_size, NULL, momentum_settings.file_naming_prefix_after);
}

void name_generator_make_detailed_datetime(
    char* name,
    size_t max_name_size,
    const char* prefix,
    DateTime* custom_time,
    bool prefix_after) {
    furi_check(name);
    furi_check(max_name_size);

    DateTime dateTime;
    if(custom_time) {
        dateTime = *custom_time;
    } else {
        furi_hal_rtc_get_datetime(&dateTime);
    }

    if(prefix) {
        if(prefix_after) {
            snprintf(
                name,
                max_name_size,
                "%.4d-%.2d-%.2d_%.2d,%.2d,%.2d_%s",
                dateTime.year,
                dateTime.month,
                dateTime.day,
                dateTime.hour,
                dateTime.minute,
                dateTime.second,
                prefix);
        } else {
            snprintf(
                name,
                max_name_size,
                "%s_%.4d-%.2d-%.2d_%.2d,%.2d,%.2d",
                prefix,
                dateTime.year,
                dateTime.month,
                dateTime.day,
                dateTime.hour,
                dateTime.minute,
                dateTime.second);
        }
    } else {
        snprintf(
            name,
            max_name_size,
            "%.4d-%.2d-%.2d_%.2d,%.2d,%.2d",
            dateTime.year,
            dateTime.month,
            dateTime.day,
            dateTime.hour,
            dateTime.minute,
            dateTime.second);
    }

    // Set first symbol to upper case
    if(islower((int)name[0])) name[0] = name[0] - 0x20;
}

void name_generator_make_detailed(char* name, size_t max_name_size, const char* prefix) {
    name_generator_make_detailed_datetime(
        name, max_name_size, prefix, NULL, momentum_settings.file_naming_prefix_after);
}
