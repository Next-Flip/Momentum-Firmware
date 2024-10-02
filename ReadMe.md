<p align="center">
  <img src=".github/assets/xero.svg">
</p>

Xero is a fork of the [official Flipper Zero firmware](https://github.com/flipperdevices/flipperzero-firmware) that focuses on providing the latest NFC/RFID features, improved memory utilization, a curated selection of applications, and above all: stability.

Flipper Zero resources:

- [Flipper Zero Official Website](https://flipperzero.one). A simple way to explain to your friends what Flipper Zero can do.
- [Official Documentation](https://docs.flipperzero.one). Learn more about your dolphin: specs, usage guides, and anything you want to ask.
- [Community Wiki](https://flipper.wiki/). Contribute to the community wiki covering what is possible on the Flipper Zero.

# Why Xero

Coming from the **Official Firmware** (OFW), you'll get:

* Newly supported cards and protocols
* Fixes and performance improvements for existing cards and protocols
* Reduced memory usage of core applications
* A curated list of community applications with a focus on utility

Coming from other custom firmware, you'll get:

* All of the above
* General stability improvements
* Reduced memory usage
* Minimal theming

# Xero features

* Ultralight C attacks (coming soon!)
* Minimal theme (coming soon!)

# Contributing

Our main goal is to build a healthy and sustainable community around Flipper, so we're open to any new ideas and contributions. We also have some rules and taboos here, so please read this page and our [Code of Conduct](/CODE_OF_CONDUCT.md) carefully.

## I want to report an issue

If you've found an issue and want to report it, please check our [Issues](https://github.com/noproto/xero-firmware/issues) page. Make sure the description contains information about the firmware version you're using, your platform, and a clear explanation of the steps to reproduce the issue.

# Firmware RoadMap

[Firmware RoadMap](https://github.com/noproto/xero-firmware/projects?query=is%3Aopen)

## Requirements

Supported development platforms:

- Windows 10+ with PowerShell and Git (x86_64)
- macOS 12+ with Command Line tools (x86_64, arm64)
- Ubuntu 20.04+ with build-essential and Git (x86_64)

Supported in-circuit debuggers (optional but highly recommended):

- [Flipper Zero Wi-Fi Development Board](https://shop.flipperzero.one/products/wifi-devboard)
- CMSIS-DAP compatible: Raspberry Pi Debug Probe and etc...
- ST-Link (v2, v3, v3mods)
- J-Link

Flipper Build System will take care of all the other dependencies.

## Cloning source code

Make sure you have enough space and clone the source code:

```shell
git clone --recursive https://github.com/noproto/xero-firmware.git
```

## Building

Build firmware using Flipper Build Tool:

```shell
./fbt
```

## Flashing firmware using an in-circuit debugger

Connect your in-circuit debugger to your Flipper and flash firmware using Flipper Build Tool:

```shell
./fbt flash
```

## Flashing firmware using USB

Make sure your Flipper is on, and your firmware is functioning. Connect your Flipper with a USB cable and flash firmware using Flipper Build Tool:

```shell
./fbt flash_usb_full
```

# Links

- Official Discord: [flipp.dev/discord](https://flipp.dev/discord)
