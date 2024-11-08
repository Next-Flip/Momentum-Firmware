#pragma once

#include "cli.h"

#include <furi.h>
#include <furi_hal.h>

#include <m-dict.h>
#include <m-bptree.h>
#include <m-array.h>

#include "cli_vcp.h"

#define CLI_LINE_SIZE_MAX
#define CLI_COMMANDS_TREE_RANK 4

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    CliCallback callback;
    void* context;
    uint32_t flags;
} CliCommand;

struct CliSession {
    void (*init)(void);
    void (*deinit)(void);
    size_t (*rx)(uint8_t* buffer, size_t size, uint32_t timeout);
    void (*tx)(const uint8_t* buffer, size_t size);
    void (*tx_stdout)(const char* data, size_t size);
    bool (*is_connected)(void);
};

BPTREE_DEF2(
    CliCommandTree,
    CLI_COMMANDS_TREE_RANK,
    FuriString*,
    FURI_STRING_OPLIST,
    CliCommand,
    M_POD_OPLIST)

#define M_OPL_CliCommandTree_t() BPTREE_OPLIST(CliCommandTree, M_POD_OPLIST)

struct Cli {
    CliCommandTree_t commands;
    FuriMutex* mutex;
    FuriSemaphore* idle_sem;
    FuriString* last_line;
    FuriString* line;
    CliSession* session;

    size_t cursor_position;
};

Cli* cli_alloc(void);

void cli_reset(Cli* cli);

void cli_putc(Cli* cli, char c);

void cli_stdout_callback(void* _cookie, const char* data, size_t size);

// CLI command wrapping to load from plugin file on SD card
// Just need to:
// - Use CLI_PLUGIN_WRAPPER("name", cmd_callback)
// - Replace callback usages with cmd_callback_wrapper
// - Add "name_cli" entry in app manifest to build as plugin
void cli_plugin_wrapper(const char* name, Cli* cli, FuriString* args, void* context);
#include <flipper_application/flipper_application.h>
#define CLI_PLUGIN_APP_ID      "cli"
#define CLI_PLUGIN_API_VERSION 1
#define CLI_PLUGIN_WRAPPER(plugin_name_without_cli_suffix, cli_command_callback)         \
    void cli_command_callback##_wrapper(Cli* cli, FuriString* args, void* context) {     \
        cli_plugin_wrapper(plugin_name_without_cli_suffix, cli, args, context);          \
    }                                                                                    \
    static const FlipperAppPluginDescriptor cli_command_callback##_plugin_descriptor = { \
        .appid = CLI_PLUGIN_APP_ID,                                                      \
        .ep_api_version = CLI_PLUGIN_API_VERSION,                                        \
        .entry_point = &cli_command_callback,                                            \
    };                                                                                   \
    const FlipperAppPluginDescriptor* cli_command_callback##_plugin_ep(void) {           \
        UNUSED(cli_command_callback##_wrapper);                                          \
        return &cli_command_callback##_plugin_descriptor;                                \
    }

#ifdef __cplusplus
}
#endif
