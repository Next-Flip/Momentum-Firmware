#include "../infrared_app_i.h"
#include "common/infrared_scene_universal_common.h"

#define TAG "InfraredUniversalBlurayDVD"

void infrared_scene_universal_bluray_on_enter(void* context) {
    InfraredApp* infrared = context;
    ButtonPanel* button_panel = infrared->button_panel;
    InfraredBruteForce* brute_force = infrared->brute_force;

    FURI_LOG_I(TAG, "Entering Universal Blu-ray/DVD scene");

    infrared_brute_force_set_db_filename(brute_force, EXT_PATH("infrared/assets/bluray_dvd.ir"));
    FURI_LOG_I(TAG, "Set database filename: %s", EXT_PATH("infrared/assets/bluray_dvd.ir"));

    button_panel_reserve(button_panel, 2, 4);
    uint32_t i = 0;

    // Power button
    button_panel_add_item(
        button_panel,
        i,
        0,
        0,
        6,
        13,
        &I_power_19x20,
        &I_power_hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 4, 35, &I_power_text_24x5);
    infrared_brute_force_add_record(brute_force, i++, "Power");

    // Eject button (using mute icon as a placeholder)
    button_panel_add_item(
        button_panel,
        i,
        1,
        0,
        39,
        13,
        &I_eject_19x20,
        &I_eject_hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 39, 35, &I_eject_text_19x5);
    infrared_brute_force_add_record(brute_force, i++, "Eject");

    // Play button
    button_panel_add_item(
        button_panel,
        i,
        0,
        1,
        6,
        42,
        &I_play_19x20,
        &I_play_hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 6, 64, &I_play_text_19x5);
    infrared_brute_force_add_record(brute_force, i++, "Play");

    // Pause button
    button_panel_add_item(
        button_panel,
        i,
        1,
        1,
        39,
        42,
        &I_pause_19x20,
        &I_pause_hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 37, 64, &I_pause_text_23x5);
    infrared_brute_force_add_record(brute_force, i++, "Pause");

    // Fast Backward
    button_panel_add_item(
        button_panel,
        i,
        0,
        2,
        6,
        71,
        &I_fast_backward_19x20,
        &I_fast_backward_hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 4, 93, &I_fast_backward_text_19x6);
    infrared_brute_force_add_record(brute_force, i++, "Fast_ba");

    // Fast Forward button
    button_panel_add_item(
        button_panel,
        i,
        1,
        2,
        39,
        71,
        &I_fast_f_19x20,
        &I_fast_f__hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 39, 93, &I_fast_f__text_19x6);
    infrared_brute_force_add_record(brute_force, i++, "Fast_fo");

    // OK/Select Button
    button_panel_add_item(
        button_panel,
        i,
        0,
        3,
        6,
        101,
        &I_ok_19x20,
        &I_ok_hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 6, 123, &I_ok_text_19x5);
    infrared_brute_force_add_record(brute_force, i++, "Ok");

    // Subtitle/CC Button
    button_panel_add_item(
        button_panel,
        i,
        1,
        3,
        39,
        101, 
        &I_subtitle_19x20,
        &I_subtitle_hover_19x20,
        infrared_scene_universal_common_item_callback,
        context);
    button_panel_add_icon(button_panel, 39, 123, &I_subtitle_text_19x5);
    infrared_brute_force_add_record(brute_force, i++, "Subtitle");

    button_panel_add_label(button_panel, 1, 11, FontPrimary, "Blu-ray/DVD");

    FURI_LOG_I(TAG, "Calling infrared_scene_universal_common_on_enter");
    infrared_scene_universal_common_on_enter(context);
    FURI_LOG_I(TAG, "Finished infrared_scene_universal_common_on_enter");
}

bool infrared_scene_universal_bluray_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_D(
        TAG, "Universal Blu-ray/DVD scene event: type=%d, event=%ld", event.type, event.event);

    bool result = infrared_scene_universal_common_on_event(context, event);

    FURI_LOG_D(TAG, "infrared_scene_universal_common_on_event result: %d", result);

    return result;
}

void infrared_scene_universal_bluray_on_exit(void* context) {
    FURI_LOG_D(TAG, "Exiting Universal Blu-ray/DVD scene");

    InfraredApp* infrared = context;
    FURI_LOG_D(
        TAG,
        "Brute force state before exit: started=%d",
        infrared_brute_force_is_started(infrared->brute_force));

    infrared_scene_universal_common_on_exit(context);

    FURI_LOG_D(TAG, "Finished exiting Universal Blu-ray/DVD scene");
}
