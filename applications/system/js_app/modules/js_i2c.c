#include "../js_modules.h"
#include <furi_hal_i2c.h>

typedef struct {
    uint32_t addr;
} JsI2cInst;

static void ret_bad_args(struct mjs* mjs, const char* error) {
    mjs_prepend_errorf(mjs, MJS_BAD_ARGS_ERROR, "%s", error);
    mjs_return(mjs, MJS_UNDEFINED);
}

static bool check_arg_count_range(struct mjs* mjs, size_t min_count, size_t max_count) {
    size_t num_args = mjs_nargs(mjs);
    if(num_args < min_count || num_args > max_count) {
        ret_bad_args(mjs, "Wrong argument count");
        return false;
    }
    return true;
}

static void js_i2c_is_device_ready(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 1,2)) return;

    mjs_val_t addr_arg = mjs_arg(mjs, 0);
    if(!mjs_is_number(addr_arg)) {
        ret_bad_args(mjs, "Addr must be a number");
        return;
    }
    uint32_t addr = mjs_get_int32(mjs, addr_arg);

    uint32_t timeout = 1;
    if (mjs_nargs(mjs) > 1) { // Timeout is optional argument
        mjs_val_t timeout_arg = mjs_arg(mjs, 1);
        if(!mjs_is_number(timeout_arg)) {
            ret_bad_args(mjs, "Timeout must be a number");
            return;
        }
        timeout = mjs_get_int32(mjs, timeout_arg);
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ready = furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, addr, timeout);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    mjs_return(mjs, mjs_mk_boolean(mjs, ready));
}

static void js_i2c_write(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 2, 3)) return;

    mjs_val_t addr_arg = mjs_arg(mjs, 0);
    if(!mjs_is_number(addr_arg)) {
        ret_bad_args(mjs, "Addr must be a number");
        return;
    }
    uint32_t addr = mjs_get_int32(mjs, addr_arg);

    mjs_val_t data_arg = mjs_arg(mjs, 1);
    if(!mjs_is_array(data_arg)) {
        ret_bad_args(mjs, "Data must be an array");
        return;
    }
    size_t data_len = mjs_array_length(mjs, data_arg);
    if(data_len == 0) {
        ret_bad_args(mjs, "Data array must not be empty");
        return;
    }
    uint8_t* data = malloc(data_len);
    for(size_t i = 0; i < data_len; i++) {
        mjs_val_t val = mjs_array_get(mjs, data_arg, i);
        if(!mjs_is_number(val)) {
            ret_bad_args(mjs, "Data array must contain only numbers");
            free(data);
            return;
        }
        data[i] = mjs_get_int32(mjs, val);
    }

    uint32_t timeout = 1;
    if (mjs_nargs(mjs) > 2) { // Timeout is optional argument
        mjs_val_t timeout_arg = mjs_arg(mjs, 2);
        if(!mjs_is_number(timeout_arg)) {
            ret_bad_args(mjs, "Timeout must be a number");
            free(data);
            return;
        }
        timeout = mjs_get_int32(mjs, timeout_arg);
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool result = furi_hal_i2c_tx(&furi_hal_i2c_handle_external, addr, data, data_len, timeout);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    free(data);
    mjs_return(mjs, mjs_mk_boolean(mjs, result));
}

static void js_i2c_read(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 2, 3)) return;

    mjs_val_t addr_arg = mjs_arg(mjs, 0);
    if(!mjs_is_number(addr_arg)) {
        ret_bad_args(mjs, "Addr must be a number");
        return;
    }
    uint32_t addr = mjs_get_int32(mjs, addr_arg);

    mjs_val_t length_arg = mjs_arg(mjs, 1);
    if(!mjs_is_number(length_arg)) {
        ret_bad_args(mjs, "Length must be a number");
        return;
    }
    size_t len = mjs_get_int32(mjs, length_arg);
    if(len == 0) {
        ret_bad_args(mjs, "Length must not zero");
        return;
    }

    uint8_t* mem_addr = malloc(len);
    memset(mem_addr, 0, len);

    uint32_t timeout = 1;
    if (mjs_nargs(mjs) > 2) { // Timeout is optional argument
        mjs_val_t timeout_arg = mjs_arg(mjs, 2);
        if(!mjs_is_number(timeout_arg)) {
            ret_bad_args(mjs, "Timeout must be a number");
            free(mem_addr);
            return;
        }
        timeout = mjs_get_int32(mjs, timeout_arg);
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    uint8_t* read_buffer = malloc(3);
    bool result = furi_hal_i2c_rx(&furi_hal_i2c_handle_external, addr, mem_addr, len, timeout);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    mjs_val_t ret = mjs_mk_array(mjs);
    if (result) {
        for(size_t i = 0; i < len; i++) {
            mjs_array_push(mjs, ret, mjs_mk_number(mjs, mem_addr[i]));
        }
    }
    free(mem_addr);
    free(read_buffer);
    mjs_return(mjs, ret);
}

static void js_i2c_write_read(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 3, 4)) return;

    mjs_val_t addr_arg = mjs_arg(mjs, 0);
    if(!mjs_is_number(addr_arg)) {
        ret_bad_args(mjs, "Addr must be a number");
        return;
    }
    uint32_t addr = mjs_get_int32(mjs, addr_arg);

    mjs_val_t data_arg = mjs_arg(mjs, 1);
    if(!mjs_is_array(data_arg)) {
        ret_bad_args(mjs, "Data must be an array");
        return;
    }
    size_t data_len = mjs_array_length(mjs, data_arg);
    if(data_len == 0) {
        ret_bad_args(mjs, "Data array must not be empty");
        return;
    }
    uint8_t* data = malloc(data_len);
    for(size_t i = 0; i < data_len; i++) {
        mjs_val_t val = mjs_array_get(mjs, data_arg, i);
        if(!mjs_is_number(val)) {
            ret_bad_args(mjs, "Data array must contain only numbers");
            free(data);
            return;
        }
        data[i] = mjs_get_int32(mjs, val);
    }

    mjs_val_t length_arg = mjs_arg(mjs, 2);
    if(!mjs_is_number(length_arg)) {
        ret_bad_args(mjs, "Length must be a number");
        free(data);
        return;
    }
    size_t len = mjs_get_int32(mjs, length_arg);
    if(len == 0) {
        ret_bad_args(mjs, "Length must not zero");
        free(data);
        return;
    }

    uint32_t timeout = 1;
    if (mjs_nargs(mjs) > 3) { // Timeout is optional argument
        mjs_val_t timeout_arg = mjs_arg(mjs, 3);
        if(!mjs_is_number(timeout_arg)) {
            ret_bad_args(mjs, "Timeout must be a number");
            free(data);
            return;
        }
        timeout = mjs_get_int32(mjs, timeout_arg);
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    uint8_t* mem_addr = malloc(len);
    memset(mem_addr, 0, len);
    bool result = furi_hal_i2c_trx(&furi_hal_i2c_handle_external, addr, data, data_len, mem_addr, len, timeout);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    mjs_val_t ret = mjs_mk_array(mjs);
    if (result) {
        for(size_t i = 0; i < len; i++) {
            mjs_array_push(mjs, ret, mjs_mk_number(mjs, mem_addr[i]));
        }
    }
    free(data);
    free(mem_addr);
    mjs_return(mjs, ret);
}

static void* js_i2c_create(struct mjs* mjs, mjs_val_t* object) {
    JsI2cInst* i2c = malloc(sizeof(JsI2cInst));
    mjs_val_t i2c_obj = mjs_mk_object(mjs);
    mjs_set(mjs, i2c_obj, INST_PROP_NAME, ~0, mjs_mk_foreign(mjs, i2c));
    mjs_set(mjs, i2c_obj, "isDeviceReady", ~0, MJS_MK_FN(js_i2c_is_device_ready));
    mjs_set(mjs, i2c_obj, "write", ~0, MJS_MK_FN(js_i2c_write));
    mjs_set(mjs, i2c_obj, "read", ~0, MJS_MK_FN(js_i2c_read));
    mjs_set(mjs, i2c_obj, "writeRead", ~0, MJS_MK_FN(js_i2c_write_read));
    *object = i2c_obj;
    return i2c;
}

static void js_i2c_destroy(void* inst) {
    JsI2cInst* i2c = inst;
    free(i2c);
}

static const JsModuleDescriptor js_i2c_desc = {
    "i2c",
    js_i2c_create,
    js_i2c_destroy,
};

static const FlipperAppPluginDescriptor i2c_plugin_descriptor = {
    .appid = PLUGIN_APP_ID,
    .ep_api_version = PLUGIN_API_VERSION,
    .entry_point = &js_i2c_desc,
};

const FlipperAppPluginDescriptor* js_i2c_ep(void) {
    return &i2c_plugin_descriptor;
}
