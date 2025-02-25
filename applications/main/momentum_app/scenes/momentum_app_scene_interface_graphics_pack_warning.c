#include "../momentum_app.h"

void momentum_app_scene_interface_graphics_pack_warning_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MomentumApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void momentum_app_scene_interface_graphics_pack_warning_on_enter(void* context) {
    MomentumApp* app = context;

    widget_add_button_element(
        app->widget,
        GuiButtonTypeLeft,
        "Back",
        momentum_app_scene_interface_graphics_pack_warning_widget_callback,
        app);
    widget_add_button_element(
        app->widget,
        GuiButtonTypeRight,
        "Info",
        momentum_app_scene_interface_graphics_pack_warning_widget_callback,
        app);
    widget_add_text_box_element(
        app->widget, 0, 0, 128, 22, AlignCenter, AlignCenter, "\e#Size Warning\e#", false);
    widget_add_string_multiline_element(
        app->widget,
        64,
        33,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Your selected pack contains\n"
        "Fonts & Icons, which remain\n"
        "loaded and use up memory.");

    view_dispatcher_switch_to_view(app->view_dispatcher, MomentumAppViewWidget);
}

bool momentum_app_scene_interface_graphics_pack_warning_on_event(
    void* context,
    SceneManagerEvent event) {
    MomentumApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            return scene_manager_previous_scene(app->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(
                app->scene_manager, MomentumAppSceneInterfaceGraphicsPackWarningInfo);
            return true;
        }
    }
    return false;
}

void momentum_app_scene_interface_graphics_pack_warning_on_exit(void* context) {
    MomentumApp* app = context;
    widget_reset(app->widget);
}
