# Level99 Firmware

Level99 Firmware is an unofficial personal fork based on Momentum Firmware and
the official Flipper Zero firmware.

It is not an official release of Momentum Firmware, Next-Flip, Flipper Devices,
or the Flipper Zero project. Original authors, contributors, copyright holders,
licenses, attribution, and third-party credits remain in place.

## Branding and compatibility

The user-facing names are:

- firmware name: **Level99**
- display name: **LEVEL99**
- short name: **LVL99**
- description: **Level99 Firmware**
- CAN application: **Level99 CAN**

Level99 changes the first-install slideshow, updater logo, firmware settings
label/icon, About page, displayed development version (`lvl99-dev`), and build
artifact prefix (`lvl99-...`). Internal Momentum identifiers are retained where
renaming could break settings, app compatibility, JavaScript feature detection,
updates, or ABI/API compatibility.

The monochrome source assets are reproducible with:

```sh
toolchain/current/bin/python3 scripts/generate_level99_assets.py
```

The generated C icon files remain build products and are not edited manually.

## Fresh dolphin level

Momentum's persisted dolphin structure and `.dolphin.state` path are unchanged.
The original progression thresholds through level 30 are also unchanged.
Level99 extends the same threshold table through level 99 with 600-XP steps.

The repository's level calculation defines a level as one greater than the
number of completed thresholds. The final level-98 threshold is `51399`, so
`51400` is the minimum XP that displays level 99.

`dolphin_state_load()` first attempts the existing checksummed/versioned state
load. If it succeeds, Level99 does not alter XP, mood, daily deed limits, flags,
or statistics. Only when no valid state exists (fresh install, missing state,
or corrupt state) is a zeroed state initialized with `51400` XP. The value is
obtained through `dolphin_state_level_minimum_xp(99)`, not a hard-coded UI
label. The existing state format and version are unchanged.

## Build setup

Clone the personal fork and initialize its submodules:

```sh
git clone https://github.com/snaximuscs/Momentum-Firmware.git
cd Momentum-Firmware
git submodule update --init --recursive
```

The first `fbt` invocation downloads the supported toolchain.

Build Level99 CAN by itself:

```sh
./fbt fap_level99_can
```

Expected standalone artifact:

```text
build/f7-firmware-C/.extapps/level99_can.fap
```

Build the complete firmware and update bundle:

```sh
./fbt
./fbt updater_package
```

The firmware ELF/DFU and related products are under `build/f7-firmware-C/`.
The update bundle is under `dist/` and uses a `lvl99-...` suffix.

## Installation and recovery

Install the generated update bundle with qFlipper or the Flipper mobile app
using the normal local-update flow. Back up SD-card data and device settings
first. Level99 is unofficial firmware and is installed at the user's risk.

To recover to official firmware, use the official qFlipper repair/recovery
procedure and an official Flipper Devices firmware package. DFU recovery may
be required if a normal update cannot start. Restoring official firmware does
not automatically remove unrelated files from the SD card.

## Level99 CAN

Level99 CAN is a separate repository-local application in
`applications_user/level99_can`. See its [README](../applications_user/level99_can/README.md)
for wiring, safety, operation, build details, and limitations.

The repository's CAN Commander app is preserved unchanged. CAN Commander is a
UART UI for a separate ESP32 CAN Commander board and does not contain a direct
MCP2515 SPI driver. Level99 CAN therefore implements a small direct MCP2515
driver while following the repository's established GUI, worker, storage, and
application-manifest conventions.

Level99 CAN is derived from or built upon existing CAN Commander components
where indicated. Original authors retain ownership of their work. Matthew
KuKanich's CAN Commander authorship and project attribution are preserved.

## Unverified hardware work

No physical Flipper Zero, MCP2515, transceiver, SD-card removal test, or live
CAN network was available during software validation. Hardware-dependent
behavior must be verified before relying on this firmware, especially at high
bus load and at 1 Mbit/s.
