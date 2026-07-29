# Level99 CAN

Level99 CAN is an unofficial, passive-first Classic CAN monitor for Flipper
Zero and a directly connected MCP2515 controller.

Level99 Firmware is an unofficial personal fork based on Momentum Firmware and
the official Flipper Zero firmware. Level99 CAN is derived from or built upon
existing CAN Commander components where indicated. Original authors retain
ownership of their work.

CAN Commander by Matthew KuKanich remains present, attributed, and unmodified.
It controls a separate ESP32 CAN Commander board over UART. It has no direct
MCP2515 driver to share, so Level99 CAN uses a separate app identity and a
compact MCP2515 SPI implementation while reusing repository application, GUI,
threading, storage, and asset conventions.

## Safety

Use Level99 CAN only on hardware and networks you own or are explicitly
authorized to test.

- Never connect automotive 12 V directly to Flipper GPIO.
- CANH and CANL are bus signals, not GPIO signals.
- Use a properly powered, protected, **3.3 V logic-compatible** CAN
  transceiver.
- Common 5 V MCP2515 plus TJA1050 modules may drive 5 V logic and may not be
  safe for direct Flipper Zero GPIO connection.
- A common ground may be required, depending on the isolated/non-isolated test
  setup.
- Correct termination is required. A normal two-ended high-speed CAN bus uses
  120-ohm termination at each physical end; do not add a third terminator.
- Verify the controller oscillator before selecting 8 MHz or 16 MHz.
- This app supports Classic CAN only. It does not support CAN FD.

Listen-only mode is the factory default. Startup and passive monitoring never
request frame transmission. Listen-only prevents the MCP2515 from transmitting
ACK or error frames.

## Exact Flipper Zero wiring

Level99 CAN uses the target's existing
`furi_hal_spi_bus_handle_external`; the mapping below comes from
`targets/f7/furi_hal/furi_hal_spi_config.c`. It is not a new undocumented pin
assignment.

| Flipper expansion pin | MCU | SPI role | MCP2515 |
|---|---|---|---|
| 2 | PA7 | MOSI | SI |
| 3 | PA6 | MISO | SO |
| 4 | PA4 | chip select | CS |
| 5 | PB3 | clock | SCK |
| 6 | PB2 | interrupt input | INT |
| 8 or 11 | GND | ground | controller/transceiver ground |
| 9 | 3V3 | 3.3 V supply, only if the complete board is suitable | VCC as applicable |

Do not power an unknown automotive module from pin 9. Confirm the MCP2515
board, oscillator, regulator, level shifting, transceiver, and current draw.
A raw MCP2515 needs a separate CAN transceiver between TXCAN/RXCAN and
CANH/CANL. Connect CANH to CANH and CANL to CANL only at the transceiver.

The SPI handle runs at the repository's verified 2 MHz external-bus preset.
CS and SPI pins are released when the worker exits. INT is restored to analog
mode on exit. PB2 is sampled as an active-low status hint; bounded polling of
MCP2515 `CANINTF` provides the receive fallback.

## Supported controller configuration

- Controller: MCP2515
- CAN format: Classic CAN
- Oscillators: 8 MHz and 16 MHz
- Modes: configuration, listen-only, normal, loopback, and reset
- IDs: standard 11-bit and extended 29-bit
- Frames: data and RTR
- DLC: 0 through 8

Supported nominal rates for both oscillator selections:

| Rate | 8 MHz | 16 MHz |
|---|---:|---:|
| 10 kbit/s | yes | yes |
| 20 kbit/s | yes | yes |
| 50 kbit/s | yes | yes |
| 100 kbit/s | yes | yes |
| 125 kbit/s | yes | yes |
| 250 kbit/s | yes | yes |
| 500 kbit/s | yes | yes |
| 1 Mbit/s | yes | yes |

The CNF timing table is maintained in `mcp2515.c` and follows the Microchip
MCP2515 bit-time equations and established MCP_CAN table values. Hardware
sample-point and oscillator-tolerance tests are still required for every
board/rate combination.

## User interface

The main menu contains:

- **CAN Monitor** — live passive view with relative tick timestamp, ID,
  standard/extended marker, RTR/data marker, DLC, a compact/detailed display,
  total RX, frames/s, dropped display frames, bitrate, oscillator, mode, error
  flags, and logging state.
- **Frame Details** — full latest-frame ID, type, timestamp, RTR, DLC, hex,
  printable ASCII, occurrences in the bounded history, and time since the
  previous matching frame.
- **Filters** — enable/disable, ID type, frame type, ID range, clear, and one
  saved preset. Set minimum equal to maximum for an exact-ID filter.
- **Record Log** — start/stop structured CSV recording.
- **Saved Logs** — displays the log location for Archive/qFlipper access.
- **Transmit** — explicitly warned and confirmed one-shot transmission only.
- **Diagnostics** — controller status, reset, counters, listen-only, and
  internal loopback.
- **Settings** — oscillator, bitrate, default mode, display, and fixed pins.
- **About** — unofficial status, attribution, hardware type, and CAN FD limit.

Monitor controls:

- `OK`: start or stop reception
- `Up`: pause/resume display updates while capture and logging continue
- `Down`: clear the bounded display ring
- `Left`: compact/detailed format
- `Right`: latest frame details

The display ring is fixed at 64 frames. Old display frames are overwritten and
counted as dropped display frames. SPI receive runs in a worker thread, UI
snapshots update at 10 Hz, and the logger has a separate 32-frame bounded queue
and storage thread.

## Filters and configuration

Software filters run in the receive worker and do not block MCP2515 draining.
They support:

- exact ID (minimum equals maximum)
- ID range
- standard only
- extended only
- RTR only
- data only

Payload masking is not implemented in version 1.0. One filter preset is stored
inside the versioned application configuration. The checksummed configuration
is:

```text
/apps_data/level99_can/config.bin
```

Missing, old, or corrupt configuration falls back to:

- 8 MHz oscillator
- 500 kbit/s
- listen-only mode
- compact display
- filtering and logging disabled

CS and INT are fixed to the repository-defined external SPI mapping in version
1.0 and are shown read-only in Settings.

The persisted default controller mode is limited to listen-only or internal
loopback. Normal mode is never a startup/passive-monitoring choice; it is
entered only for an explicitly confirmed one-shot transmission.

## CSV logging

Logs are stored under:

```text
/apps_data/level99_can/logs/
```

Names use:

```text
level99_can_YYYYMMDD_HHMMSS.csv
```

A numeric suffix is added if needed; existing logs are never overwritten.
The format is:

```csv
timestamp_ms,id,id_type,rtr,dlc,data
1250,0x7E8,standard,0,8,03 41 0C 1A F8 00 00 00
```

Writes happen outside the receive and GUI paths, are flushed every 16 records,
and are synchronized/closed at stop and app exit. Missing storage, open
failure, removal/write failure, and log-queue overflow are reported without
unbounded buffering. Logging stops accepting frames after a write failure.

## Transmit safeguards

Opening Transmit shows:

> Transmit only on hardware and networks you own or are explicitly authorized
> to test. Incorrect CAN traffic can cause equipment malfunction or unsafe
> vehicle behavior.

The compact screen wording conveys the same warning within display limits.
Continuing only opens the editor. Sending a validated frame requires a second
confirmation. Validation rejects standard IDs above `0x7FF`, extended IDs above
`0x1FFFFFFF`, and DLC above 8.

Version 1.0 deliberately supports one-shot transmission only. It has no
repeated mode, fuzzing, flooding, random generation, attack tools, automatic
transmit scan, hidden transmit mode, or vehicle/safety-critical presets. The
worker enters normal mode only for the confirmed request, waits up to 100 ms
for TX completion, and then immediately asks the MCP2515 to return to
listen-only. App exit also requests listen-only before releasing SPI/GPIO
resources.

The loopback test enters MCP2515 internal loopback, sends ID `0x599` internally,
verifies the returned payload, and restores configured listen-only operation.
MCP2515 loopback mode does not place that frame on the physical CAN bus.

## Diagnostics

Status includes MCP2515 detection, SPI/absence state, requested/controller
mode, oscillator, bitrate (monitor header), INT state, EFLG, warning, passive,
bus-off and overflow states, TEC, REC, SPI errors, RX/TX/dropped/log-dropped
counters, and the last error.

Actions are:

- Reset Controller (also detects a board inserted after app startup)
- Clear Counters
- Enter Listen-Only
- Run Loopback Test

If no MCP2515 is connected at startup, the app remains usable and reports
`MCP2515 not detected`; it does not crash or enter active transmission.

## Build and install

Initialize the firmware repository and submodules first:

```sh
git submodule update --init --recursive
```

Standalone build:

```sh
./fbt fap_level99_can
```

Artifact:

```text
build/f7-firmware-C/.extapps/level99_can.fap
```

Full firmware and update bundle:

```sh
./fbt
./fbt updater_package
```

Copy the standalone FAP to the SD card's `apps/GPIO` directory, or install the
complete Level99 update bundle through qFlipper/mobile local update.

## Known limitations and required hardware tests

- Hardware behavior has not been verified in this development environment.
- No CAN FD support.
- One software filter preset; no payload mask.
- Saved Logs gives the path rather than embedding a CSV file browser.
- No repeated transmission by design.
- Fixed CS/INT mapping in version 1.0.
- The receive path polls every 2 ms with an INT status hint; it does not attach
  a GPIO interrupt callback.
- Display-drop count includes bounded-history overwrite, while log-drop counts
  logger-queue overflow separately.
- Transmit completion and ACK/error handling are not yet confirmed with
  physical-bus tests.
- MCP2515 detection, all 8/16 MHz timing combinations, heavy-load overflow,
  SD removal/reinsertion, bus-off recovery, controller hot unplug/replug,
  cleanup, and internal loopback must be tested on real hardware.
