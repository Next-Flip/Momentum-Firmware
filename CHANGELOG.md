### Breaking Changes:
- Desktop: Settings restructured due to LFS removal
  - You might need to reconfigure Desktop Settings (PIN code, auto lock, show clock)
  - Desktop Keybinds should transfer correctly automatically

### Added:
- Settings: Show free flash amount in internal storage info (by @Willy-JL)
- Services:
  - OFW: On SD insert load BT, Desktop, Dolphin, Expansion, Notification, Region files (by @gsurkov)
  - On SD insert also load Momentum settings, Asset Packs, FindMy Flipper, NameSpoof, SubGHz options, and migrate files (by @Willy-JL)
- Furi: Re-enabled `FURI_TRACE` since LFS removal frees DFU, will get better crash messages with source code path (by @Willy-JL)
- OFW: Sub-GHz: Add Dickert MAHS garage door protocol (by @OevreFlataeker)
- OFW: RFID: Add GProxII support (by @BarTenderNZ)
- OFW: iButton: Support ID writing (by @Astrrra)
- OFW: GUI: Added a text input that only accepts full numbers (by @leedave)
- FBT:
  - OFW: Add `-Wundef` to compiler options (by @hedger)
  - OFW: Ensure that all images conform specification (by @skyhawkillusions & @hedger)
  - Don't format images in external apps folder, only firmware (by @Willy-JL)

### Updated:
- Apps:
  - BLE Spam: Can use 20ms advertising again with LFS gone (by @Willy-JL)
  - Seader: Remove some optional asn1 fields (by @bettse)
  - NFC Playlist: Fix extension check and error messages (by @acegoal07)
  - ESP Flasher: Update Marauder bins to v1.0.0 (by @justcallmekoko)
  - Sub-GHz Bruteforcer: Fix one/two byte text (by @Willy-JL)
  - Various app fixes for `-Wundef` option (by @Willy-JL)
- BadKB: Lower BLE conn interval like base HID profile (by @Willy-JL)
- Desktop: Refactor Keybinds, no more 63 character limit, keybinds only loaded when pressed to save RAM (by @Willy-JL)
- Settings: Statusbar Clock and Left Handed options show in normal Settings app like OFW (by @Willy-JL)
- Services:
  - Big cleanup of services and settings handling, refactor some old code (by @Willy-JL)
  - Update all settings paths to use equivalents like OFW or UL for better compatibility (by @Willy-JL)
- OFW: NFC: Refactor detected protocols list (by @Astrrra)
- Furi:
  - OFW: FuriEventLoop Pt.2 with `Mutex` `Semaphore` `StreamBuffer`, refactor Power service (by @gsurkov)
  - OFW: Update string documentation (by @skotopes)
- OFW: CCID: App refactor (by @kidbomb)
- OFW: FBT: Toolchain v39 (by @hedger)

### Fixed:
- GUI:
  - Fix Dark Mode after XOR canvas color, like in NFC dict attack (by @Willy-JL)
  - OFW: Make file extensions case-insensitive (by @gsurkov))
- NFC:
  - OFW: Fix plantain balance string (by @Astrrra)
  - OFW: Now fifo size in ST25 chip is calculated properly (by @RebornedBrain)
- OFW: Infrared: Fix cumulative error in infrared signals (by @gsurkov)
- OFW: Desktop: Separate callbacks for dolphin and storage subscriptions (by @skotopes)
- OFW: FBT: Improved size validator for updater image (by @hedger)
- OFW: JS: Ensure proper closure of variadic function in `mjs_array` (by @derskythe)

### Removed:
- OFW: Storage: Remove LFS / LittleFS Internal Storage, all config on SD Card (by @skotopes & @gsurkov)
- Storage: Remove `CFG_PATH()` and `.config/` folder, `INT_PATH()` is now on SD card at `.int/` due to LFS removal and should be used instead (by @Willy-JL)
