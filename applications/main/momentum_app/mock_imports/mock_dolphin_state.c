// Since settings app is external, it cannot access firmware functions
// For simple utils like this, easier to include C code in app rather than exposing to API
// Instead of copying the file, can (ab)use the preprocessor to insert the source code here
// Then, we still use the Header from original code as if nothing happened

// DOLPHIN_LEVELS, DOLPHIN_LEVEL_COUNT
#include <applications/services/dolphin/helpers/dolphin_state.c>
