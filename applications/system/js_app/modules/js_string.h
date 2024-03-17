#include "../js_modules.h"

static void js_string_to_upper_case(struct mjs* mjs) {
    mjs_val_t arg0 = mjs_arg(mjs, 0);

    size_t str_len;
    const char* str = NULL;
    if(mjs_is_string(arg0)) {
        str = mjs_get_string(mjs, &arg0, &str_len);
    } else {
        mjs_return(mjs, MJS_UNDEFINED);
        return;
    }

    char* upperStr = strdup(str);
    for(size_t i = 0; i < str_len; i++) {
        upperStr[i] = toupper(upperStr[i]);
    }

    mjs_val_t resultStr = mjs_mk_string(mjs, upperStr, str_len, 1);
    free(upperStr);
    mjs_return(mjs, resultStr);
}

static void js_string_to_lower_case(struct mjs* mjs) {
    mjs_val_t arg0 = mjs_arg(mjs, 0);

    size_t str_len;
    const char* str = NULL;
    if(mjs_is_string(arg0)) {
        str = mjs_get_string(mjs, &arg0, &str_len);
    } else {
        mjs_return(mjs, MJS_UNDEFINED);
        return;
    }

    char* lowerStr = strdup(str);
    for(size_t i = 0; i < str_len; i++) {
        lowerStr[i] = tolower(lowerStr[i]);
    }

    mjs_val_t resultStr = mjs_mk_string(mjs, lowerStr, str_len, 1);
    free(lowerStr);
    mjs_return(mjs, resultStr);
}

static void string_utils_init(struct mjs* mjs) {
    mjs_val_t global = mjs_get_global(mjs);
    mjs_set(mjs, global, "toUpperCase", ~0, MJS_MK_FN(js_string_to_upper_case));
    mjs_set(mjs, global, "toLowerCase", ~0, MJS_MK_FN(js_string_to_lower_case));
}