#include "../js_modules.h"

static void js_string_substring(struct mjs* mjs) {
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    mjs_val_t arg1 = mjs_arg(mjs, 1);
    mjs_val_t arg2 = mjs_arg(mjs, 2);

    size_t str_len;
    const char* str = NULL;
    if(mjs_is_string(arg0)) {
        str = mjs_get_string(mjs, &arg0, &str_len);
    } else {
        mjs_return(mjs, MJS_UNDEFINED);
        return;
    }

    size_t start = mjs_is_number(arg1) ? (size_t)mjs_get_int(mjs, arg1) : (size_t)-1;
    size_t end = mjs_is_number(arg2) ? (size_t)mjs_get_int(mjs, arg2) : (size_t)str_len;

    if((int)start < 0 || end > str_len || start > end) {
        mjs_return(mjs, MJS_UNDEFINED);
        return;
    }

    mjs_val_t substr_obj = mjs_mk_string(mjs, str + start, end - start, 1);
    mjs_return(mjs, substr_obj);
}

static void js_string_get_length(struct mjs* mjs) {
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    size_t str_len;
    if(mjs_is_string(arg0)) {
        const char* str = mjs_get_string(mjs, &arg0, &str_len);
        UNUSED(str);
        mjs_return(mjs, mjs_mk_number(mjs, str_len));
    } else {
        mjs_return(mjs, MJS_UNDEFINED);
    }
}

static void js_string_slice(struct mjs* mjs) {
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    mjs_val_t arg1 = mjs_arg(mjs, 1);
    mjs_val_t arg2 = mjs_arg(mjs, 2);

    size_t str_len;
    const char* str = NULL;
    if(mjs_is_string(arg0)) {
        str = mjs_get_string(mjs, &arg0, &str_len);
    } else {
        mjs_return(mjs, MJS_UNDEFINED);
        return;
    }

    int start = mjs_is_number(arg1) ? mjs_get_int(mjs, arg1) : 0;
    int end = mjs_is_number(arg2) ? mjs_get_int(mjs, arg2) : (int)str_len;

    if(start < 0) start = (int)str_len + start < 0 ? 0 : (int)str_len + start;
    if(end < 0) end = (int)str_len + end < 0 ? 0 : (int)str_len + end;
    start = start > (int)str_len ? (int)str_len : start;
    end = end > (int)str_len ? (int)str_len : end;

    if(end < start) end = start;

    mjs_val_t resultStr = mjs_mk_string(mjs, str + start, end - start, 1);
    mjs_return(mjs, resultStr);
}

static void js_string_index_of(struct mjs* mjs) {
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    mjs_val_t arg1 = mjs_arg(mjs, 1);

    size_t str_len;
    const char* str = NULL;
    if(mjs_is_string(arg0)) {
        str = mjs_get_string(mjs, &arg0, &str_len);
    } else {
        mjs_return(mjs, MJS_UNDEFINED);
        return;
    }

    const char* searchValue = NULL;
    if(mjs_is_string(arg1)) {
        searchValue = mjs_get_string(mjs, &arg1, NULL);
    } else {
        mjs_return(mjs, MJS_UNDEFINED);
        return;
    }

    char* found = strstr(str, searchValue);
    int position = found ? (int)(found - str) : -1;

    mjs_return(mjs, mjs_mk_number(mjs, position));
}

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
    mjs_val_t string_utils_obj = mjs_mk_object(mjs);
    mjs_set(mjs, string_utils_obj, "substring", ~0, MJS_MK_FN(js_string_substring));
    mjs_set(mjs, string_utils_obj, "slice", ~0, MJS_MK_FN(js_string_slice));
    mjs_set(mjs, string_utils_obj, "indexOf", ~0, MJS_MK_FN(js_string_index_of));
    mjs_set(mjs, string_utils_obj, "toUpperCase", ~0, MJS_MK_FN(js_string_to_upper_case));
    mjs_set(mjs, string_utils_obj, "toLowerCase", ~0, MJS_MK_FN(js_string_to_lower_case));
    mjs_set(mjs, string_utils_obj, "GetLength", ~0, MJS_MK_FN(js_string_get_length));
    mjs_val_t global = mjs_get_global(mjs);
    mjs_set(mjs, global, "StringUtils", ~0, string_utils_obj);
}