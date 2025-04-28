#include "input.h"

#include <furi.h>
#include <cli/cli_main_commands.h>
#include <toolbox/cli/cli_ansi.h>
#include <toolbox/cli/cli_command.h>
#include <toolbox/args.h>
#include <toolbox/pipe.h>

static void input_cli_usage(void) {
    printf("Usage:\r\n");
    printf("input <cmd> <args>\r\n");
    printf("Cmd list:\r\n");
    printf("\tdump\t\t\t - dump input events\r\n");
    printf("\tkeyboard\t\t - use keyboard feedback to control flipper\r\n");
    printf("\tsend <key> <type>\t - send input event\r\n");
}

static void input_cli_dump_events_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);
    FuriMessageQueue* input_queue = ctx;
    furi_message_queue_put(input_queue, value, FuriWaitForever);
}

static void input_cli_dump(PipeSide* pipe, FuriString* args, FuriPubSub* event_pubsub) {
    UNUSED(args);
    FuriMessageQueue* input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    FuriPubSubSubscription* input_subscription =
        furi_pubsub_subscribe(event_pubsub, input_cli_dump_events_callback, input_queue);

    InputEvent input_event;
    printf("Press CTRL+C to stop\r\n");
    while(!cli_is_pipe_broken_or_is_etx_next_char(pipe)) {
        if(furi_message_queue_get(input_queue, &input_event, 100) == FuriStatusOk) {
            printf(
                "key: %s type: %s\r\n",
                input_get_key_name(input_event.key),
                input_get_type_name(input_event.type));
        }
    }

    furi_pubsub_unsubscribe(event_pubsub, input_subscription);
    furi_message_queue_free(input_queue);
}

static void fake_input(FuriPubSub* event_pubsub, InputKey key, InputType type) {
    bool wrap = type == InputTypeShort || type == InputTypeLong;
    InputEvent event;
    event.key = key;

    if(wrap) {
        event.type = InputTypePress;
        furi_pubsub_publish(event_pubsub, &event);
    }
    event.type = type;
    furi_pubsub_publish(event_pubsub, &event);
    if(wrap) {
        event.type = InputTypeRelease;
        furi_pubsub_publish(event_pubsub, &event);
    }
}

static void input_cli_keyboard(PipeSide* pipe, FuriString* args, FuriPubSub* event_pubsub) {
    UNUSED(args);
    printf("Using console keyboard feedback for flipper input\r\n");

    printf("\r\nUsage:\r\n");
    printf("\tMove = Arrows\r\n");
    printf("\tOk = Enter\r\n");
    printf("\tBack = Backspace/Ctrl + Q\r\n");
    printf("\tEnable hold for next key = Space (press twice to send space key)\r\n");

    printf("\r\nPress CTRL+C to stop\r\n");
    bool hold = false;
    FuriPubSub* ascii_pubsub = furi_record_open(RECORD_ASCII_EVENTS);
    while(pipe_state(pipe) == PipeStateOpen) {
        char in_chr = getchar();
        if(in_chr == CliKeyETX) break;
        InputKey send_key = InputKeyMAX;
        uint8_t send_ascii = AsciiValueNUL;

        switch(in_chr) {
        case CliKeyEsc: // Escape code for arrows
            if(!pipe_receive(pipe, &in_chr, 1) || in_chr != '[') break;
            if(!pipe_receive(pipe, &in_chr, 1)) break;
            if(in_chr >= 'A' && in_chr <= 'D') { // Arrows = Dpad
                if(hold) {
                    send_key = InputKeyUp + (in_chr - 'A'); // Same order as InputKey
                } else {
                    send_ascii = AsciiValueDC1 + (in_chr - 'A'); // Same order as DC
                }
            }
            break;
        case CliKeyBackspace: // (minicom) Backspace = Back
        case CliKeyDEL: // (putty/picocom) Backspace = Back
            if(hold) {
                send_key = InputKeyBack;
            } else {
                send_ascii = AsciiValueBS;
            }
            break;
        case 0x11: // Ctrl Q = Escape (no Esc key over CLI)
            if(hold) {
                send_key = InputKeyBack;
            } else {
                send_ascii = AsciiValueESC;
            }
            break;
        case CliKeyCR: // Enter = Ok
            if(hold) {
                send_key = InputKeyOk;
            } else {
                send_ascii = AsciiValueCR;
            }
            break;
        case CliKeySpace: // Space = Toggle hold next key
            if(hold) {
                send_ascii = ' ';
            } else {
                hold = true;
            }
            break;
        default:
            send_ascii = in_chr;
            break;
        }

        if(send_key != InputKeyMAX) {
            fake_input(event_pubsub, send_key, hold ? InputTypeLong : InputTypeShort);
            hold = false;
        }
        if(send_ascii != AsciiValueNUL) {
            AsciiEvent event = {.value = send_ascii};
            furi_pubsub_publish(ascii_pubsub, &event);
            hold = false;
        }
    }
    furi_record_close(RECORD_ASCII_EVENTS);
}

static void input_cli_send_print_usage(void) {
    printf("Invalid arguments. Usage:\r\n");
    printf("\tinput send <key> <type>\r\n");
    printf("\t\t <key>\t - one of 'up', 'down', 'left', 'right', 'back', 'ok'\r\n");
    printf("\t\t <type>\t - one of 'press', 'release', 'short', 'long'\r\n");
}

static void input_cli_send(PipeSide* pipe, FuriString* args, FuriPubSub* event_pubsub) {
    UNUSED(pipe);
    InputKey key;
    InputType type;
    FuriString* key_str;
    key_str = furi_string_alloc();
    bool parsed = false;

    do {
        // Parse Key
        if(!args_read_string_and_trim(args, key_str)) {
            break;
        }
        if(!furi_string_cmp(key_str, "up")) {
            key = InputKeyUp;
        } else if(!furi_string_cmp(key_str, "down")) {
            key = InputKeyDown;
        } else if(!furi_string_cmp(key_str, "left")) {
            key = InputKeyLeft;
        } else if(!furi_string_cmp(key_str, "right")) {
            key = InputKeyRight;
        } else if(!furi_string_cmp(key_str, "ok")) {
            key = InputKeyOk;
        } else if(!furi_string_cmp(key_str, "back")) {
            key = InputKeyBack;
        } else {
            break;
        }
        // Parse Type
        if(!furi_string_cmp(args, "press")) {
            type = InputTypePress;
        } else if(!furi_string_cmp(args, "release")) {
            type = InputTypeRelease;
        } else if(!furi_string_cmp(args, "short")) {
            type = InputTypeShort;
        } else if(!furi_string_cmp(args, "long")) {
            type = InputTypeLong;
        } else {
            break;
        }
        parsed = true;
    } while(false);

    if(parsed) { //-V547
        fake_input(event_pubsub, key, type);
    } else {
        input_cli_send_print_usage();
    }
    furi_string_free(key_str);
}

static void execute(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    FuriPubSub* event_pubsub = furi_record_open(RECORD_INPUT_EVENTS);
    FuriString* cmd;
    cmd = furi_string_alloc();

    do {
        if(!args_read_string_and_trim(args, cmd)) {
            input_cli_usage();
            break;
        }
        if(furi_string_cmp_str(cmd, "dump") == 0) {
            input_cli_dump(pipe, args, event_pubsub);
            break;
        }
        if(furi_string_cmp_str(cmd, "keyboard") == 0) {
            input_cli_keyboard(pipe, args, event_pubsub);
            break;
        }
        if(furi_string_cmp_str(cmd, "send") == 0) {
            input_cli_send(pipe, args, event_pubsub);
            break;
        }

        input_cli_usage();
    } while(false);

    furi_string_free(cmd);
    furi_record_close(RECORD_INPUT_EVENTS);
}

CLI_COMMAND_INTERFACE(input, execute, CliCommandFlagParallelSafe, 1024, CLI_APPID);
