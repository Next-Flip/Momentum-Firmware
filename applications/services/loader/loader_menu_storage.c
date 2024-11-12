#include "loader_menu_storage_i.h"

#include <core/thread.h>
#include <storage/storage.h>
#include <gui/modules/dialog_ex.h>
#include <gui/view_holder.h>
#include <assets_icons.h>

#include "loader.h"

typedef enum {
    FormatFlagContinue = (1 << 0),
    FormatFlagCancel = (1 << 1),
    FormatFlagAll = (FormatFlagContinue | FormatFlagCancel),
} FormatFlag;

static void loader_menu_storage_settings_callback(DialogExResult result, void* context) {
    FuriThread* thread = context;
    furi_thread_flags_set(
        furi_thread_get_id(thread),
        result == DialogExResultRight ? FormatFlagContinue : FormatFlagCancel);
}

static void loader_menu_storage_settings_back(void* context) {
    FuriThread* thread = context;
    furi_thread_flags_set(furi_thread_get_id(thread), FormatFlagCancel);
}

int32_t loader_menu_storage_settings(void* context) {
    Storage* storage = context;
    Loader* loader = furi_record_open(RECORD_LOADER);
    LoaderStatus result = loader_start(loader, "Storage", NULL, NULL);
    furi_record_close(RECORD_LOADER);

    if(result != LoaderStatusOk) {
        DialogEx* dialog_ex = dialog_ex_alloc();
        Gui* gui = furi_record_open(RECORD_GUI);
        ViewHolder* view_holder = view_holder_alloc();
        view_holder_attach_to_gui(view_holder, gui);
        view_holder_set_view(view_holder, dialog_ex_get_view(dialog_ex));
        dialog_ex_set_context(dialog_ex, furi_thread_get_current());
        dialog_ex_set_result_callback(dialog_ex, loader_menu_storage_settings_callback);
        view_holder_set_back_callback(
            view_holder, loader_menu_storage_settings_back, furi_thread_get_current());

        dialog_ex_set_header(dialog_ex, "Update needed", 64, 0, AlignCenter, AlignTop);
        dialog_ex_set_text(
            dialog_ex,
            "Reinstall firmware\n"
            "to run this app.\n"
            "Can format SD\n"
            "here if needed.",
            3,
            17,
            AlignLeft,
            AlignTop);
        dialog_ex_set_icon(dialog_ex, 83, 11, &I_WarningDolphinFlip_45x42);
        dialog_ex_set_right_button_text(dialog_ex, "Format SD");

        FormatFlag flag = furi_thread_flags_wait(FormatFlagAll, FuriFlagWaitAny, FuriWaitForever);
        if(flag == FormatFlagContinue) {
            char text[39];
            dialog_ex_set_header(dialog_ex, "Format SD Card?", 64, 0, AlignCenter, AlignTop);
            dialog_ex_set_icon(dialog_ex, 0, 0, NULL);
            dialog_ex_set_left_button_text(dialog_ex, "Cancel");
            dialog_ex_set_right_button_text(dialog_ex, "Format");
            for(uint8_t counter = 5; counter > 0; counter--) {
                snprintf(text, sizeof(text), "All data will be lost!\n%d presses left", counter);
                dialog_ex_set_text(dialog_ex, text, 64, 12, AlignCenter, AlignTop);
                flag = furi_thread_flags_wait(FormatFlagAll, FuriFlagWaitAny, FuriWaitForever);
                if(flag != FormatFlagContinue) break;

                if(counter == 1) {
                    dialog_ex_set_header(
                        dialog_ex, "Formatting...", 70, 32, AlignCenter, AlignCenter);
                    dialog_ex_set_text(dialog_ex, NULL, 0, 0, AlignCenter, AlignCenter);
                    dialog_ex_set_icon(dialog_ex, 15, 20, &I_LoadingHourglass_24x24);
                    dialog_ex_set_left_button_text(dialog_ex, NULL);
                    dialog_ex_set_right_button_text(dialog_ex, NULL);

                    FS_Error error = storage_sd_format(storage);
                    if(error != FSE_OK) {
                        dialog_ex_set_header(
                            dialog_ex, "Cannot Format SD Card", 64, 10, AlignCenter, AlignCenter);
                        dialog_ex_set_icon(dialog_ex, 0, 0, NULL);
                        dialog_ex_set_text(
                            dialog_ex,
                            storage_error_get_desc(error),
                            64,
                            32,
                            AlignCenter,
                            AlignCenter);
                    } else {
                        dialog_ex_set_icon(dialog_ex, 48, 6, &I_DolphinDone_80x58);
                        dialog_ex_set_header(dialog_ex, "Formatted", 5, 10, AlignLeft, AlignTop);
                    }
                    dialog_ex_set_left_button_text(dialog_ex, "Finish");
                    furi_thread_flags_wait(FormatFlagAll, FuriFlagWaitAny, FuriWaitForever);
                }
            }
        }

        view_holder_set_view(view_holder, NULL);
        view_holder_free(view_holder);
        furi_record_close(RECORD_GUI);
        dialog_ex_free(dialog_ex);
    }
    return 0;
}
