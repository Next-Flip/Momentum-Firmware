#include <furi.h>
#include <furi_hal.h>
#include <toolbox/pipe.h>
#include <toolbox/cli/cli_command.h>
#include <toolbox/cli/cli_registry.h>
#include <cli/cli_main_commands.h>
#include <toolbox/run_parallel.h>

#include "test_runner.h"

void unit_tests_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);

    TestRunner* test_runner = test_runner_alloc(pipe, args);
    test_runner_run(test_runner);
    test_runner_free(test_runner);
}

static int32_t unit_tests_thread(void* context) {
    UNUSED(context);
    furi_delay_ms(5000);
    FuriString* args = furi_string_alloc();
    TestRunner* test_runner = test_runner_alloc(NULL, args);
    test_runner_run(test_runner);
    test_runner_free(test_runner);
    furi_string_free(args);
    return 0;
}

void unit_tests_on_system_start(void) {
#ifdef SRV_CLI
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(
        registry, "unit_tests", CliCommandFlagParallelSafe, unit_tests_cli, NULL);
    furi_record_close(RECORD_CLI);
#endif
    if(furi_hal_is_normal_boot()) {
        run_parallel(unit_tests_thread, NULL, 4 * 1024);
    }
}
