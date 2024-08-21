#pragma once

void flipper_init(void);

#ifndef FURI_RAM_EXEC
void flipper_mount_callback(const void* message, void* context);
#endif
