#include "../js_modules.h"
#include <furi_hal_bt.h>
#include <extra_beacon.h>

typedef struct {
    char* data;
    char* mac_addr;
    size_t beacon_data_len;
    GapExtraBeaconConfig beacon_config;
} JSblebeaconInst;

static JSblebeaconInst* get_this_ctx(struct mjs* mjs) {
    mjs_val_t obj_inst = mjs_get(mjs, mjs_get_this(mjs), INST_PROP_NAME, ~0);
    JSblebeaconInst* storage = mjs_get_ptr(mjs, obj_inst);
    furi_assert(storage);
    return storage;
}

static void ret_bad_args(struct mjs* mjs, const char* error) {
    mjs_prepend_errorf(mjs, MJS_BAD_ARGS_ERROR, "%s", error);
    mjs_return(mjs, MJS_UNDEFINED);
}

static bool check_arg_count(struct mjs* mjs, size_t count) {
    size_t num_args = mjs_nargs(mjs);
    if(num_args != count) {
        ret_bad_args(mjs, "Wrong argument count");
        return false;
    }
    return true;
}

static bool get_str_arg(struct mjs* mjs, size_t index, char** value) {
    mjs_val_t str_obj = mjs_arg(mjs, index);
    if(!mjs_is_string(str_obj)) {
        ret_bad_args(mjs, "Argument must be a string");
        return false;
    }
    size_t str_len;
    const char* temp = mjs_get_string(mjs, &str_obj, &str_len);
    if(str_len == 0 || !temp) {
        ret_bad_args(mjs, "Bad string argument");
        return false;
    }
    
    *value = (char*)malloc(str_len + 1);
    if (!*value) {
        ret_bad_args(mjs, "Memory allocation failed");
        return false;
    }
    strncpy(*value, temp, str_len);
    (*value)[str_len] = '\0'; // Ensure null termination
    return true;
}

static uint8_t hex_char_to_uint(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}

static uint8_t* macstr_to_uint8(const char* macstr) {
    if (strlen(macstr) != 17) return NULL; 

    uint8_t* mac_bytes = (uint8_t*)malloc(6 * sizeof(uint8_t));
    if (!mac_bytes) return NULL;

    for (size_t i = 0, j = 0; i < 17; i += 3, ++j) {

        mac_bytes[j] = (hex_char_to_uint(macstr[i]) << 4) | hex_char_to_uint(macstr[i + 1]);

        if (i < 15 && macstr[i + 2] != ':' && macstr[i + 2] != '-') {
            free(mac_bytes); 
            return NULL;
        }
    }

    return mac_bytes;
}

static uint8_t* hexstr_to_uint8(const char* hexstr, size_t* out_length) {
    size_t len = strlen(hexstr);
    if (len % 2 != 0) return NULL; 

    if (len > EXTRA_BEACON_MAX_DATA_SIZE + 1) return NULL;

    *out_length = len / 2;
    uint8_t* bytes = (uint8_t*)malloc(*out_length);
    if (!bytes) return NULL; 

    for (size_t i = 0; i < *out_length; ++i) {
        bytes[i] = (hex_char_to_uint(hexstr[i * 2]) << 4) | hex_char_to_uint(hexstr[i * 2 + 1]);
    }

    return bytes;
}

static void js_blebeacon_set_data(struct mjs *mjs) {
    FURI_LOG_D("BLE", "Setting data");
    if (!check_arg_count(mjs, 1)) return;
    JSblebeaconInst* inst = get_this_ctx(mjs);
    if (!inst) {
        FURI_LOG_D("BLE", "Beacon instance is null");
        ret_bad_args(mjs, "Beacon instance is null");
        return;
    }

    if (inst->data) {
        FURI_LOG_D("BLE", "Freeing existing data");
        free(inst->data);
        inst->data = NULL; 
    }


    if (!get_str_arg(mjs, 0, &(inst->data))) return; // get_str_arg now modifies inst->data directly

    size_t data_len = 0;
    uint8_t* beacon_data = hexstr_to_uint8(inst->data, &data_len);
    if (!beacon_data) {
        FURI_LOG_D("BLE", "Failed to convert data to hex");
        ret_bad_args(mjs, "Failed to convert data to hex");
        return;
    }

    FURI_LOG_D("BLE", "Successfully set beacon data");
    furi_hal_bt_extra_beacon_set_data(beacon_data, data_len);
    free(beacon_data);
    mjs_return(mjs, MJS_UNDEFINED);
}

static void js_blebeacon_set_mac(struct mjs *mjs) {
    FURI_LOG_D("BLE", "Setting Mac");
    if (!check_arg_count(mjs, 1)) 
    {
        ret_bad_args(mjs, "Bad args");
        return;
    }
    
    JSblebeaconInst* inst = get_this_ctx(mjs);
    char* mac_addr = "";
    if (!get_str_arg(mjs, 0, &mac_addr)) return; 
    inst->mac_addr = mac_addr;
    inst->beacon_config.min_adv_interval_ms = 50;
    inst->beacon_config.max_adv_interval_ms = 150;

    inst->beacon_config.adv_channel_map = GapAdvChannelMapAll;
    inst->beacon_config.adv_power_level = GapAdvPowerLevel_0dBm;

    inst->beacon_config.address_type = GapAddressTypePublic;

    uint8_t* mac = macstr_to_uint8(mac_addr);
    if (mac) {
        memcpy(inst->beacon_config.address, mac, 6);
        furi_hal_bt_extra_beacon_set_config(&inst->beacon_config);
        mjs_return(mjs, MJS_UNDEFINED);
    } else {
        FURI_LOG_D("BLE", "Bad MacAddress");
        ret_bad_args(mjs, "Bad Mac Address");
        return;
    }
}

static void js_blebeacon_send(struct mjs *mjs) {
    
    furi_hal_bt_extra_beacon_start();

    mjs_return(mjs, MJS_UNDEFINED);
}

static void js_blebeacon_stop(struct mjs *mjs) {
    
    furi_hal_bt_extra_beacon_stop();

    mjs_return(mjs, MJS_UNDEFINED);
}

static void* js_blebeacon_create(struct mjs *mjs, mjs_val_t* object) {
    JSblebeaconInst* inst = malloc(sizeof(JSblebeaconInst));
    mjs_val_t blebeacon_obj = mjs_mk_object(mjs);
    mjs_set(mjs, blebeacon_obj, INST_PROP_NAME, ~0, mjs_mk_foreign(mjs, inst));
    mjs_set(mjs, blebeacon_obj, "setData", ~0, MJS_MK_FN(js_blebeacon_set_data));
    mjs_set(mjs, blebeacon_obj, "setMac", ~0, MJS_MK_FN(js_blebeacon_set_mac));
    mjs_set(mjs, blebeacon_obj, "send", ~0, MJS_MK_FN(js_blebeacon_send));
    mjs_set(mjs, blebeacon_obj, "stop", ~0, MJS_MK_FN(js_blebeacon_stop));
    *object = blebeacon_obj;
    return inst;
}

static void js_blebeacon_destroy(void *ptr) {
    JSblebeaconInst* inst = (JSblebeaconInst*)ptr;
    if (inst) {
        free(inst->data);
        free(inst->mac_addr);
        free(inst);
    }
}

static const JsModuleDescriptor js_blebeacon_desc = {
    "blebeacon",
    js_blebeacon_create,
    js_blebeacon_destroy,
};

static const FlipperAppPluginDescriptor blebeacon_plugin_descriptor = {
    .appid = PLUGIN_APP_ID,
    .ep_api_version = PLUGIN_API_VERSION,
    .entry_point = &js_blebeacon_desc,
};

const FlipperAppPluginDescriptor* js_blebeacon_ep(void) {
    return &blebeacon_plugin_descriptor;
}