#include "../infrared_app_i.h"

void infrared_scene_contribute_widget_callback(GuiButtonType result, InputType type, void* context) {
    InfraredApp* infrared = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(infrared->view_dispatcher, result);
    }
}

void infrared_scene_contribute_on_enter(void* context) {
    InfraredApp* infrared = context;
    Widget* widget = infrared->widget;

    infrared->app_state.is_contributing_remote = true;
    FURI_LOG_I("IR_CONTRIB", "Set contributing state to true");

    // Add header text
    widget_add_string_element(
        widget, 64, 11, AlignCenter, AlignBottom, FontPrimary, "Contribute IR");

    // Add body text
    widget_add_string_multiline_element(
        widget,
        64,
        46,
        AlignCenter,
        AlignBottom,
        FontSecondary,
        "This will prepare your\nremote for sharing by\nadding required metadata");

    // Add buttons
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", infrared_scene_contribute_widget_callback, infrared);

    widget_add_button_element(
        widget, GuiButtonTypeRight, "OK", infrared_scene_contribute_widget_callback, infrared);

    view_dispatcher_switch_to_view(infrared->view_dispatcher, InfraredViewWidget);
}

bool infrared_scene_contribute_on_event(void* context, SceneManagerEvent event) {
    InfraredApp* infrared = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // Handle back button
            FURI_LOG_I("IR_CONTRIB", "Back pressed, returning to edit menu");
            infrared->app_state.is_contributing_remote = false;
            consumed = scene_manager_previous_scene(infrared->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            // Handle OK button
            FURI_LOG_I(
                "IR_CONTRIB",
                "Starting contribution flow: is_contributing=%d",
                infrared->app_state.is_contributing_remote);
            infrared->app_state.edit_target = InfraredEditTargetMetadataBrand;
            infrared->app_state.edit_mode = InfraredEditModeRename;
            scene_manager_next_scene(infrared->scene_manager, InfraredSceneEditRename);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Optionally consume back button
        consumed = true;
    }

    return consumed;
}

void infrared_scene_contribute_on_exit(void* context) {
    InfraredApp* infrared = context;
    FURI_LOG_I("IR_CONTRIB", "Exiting contribute scene");

    // Only reset the widget, keep the state
    widget_reset(infrared->widget);
}
