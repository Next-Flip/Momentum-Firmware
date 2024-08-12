#pragma once

#include "power.h"

#include <gui/gui.h>
#include <gui/view_holder.h>

#include <toolbox/api_lock.h>
#include <assets_icons.h>
#include <loader/loader.h>

#include "views/power_off.h"
#include <power/power_settings.h>
#include "views/power_unplug_usb.h"

typedef enum {
    PowerStateNotCharging,
    PowerStateCharging,
    PowerStateCharged,
} PowerState;

struct Power {
    ViewHolder* view_holder;
    FuriPubSub* event_pubsub;
    FuriEventLoop* event_loop;
    FuriMessageQueue* message_queue;

    ViewPort* battery_view_port;
    PowerOff* view_power_off;
    PowerUnplugUsb* view_power_unplug_usb;

    PowerEvent event;
    PowerState state;
    PowerInfo info;

    bool battery_low;
    bool show_battery_low_warning;
    uint8_t battery_level;
    uint8_t power_off_timeout;

    FuriPubSub* settings_events;
    FuriPubSub* input_events_pubsub;
    FuriPubSubSubscription* input_events_subscription;
    FuriPubSub* ascii_events_pubsub;
    FuriPubSubSubscription* ascii_events_subscription;
    FuriPubSubSubscription* app_start_stop_subscription;
    FuriPubSubSubscription* settings_events_subscription;
    uint32_t shutdown_idle_delay_ms;
    FuriTimer* auto_shutdown_timer;
    Loader* loader;
};

typedef enum {
    PowerViewOff,
    PowerViewUnplugUsb,
} PowerView;

typedef enum {
    PowerMessageTypeShutdown,
    PowerMessageTypeReboot,
    PowerMessageTypeGetInfo,
    PowerMessageTypeIsBatteryHealthy,
    PowerMessageTypeShowBatteryLowWarning,
} PowerMessageType;

typedef struct {
    PowerMessageType type;
    union {
        PowerBootMode boot_mode;
        PowerInfo* power_info;
        bool* bool_param;
    };
    FuriApiLock lock;
} PowerMessage;
