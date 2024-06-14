### Breaking Changes:
- OFW: NFC: Reading Apple/Google Pay can crash due to new Mifare Plus poller
- VGM: Reworked color customization functionality over RPC (by @HaxSam & @Willy-JL)
  - Better rainbow support, more responsive config, custom fore/back-ground
  - If you used this, need to reflash your VGM and reconfigure the colors
- OFW: CLI: New `top` command, replaces `ps` (by @skotopes)
  - Now includes CPU usage info too

### Added:
- Sub-GHz:
  - New Legrand doorbell protocol (by @user890104)
  - OFW: Princeton protocol add custom guard time (by @Skorpionm)
- NFC:
  - OFW: Mifare Plus detection support (by @Astrrra)
  - OFW: Felica emulation (by @RebornedBrain)
  - OFW: Write to ultralight cards is now possible (by @RebornedBrain)
- OFW: RFID: Added Support for Securakey Protocol (by @zinongli)
- MNTM Settings: Click Ok on Asset Pack setting to choose from a full-screen list (by @Willy-JL)
- JS: Added ADC (analog voltage) support to gpio library (by @jamisonderek)
- FBT: New `SKIP_EXTERNAL` toggle and `EXTRA_EXT_APPS` config option (by @Willy-JL)
- Desktop: Added TV animation from OFW which was missing (internal on OFW)
- UL: BadKB: Add Finnish keyboard layout (by @nicou)
- OFW: Furi: Event loop (by @skotopes)
- OFW: RPC: Add TarExtract command, some small fixes (by @Willy-JL)
- OFW: USB/CCID: Add initial ISO7816 support (by @kidbomb)
- OFW: FBT/VsCode: Tweaks for cdb generation for clangd (by @hedger)

### Updated:
- Apps:
  - VGM Tool: New RGB VGM firmware to support Flipper FW changes (by @HaxSam)
  - Picopass: Add acknowledgements page, plugin improvements (by @bettse)
  - Authenticator: Fix URL format (by @akopachov)
  - NFC Playlist: Various fixes (by @acegoal07)
  - BMI160 Air Mouse: Add support for LSM6DSO (by @alex-vg & @ginkage)
  - UL: Mifare Nested: Free some space by simplifying nfclegacy lib (by @xMasterX)
  - UL: RFID Fuzzer: Fix worker being not in LFRFIDWorkerIdle before next key (by @xMasterX)
  - UL: Barcode: Fix backlight settings (by @xMasterX)
  - Many apps updated for new refactors (by @Willy-JL & @xMasterX)
- UL: NFC: Better plugin loading, faster launch from favourites, no lag in Saved menu (by @xMasterX)
- CLI: Simpler plugin wrapper (by @Willy-JL)
- OFW: Furi: Use static synchronisation primitives, prepare for event loop (by @gsurkov & @skotopes)
- OFW: Code Cleanup: Unused includes, useless checks, unused variables, etc... (by @skotopes)

### Fixed:
- OFW: USB: IRQ, CDC and EP fixes, no more "Operation timeout (generic)" updating from OFW (by @skotopes)
- Archive: Fix favorite's parent folders thinking they are favorited too (by @Willy-JL)
- FBT: Consistent version/branch info, fix gitorigin (by @Willy-JL)
- AssetPacker: Pack pre-compiled icons and fonts too (by @Willy-JL)
- OFW: NFC: Fixed infinite loop in dictionary attack scene (by @RebornedBrain)
- OFW: Desktop: Lockup fix, GUI improvements (by @skotopes)
- OFW: Sub-GHz: Fixed transition to Saved menu after Delete RAW (by @Skorpionm)
- OFW: Loader: fix crash on locked via cli loader (by @DrZlo13)
- OFW: Archive: Fix memory leak in favorites add/remove (by @skotopes)
- OFW: Accessor: Disable expansion service on start (by @skotopes)
- OFW: Cleanup of various warnings from clangd (by @hedger)

### Removed:
- Furi: Temp disabled `FURI_TRACE` due to DFU size, some crashes will say "furi_check failed" instead of source path
- API: Removed `Rgb565Color` and `rgb565cmp()` since VGM colors use normal RGB colors now
- OFW: CLI: Removed `ps` command, replaced by `top`
