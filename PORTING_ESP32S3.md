# Momentum Firmware → ESP32-S3 (N16R8) + 1.9" ST7789 320×170 — porting status

## Read this first

This is **not** a build of Momentum Firmware for ESP32-S3. Momentum (like
upstream Flipper Zero firmware) is written against the STM32WB55 — an ARM
Cortex-M4 with FreeRTOS, ST's proprietary BLE coprocessor stack, a CC1101
sub-GHz radio, an ST25R3916 NFC front-end, and a build system (`fbt`/
SConstruct) that only knows how to drive the ARM GCC toolchain against that
chip. ESP32-S3 is a different CPU architecture (Xtensa LX7) with none of
that silicon. There is no tool, and no realistic amount of scripted effort,
that turns one into the other automatically — every hardware-touching layer
has to be re-implemented by hand, and several major subsystems (sub-GHz,
NFC, the ST BLE stack) have no ESP32-S3 equivalent at all without adding
external hardware.

Sor3nt's [Flipper-Zero-ESP32-Port](https://github.com/Sor3nt/Flipper-Zero-ESP32-Port),
which this task was asked to emulate, is not a build of the real firmware
either — it's an independent codebase that recreates Flipper's *look*, not
a port of `applications/`+`furi_hal`.

What's in this commit is a **from-scratch ESP-IDF project** (`esp32s3_port/`)
that: (a) compiles the actual upstream `furi/core` sources from this repo
against ESP-IDF's FreeRTOS, with the specific ARM-only code paths in them
patched to also support Xtensa, and (b) implements real ESP32-S3 drivers for
the ST7789 panel and buttons. It boots to a tiny interactive demo (move a
square with the d-pad) that exercises the whole chain. It is **source-complete
but not compile-verified** — see "What couldn't be verified in this
session" below — and it is nowhere near running Momentum's actual
applications, GUI, or desktop. Treat this as a foundation, not a finish
line.

## Repo layout added

```
targets/esp32s3/target.json      documentation-only hardware/pin profile
                                  (NOT a real fbt target - see the file's
                                  _comment field for why)

esp32s3_port/                    the actual buildable project (ESP-IDF/CMake)
  CMakeLists.txt, sdkconfig.defaults, partitions.csv
  main/app_main.c                boots furi/core, display, input; draws demo
  components/
    furi_core/                   wraps furi/*.c + furi/core/*.c (the real
                                  upstream sources, via relative path - not
                                  copied) as an ESP-IDF component
    momentum_hal/                minimal furi_hal_* for ESP32-S3: delay,
                                  memmgr glue, power reset, and a *real*
                                  furi_hal_gpio backed by ESP-IDF's GPIO
                                  driver. board_config.h centralizes pins.
    st7789/                      ST7789 320x170 panel driver (esp_lcd based)
    mtm_input/                   polled, debounced 6-button driver
```

## Changes made to shared (non-ESP32-only) files

These are edits to files the STM32 (`f7`/`f18`) build still uses — done as
`#if defined(__arm__)` / `#else` splits so the ARM behavior is byte-for-byte
unchanged, not forks:

- **`furi/core/check.h`** — `furi_crash()`/`furi_halt()`/`furi_break()` embed
  ARM inline assembly (`register const void* r12 asm("r12")`, `bkpt 0`) that
  doesn't parse on Xtensa. Added a portable branch that just passes the
  message as a normal function argument (the register-smuggling trick only
  exists to preserve Cortex-M register state for a debugger, which doesn't
  apply here).
- **`furi/core/check.c`** — the crash/halt implementation reads Cortex-M
  debug registers (`CoreDebug->DHCSR`), STM32 headers (`stm32wbxx.h`), and
  the ST BLE coprocessor fault info (`ble_glue_get_hardfault_info`). Wrapped
  the whole thing in `#if defined(__arm__)` and added a straightforward
  portable implementation (log message + heap stats, then
  `furi_hal_power_reset()`) for everything else.
- **`furi/core/common_defines.h`** — `FURI_IS_IRQ_MASKED`/`FURI_IS_IRQ_MODE`
  used `__get_PRIMASK()`/`__get_IPSR()` (ARM CMSIS intrinsics) directly,
  unconditionally, in a header included by nearly every `.c` file in the
  firmware. Added an `#elif defined(__XTENSA__)` branch: "in ISR" maps to
  ESP-IDF's `xPortInIsrContext()`; "IRQ masked" is approximated from the
  current `PS.INTLEVEL` (read via `rsr.ps`), since Xtensa has no single
  PRIMASK-style bit — see the comment in that file for the reasoning and
  its limits.
- **`furi/core/critical.c`** — used `__disable_irq()`/`__enable_irq()`
  directly; switched to the new `FURI_DISABLE_IRQ()`/`FURI_ENABLE_IRQ()`
  macros defined alongside the above.
- **`furi/core/memmgr.c`** — `memmgr_get_total_heap()` returned
  `configTOTAL_HEAP_SIZE`, which only exists on ports with one static heap
  array. ESP-IDF manages heap across multiple capability regions (internal
  RAM + octal PSRAM) and doesn't define it. Guarded with `#ifdef` and left
  an honest stub (returns free-heap as a placeholder, with a comment
  pointing at `heap_caps_get_total_size()` as the real fix) rather than
  fabricating a number.

**Deliberately not touched**: `furi/core/memmgr_heap.c` (STM32
SRAM1/SRAM2-region-specific heap_4 fork) is excluded from the ESP32-S3
component build rather than patched — ESP-IDF's own heap allocator replaces
it, and `esp32s3_port/components/momentum_hal/src/memmgr_heap_stub.c`
supplies no-op stand-ins for the small per-thread allocation-tracking API
surface it exposed (`memmgr_heap_enable_thread_trace` etc.) so
`furi/core/thread.c` still links. Tracking itself doesn't work on this port
yet — that's an honest gap, not hidden behind a fake implementation.

`furi/core` also transitively requires `furi_hal_gpio.h` (via `furi/furi.h`),
whose STM32 version is built directly on ST LL register headers
(`stm32wbxx_ll_gpio.h`) and an alternate-function mux table for that exact
chip's pinout. `esp32s3_port/components/momentum_hal/include/furi_hal_gpio.h`
replaces it with an ESP32-S3-appropriate version (GpioPin = a plain GPIO
number; no alt-fn concept, since ESP32-S3 peripherals route through a
software GPIO matrix instead) that keeps the same function names so
higher-layer code that only calls `furi_hal_gpio_*` doesn't need to change.
It's backed by a real implementation using ESP-IDF's `driver/gpio.h`
(`momentum_hal/src/furi_hal_gpio_esp32s3.c`), not stubbed.

## What couldn't be verified in this session

This sandbox has **no ESP32-S3 hardware** and **no network path to
`dl.espressif.com`** (blocked by this environment's egress policy — every
other check confirmed the block is a policy denial, not a transient
failure), so the `xtensa-esp32s3-elf` toolchain and ESP-IDF itself could not
be installed. Git submodules (`lib/mlib`, `lib/FreeRTOS-Kernel`, etc.) are
also not checked out in this clone. **None of the code above has been
compiled.** It was written against the public ESP-IDF v5.x API from
training knowledge and cross-checked against this repo's actual sources
line by line, but treat it as a strong first draft, not verified-working
code. Specific spots flagged inline as extra-likely to need adjustment once
a real toolchain is available:

- `esp32s3_port/components/st7789/src/mtm_display.c`: the
  `esp_lcd_panel_io_spi_config_t` / `esp_lcd_panel_dev_config_t` field names
  for color order have shifted between ESP-IDF 5.0/5.1/5.2/5.3 — diff
  against whichever version you install.
- `esp32s3_port/sdkconfig.defaults` PSRAM/flash options: confirm against
  the exact N16R8 module datasheet and `idf.py menuconfig` before relying
  on them.
- Display gap/mirror/invert settings in `board_config.h` are the common
  default for this panel family, not measured on your specific module.
- Button/display GPIO numbers in `board_config.h` target a genuine
  ESP32-S3-DevKitC-1 (N16R8) and are chosen to avoid that board's strapping
  pins, native-USB pins, console UART, and (N16R8-specific) the GPIO 26-37
  range its octal PSRAM uses internally — see the rationale comment at the
  top of that file. Still unflashed, but not arbitrary.

## CI build

`.github/workflows/build-esp32s3.yml` builds `esp32s3_port/` on every push
to this branch using `espressif/esp-idf-ci-action` (runs in a Docker
container with the real toolchain — GitHub Actions runners have normal
internet access, unlike the sandbox that wrote this port) and uploads the
resulting `.bin` files (bootloader, partition table, app, and a merged
single-file image) as a workflow artifact. This is the first real
compile-verification pass this code gets; check the Actions tab for
results and fix forward from actual compiler errors rather than the
"probably right" reasoning in this document.

## Next steps to actually build and flash this locally

1. Install ESP-IDF v5.x (`xtensa-esp32s3-elf` toolchain) — this environment
   couldn't reach Espressif's download servers; do this on a machine with
   normal internet access, or just use the CI build above.
2. `git submodule update --init lib/mlib` (needed for `m-core.h`, used by
   `furi/core/check.h` and `string.c`).
3. `cd esp32s3_port && idf.py set-target esp32s3 && idf.py build`. Fix
   whatever the compiler finds — expect the color-order struct fields
   mentioned above to be the first real issue.
4. `idf.py -p <port> flash monitor` and confirm the demo square responds to
   buttons.

## Roadmap for everything this doesn't cover yet

Rough order of "what to port next," each a substantial project on its own:

1. **GUI/Canvas (Phase 2).** `applications/services/gui/canvas.c` is
   hardcoded to u8g2's 128×64 1bpp `st756x` setup
   (`u8g2_Setup_st756x_flipper`, `u8x8_hw_spi_stm32`). Two real options:
   (a) keep u8g2's monochrome drawing API and font rendering, write new
   glue (`u8x8_hw_spi_esp32`) that targets an in-RAM 1bpp buffer instead of
   real SPI, then blit that buffer through
   `st7789/mtm_display_blit_mono1()` (already stubbed in this commit,
   unverified) — preserves every app's existing draw calls unmodified; or
   (b) replace Canvas's backend with something color-native and redesign
   layouts for 320×170 the way Sor3nt's project did — much more work, but
   looks native on the wider color screen instead of a scaled monochrome
   window.
2. **Input service.** Swap `mtm_input`'s standalone event type for the real
   `applications/services/input` pub/sub service — first requires deciding
   what backs `applications/services/storage` (below), since `input.h`
   depends on it.
3. **Storage.** No SD card assumed present on this board. Decide: LittleFS
   on internal flash (the `storage` partition already reserved in
   `partitions.csv`), or real SD card if the board breaks out SPI/SDMMC
   pins, then implement `applications/services/storage`'s filesystem API on
   top.
4. **Everything hardware-specific that has no ESP32-S3 equivalent:**
   sub-GHz (`furi_hal_subghz`, needs an external CC1101/SX12xx module and a
   full rewrite — there's no built-in sub-1GHz radio), NFC/RFID
   (`furi_hal_nfc`/`furi_hal_rfid`, needs an external ST25R3916 or similar
   and a full rewrite), iButton (needs an external 1-Wire analog front
   end), infrared (ESP32-S3's RMT peripheral is a plausible carrier-PWM
   TX/RX replacement for `furi_hal_infrared`, not started), Bluetooth LE
   (ESP32-S3 has its own BT LE controller — profiles would be written
   against ESP-IDF's NimBLE, not ported from ST's stack), USB (ESP32-S3 has
   native USB-OTG, different peripheral and different host-side behavior
   than the STM32 USB stack `applications/services/cli`/DFU currently use).
5. **Once 1-3 are real**, application-layer code in `applications/` that
   only touches `furi_hal_gpio`/GUI/Input/Storage (a meaningful chunk of
   the ~100+ built-in apps, though far from all — many reach for sub-GHz,
   NFC, IR, or BLE directly) becomes portable with comparatively little
   change, since `furi/core` and the HAL boundary are the pieces this
   commit focused on making swappable.
