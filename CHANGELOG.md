### Breaking Changes:
- Desktop: Settings restructured due to removal of LFS / LittleFS Internal Storage
  - You might need to reconfigure Desktop Settings (PIN code, auto lock, show clock)
  - Desktop Keybinds should transfer correctly automatically

### Added:
- Apps:
  - Tools: Key Copier (by @zinongli)
  - Sub-GHz: Music to Sub-GHz Radio (by @jamisonderek)
- Settings: Show free flash amount in internal storage info (by @Willy-JL)
- Services:
  - OFW: On SD insert load BT, Desktop, Dolphin, Expansion, Notification, Region files (by @gsurkov)
  - On SD insert also load Momentum Settings, Asset Packs, FindMy Flipper, NameSpoof, Power, SubGHz options, and migrate files (by @Willy-JL)
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
  - Seader: Remove some optional ASN1 fields, disable ASN1 debug (by @bettse)
  - NFC Playlist: Fix extension check and error messages, bugfixes and improvements (by @acegoal07)
  - ESP Flasher: Update Marauder bins to v1.0.0 (by @justcallmekoko)
  - Sub-GHz Bruteforcer: Fix one/two byte text (by @Willy-JL)
  - Pokemon Trading: Reset trade without affecting current pokemon, major refactoring (by @kbembedded)
  - T5577 Raw Writer: Code refactor, bugfixes and improvements (by @zinongli)
  - AirMouse: Fix compatibility with new firmwares (by @ginkage)
  - Flizzer Tracker: Fix app not responding to keypresses (by @LTVA1)
  - UHF RFID: Bugfixes, some refactoring, write modes (by @frux-c)
  - UL: UART Terminal: Configurable CRLF or newline mode (by @xMasterX)
  - UL: SubGHz Bruteforcer: App refactoring and code documentation (by @derskythe)
  - Various app fixes for `-Wundef` option (by @Willy-JL)
  - Many app fixes for deprecated `view_dispatcher_enable_queue()` (by @xMasterX & @Willy-JL)
- BadKB: Lower BLE conn interval like base HID profile (by @Willy-JL)
- Desktop: Refactor Keybinds, no more 63 character limit, only load when activated to save RAM (by @Willy-JL)
- MNTM Settings: SubGHz frequency add screen uses new NumberInput view (by @Willy-JL)
- GUI: Small tweaks to NumberInput to match Text and Byte, and for better usability (by @Willy-JL)
- Settings:
  - Statusbar Clock and Left Handed options show in normal Settings app like OFW (by @Willy-JL)
  - Update list of keys files for forget all devices (by @Willy-JL)
- Services:
  - Big cleanup of all services and settings handling, refactor lots old code (by @Willy-JL)
  - Update all settings paths to use equivalents like OFW or UL for better compatibility (by @Willy-JL)
- Updater: Change to `resources.tar.gz` filename to avoid confusion with update `.tgz` (by @Willy-JL)
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
- OFW: Sub-GHz: Fix RPC status for ButtonRelease event (by @Skorpionm)
- OFW: Infrared: Fix cumulative error in infrared signals (by @gsurkov)
- OFW: Desktop: Separate callbacks for dolphin and storage subscriptions (by @skotopes)
- OFW: FBT: Improved size validator for updater image (by @hedger)
- OFW: JS: Ensure proper closure of variadic function in `mjs_array` (by @derskythe)

### Removed:
- OFW: Storage: Remove LFS / LittleFS Internal Storage, all config on SD Card (by @skotopes & @gsurkov)
- Storage: Remove `CFG_PATH()` and `.config/` folder, `INT_PATH()` is now on SD card at `.int/` due to LFS removal and should be used instead (by @Willy-JL)
