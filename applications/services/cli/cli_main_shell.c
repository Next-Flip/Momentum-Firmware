#include "cli_main_shell.h"
#include "cli_main_commands.h"
#include <toolbox/cli/cli_ansi.h>
#include <toolbox/cli/shell/cli_shell.h>
#include <furi_hal_version.h>

void cli_main_motd(void* context) {
    UNUSED(context);
    printf("CONNECT 1500000/NONE\r\n"
           "\r\n" ANSI_FG_BR_WHITE "Welcome to Kiisu Command Line Interface (compatible with Flipper Zero)!\r\n"
           "Read the manual: https://docs.flipper.net/development/cli\r\n"
           "Run `help` or `?` to list available commands\r\n"
           "Run `!` for info about the device, `power reboot` also useful\r\n"
           "\r\n" ANSI_RESET);

    const Version* firmware_version = furi_hal_version_get_firmware_version();
    if(firmware_version) {
        printf(
            "Firmware version: %s %s (%s%s built on %s)\r\n",
            version_get_gitbranch(firmware_version),
            version_get_version(firmware_version),
            version_get_githash(firmware_version),
            version_get_dirty_flag(firmware_version) ? "-dirty" : "",
            version_get_builddate(firmware_version));
    }
}

const CliCommandExternalConfig cli_main_ext_config = {
    .search_directory = "/ext/apps_data/cli/plugins",
    .fal_prefix = "cli_",
    .appid = CLI_APPID,
};
