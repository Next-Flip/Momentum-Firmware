// Since settings app is external, it cannot access firmware functions
// For simple utils like this, easier to include C code in app rather than exposing to API
// Instead of copying the file, can (ab)use the preprocessor to insert the source code here
// Then, we still use the Header from original code as if nothing happened

// desktop_pin_code_is_set(), desktop_pin_code_reset(), desktop_pin_code_check(), desktop_pin_code_is_equal(), desktop_pin_code_set(), desktop_pin_lock_error_notify()
#include <applications/services/desktop/helpers/pin_code.c>
