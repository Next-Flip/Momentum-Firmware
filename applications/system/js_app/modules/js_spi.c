#include "../js_modules.h"
#include <furi_hal_spi.h>

typedef struct {
    bool acquired_bus;
} JsSpiInst;

static JsSpiInst* get_this_ctx(struct mjs* mjs) {
    mjs_val_t obj_inst = mjs_get(mjs, mjs_get_this(mjs), INST_PROP_NAME, ~0);
    JsSpiInst* spi = mjs_get_ptr(mjs, obj_inst);
    furi_assert(spi);
    return spi;
}

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

static void js_spi_acquire(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 0, 0)) return;
    JsSpiInst* spi = get_this_ctx(mjs);
    if(!spi->acquired_bus) {
        furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
        spi->acquired_bus = true;
    }
    mjs_return(mjs, MJS_UNDEFINED);
}

static void js_spi_release(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 0, 0)) return;
    JsSpiInst* spi = get_this_ctx(mjs);
    if(spi->acquired_bus) {
        furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
        spi->acquired_bus = false;
    }
    mjs_return(mjs, MJS_UNDEFINED);
}

static bool js_spi_is_acquired(struct mjs* mjs) {
    JsSpiInst* spi = get_this_ctx(mjs);
    return spi->acquired_bus;
}

static void js_spi_write(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 1, 2)) return;

    mjs_val_t tx_buf_arg = mjs_arg(mjs, 0);
    bool tx_buf_was_allocated = false;
    uint8_t* tx_buf = NULL;
    size_t tx_len = 0;
    if(mjs_is_array(tx_buf_arg)) {
        tx_len = mjs_array_length(mjs, tx_buf_arg);
        if(tx_len == 0) {
            ret_bad_args(mjs, "Data array must not be empty");
            return;
        }
        tx_buf = malloc(tx_len);
        tx_buf_was_allocated = true;
        for(size_t i = 0; i < tx_len; i++) {
            mjs_val_t val = mjs_array_get(mjs, tx_buf_arg, i);
            if(!mjs_is_number(val)) {
                ret_bad_args(mjs, "Data array must contain only numbers");
                free(tx_buf);
                return;
            }
            uint32_t byte_val = mjs_get_int32(mjs, val);
            if(byte_val > 0xFF) {
                ret_bad_args(mjs, "Data array values must be 0-255");
                free(tx_buf);
                return;
            }
            tx_buf[i] = byte_val;
        }
    } else if(mjs_is_typed_array(tx_buf_arg)) {
        mjs_val_t array_buf = tx_buf_arg;
        if(mjs_is_data_view(tx_buf_arg)) {
            array_buf = mjs_dataview_get_buf(mjs, tx_buf_arg);
        }
        tx_buf = (uint8_t*)mjs_array_buf_get_ptr(mjs, array_buf, &tx_len);
        if(tx_len == 0) {
            ret_bad_args(mjs, "Data array must not be empty");
            return;
        }
    } else {
        ret_bad_args(mjs, "Data must be an array, arraybuf or dataview");
        return;
    }

    uint32_t timeout = 1;
    if(mjs_nargs(mjs) > 1) { // Timeout is optional argument
        mjs_val_t timeout_arg = mjs_arg(mjs, 1);
        if(!mjs_is_number(timeout_arg)) {
            ret_bad_args(mjs, "Timeout must be a number");
            if(tx_buf_was_allocated) free(tx_buf);
            return;
        }
        timeout = mjs_get_int32(mjs, timeout_arg);
    }

    if(!js_spi_is_acquired(mjs)) {
        furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    }
    bool result = furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, tx_buf, tx_len, timeout);
    if(!js_spi_is_acquired(mjs)) {
        furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    }

    if(tx_buf_was_allocated) free(tx_buf);
    mjs_return(mjs, mjs_mk_boolean(mjs, result));
}

static void js_spi_read(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 1, 2)) return;

    mjs_val_t rx_len_arg = mjs_arg(mjs, 0);
    if(!mjs_is_number(rx_len_arg)) {
        ret_bad_args(mjs, "Length must be a number");
        return;
    }
    size_t rx_len = mjs_get_int32(mjs, rx_len_arg);
    if(rx_len == 0) {
        ret_bad_args(mjs, "Length must not zero");
        return;
    }

    uint8_t* rx_buf = malloc(rx_len);

    uint32_t timeout = 1;
    if(mjs_nargs(mjs) > 1) { // Timeout is optional argument
        mjs_val_t timeout_arg = mjs_arg(mjs, 1);
        if(!mjs_is_number(timeout_arg)) {
            ret_bad_args(mjs, "Timeout must be a number");
            free(rx_buf);
            return;
        }
        timeout = mjs_get_int32(mjs, timeout_arg);
    }

    if(!js_spi_is_acquired(mjs)) {
        furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    }
    bool result = furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_external, rx_buf, rx_len, timeout);
    if(!js_spi_is_acquired(mjs)) {
        furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    }

    mjs_val_t ret = MJS_UNDEFINED;
    if(result) {
        ret = mjs_mk_array_buf(mjs, (char*)rx_buf, rx_len);
    }
    free(rx_buf);
    mjs_return(mjs, ret);
}

static void js_spi_write_read(struct mjs* mjs) {
    if(!check_arg_count_range(mjs, 1, 2)) return;

    mjs_val_t tx_buf_arg = mjs_arg(mjs, 0);
    bool tx_buf_was_allocated = false;
    uint8_t* tx_buf = NULL;
    size_t data_len = 0;
    if(mjs_is_array(tx_buf_arg)) {
        data_len = mjs_array_length(mjs, tx_buf_arg);
        if(data_len == 0) {
            ret_bad_args(mjs, "Data array must not be empty");
            return;
        }
        tx_buf = malloc(data_len);
        tx_buf_was_allocated = true;
        for(size_t i = 0; i < data_len; i++) {
            mjs_val_t val = mjs_array_get(mjs, tx_buf_arg, i);
            if(!mjs_is_number(val)) {
                ret_bad_args(mjs, "Data array must contain only numbers");
                free(tx_buf);
                return;
            }
            uint32_t byte_val = mjs_get_int32(mjs, val);
            if(byte_val > 0xFF) {
                ret_bad_args(mjs, "Data array values must be 0-255");
                free(tx_buf);
                return;
            }
            tx_buf[i] = byte_val;
        }
    } else if(mjs_is_typed_array(tx_buf_arg)) {
        mjs_val_t array_buf = tx_buf_arg;
        if(mjs_is_data_view(tx_buf_arg)) {
            array_buf = mjs_dataview_get_buf(mjs, tx_buf_arg);
        }
        tx_buf = (uint8_t*)mjs_array_buf_get_ptr(mjs, array_buf, &data_len);
        if(data_len == 0) {
            ret_bad_args(mjs, "Data array must not be empty");
            return;
        }
    } else {
        ret_bad_args(mjs, "Data must be an array, arraybuf or dataview");
        return;
    }

    uint8_t* rx_buf = malloc(data_len); // RX and TX are same length for SPI writeRead.

    uint32_t timeout = 1;
    if(mjs_nargs(mjs) > 1) { // Timeout is optional argument
        mjs_val_t timeout_arg = mjs_arg(mjs, 1);
        if(!mjs_is_number(timeout_arg)) {
            ret_bad_args(mjs, "Timeout must be a number");
            if(tx_buf_was_allocated) free(tx_buf);
            free(rx_buf);
            return;
        }
        timeout = mjs_get_int32(mjs, timeout_arg);
    }

    if(!js_spi_is_acquired(mjs)) {
        furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    }
    bool result =
        furi_hal_spi_bus_trx(&furi_hal_spi_bus_handle_external, tx_buf, rx_buf, data_len, timeout);
    if(!js_spi_is_acquired(mjs)) {
        furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    }

    mjs_val_t ret = MJS_UNDEFINED;
    if(result) {
        ret = mjs_mk_array_buf(mjs, (char*)rx_buf, data_len);
    }
    if(tx_buf_was_allocated) free(tx_buf);
    free(rx_buf);
    mjs_return(mjs, ret);
}

static void* js_spi_create(struct mjs* mjs, mjs_val_t* object, JsModules* modules) {
    UNUSED(modules);
    JsSpiInst* spi = (JsSpiInst*)malloc(sizeof(JsSpiInst));
    spi->acquired_bus = false;
    mjs_val_t spi_obj = mjs_mk_object(mjs);
    mjs_set(mjs, spi_obj, INST_PROP_NAME, ~0, mjs_mk_foreign(mjs, spi));
    mjs_set(mjs, spi_obj, "acquire", ~0, MJS_MK_FN(js_spi_acquire));
    mjs_set(mjs, spi_obj, "release", ~0, MJS_MK_FN(js_spi_release));
    mjs_set(mjs, spi_obj, "write", ~0, MJS_MK_FN(js_spi_write));
    mjs_set(mjs, spi_obj, "read", ~0, MJS_MK_FN(js_spi_read));
    mjs_set(mjs, spi_obj, "writeRead", ~0, MJS_MK_FN(js_spi_write_read));
    *object = spi_obj;

    furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);
    return (void*)spi;
}

static void js_spi_destroy(void* inst) {
    JsSpiInst* spi = (JsSpiInst*)inst;
    if(spi->acquired_bus) {
        furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    }
    free(spi);
    furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);
}

static const JsModuleDescriptor js_spi_desc = {
    "spi",
    js_spi_create,
    js_spi_destroy,
    NULL,
};

static const FlipperAppPluginDescriptor spi_plugin_descriptor = {
    .appid = PLUGIN_APP_ID,
    .ep_api_version = PLUGIN_API_VERSION,
    .entry_point = &js_spi_desc,
};

const FlipperAppPluginDescriptor* js_spi_ep(void) {
    return &spi_plugin_descriptor;
}
