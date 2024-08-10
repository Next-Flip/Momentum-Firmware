// Since settings app is external, it cannot access firmware functions
// For simple utils like this, easier to include C code in app rather than exposing to API
// Instead of copying the file, can (ab)use the preprocessor to insert the source code here
// Then, we still use the Header from original code as if nothing happened

// furi_hal_rtc_get_pin_value(), furi_hal_rtc_set_pin_value()
#include <targets/f7/furi_hal/furi_hal_rtc.c>
