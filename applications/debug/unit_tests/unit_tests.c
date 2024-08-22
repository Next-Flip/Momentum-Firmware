#include <furi.h>
#include <furi_hal.h>
#include <cli/cli.h>
#include <toolbox/run_parallel.h>

#include "test_runner.h"

void unit_tests_cli(Cli* cli, FuriString* args, void* context) {
    UNUSED(cli);
    UNUSED(context);

    TestRunner* test_runner = test_runner_alloc(cli, args);
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
    Cli* cli = furi_record_open(RECORD_CLI);
    cli_add_command(cli, "unit_tests", CliCommandFlagParallelSafe, unit_tests_cli, NULL);
    furi_record_close(RECORD_CLI);
#endif
    if(furi_hal_is_normal_boot()) {
        run_parallel(unit_tests_thread, NULL, 4 * 1024);
    }
}
