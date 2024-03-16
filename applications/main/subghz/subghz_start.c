#include <cli/cli_i.h>

static void subghz_cli_command_wrapper(Cli* cli, FuriString* args, void* context) {
    cli_plugin_wrapper("subghz_cli", 1, cli, args, context);
}

static void subghz_cli_command_chat_wrapper(Cli* cli, FuriString* args, void* context) {
    furi_string_replace_at(args, 0, 0, "chat ");
    subghz_cli_command_wrapper(cli, args, context);
}

void subghz_on_system_start() {
    Cli* cli = furi_record_open(RECORD_CLI);
    cli_add_command(cli, "subghz", CliCommandFlagDefault, subghz_cli_command_wrapper, NULL);
    cli_add_command(cli, "chat", CliCommandFlagDefault, subghz_cli_command_chat_wrapper, NULL);
    furi_record_close(RECORD_CLI);
}
