// Since settings app is external, it cannot access firmware functions
// For simple utils like this, easier to include C code in app rather than exposing to API
// Instead of copying the file, can (ab)use the preprocessor to insert the source code here
// Then, we still use the Header from original code as if nothing happened

// bt_get_settings(), bt_set_settings()
#include <applications/services/bt/bt_service/bt_settings_api.c>
