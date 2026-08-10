#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Only the subset furi/core actually needs (allocation tracking mode).
// The real RTC/clock/fault-data API surface from the STM32 target is not
// implemented yet - see PORTING_ESP32S3.md.
typedef enum {
    FuriHalRtcHeapTrackModeNone = 0,
    FuriHalRtcHeapTrackModeMain,
    FuriHalRtcHeapTrackModeTree,
    FuriHalRtcHeapTrackModeAll,
} FuriHalRtcHeapTrackMode;

FuriHalRtcHeapTrackMode furi_hal_rtc_get_heap_track_mode(void);

#ifdef __cplusplus
}
#endif
