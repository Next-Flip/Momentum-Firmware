// Since settings app is external, it cannot access firmware functions
// For simple utils like this, easier to include C code in app rather than exposing to API
// Instead of copying the file, can (ab)use the preprocessor to insert the source code here
// Then, we still use the Header from original code as if nothing happened

// desktop_view_pin_input_get_view(), desktop_view_pin_input_set_back_callback(), desktop_view_pin_input_set_label_secondary(), desktop_view_pin_input_set_pin(), desktop_view_pin_input_set_context(), desktop_view_pin_input_set_label_button(), desktop_view_pin_input_unlock_input(), desktop_view_pin_input_set_pin_position(), desktop_view_pin_input_set_label_primary(), desktop_view_pin_input_reset_pin(), desktop_view_pin_input_set_done_callback(), desktop_view_pin_input_lock_input(), desktop_view_pin_input_alloc(), desktop_view_pin_input_free()
#include <applications/services/desktop/views/desktop_view_pin_input.c>
