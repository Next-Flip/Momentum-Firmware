### Breaking Changes:
- OFW: JS: Renamed `textbox.emptyText()` to `textbox.clearText()`
  - If your JS scripts use these functions they need to be updated
  - Same functionality, just different naming chosen upstream

### Added:
- Apps:
  - Tools: Quac! (by @rdefeo)
  - NFC: NFC Playlist (by @acegoal07)
  - SubGHz: Restaurant Pager (by @leedave)
  - GPIO: W5500 Ethernet (by @karasevia)
  - GPIO: MALVEKE app suite (by @EstebanFuentealba)
  - GPIO: Pokemon Trading (by @EstebanFuentealba)
  - GPIO: Badge (by @jamisonderek)
- OFW: NFC: Add Slix capabilities, some bugfixes (by @gornekich)
- OFW: JS: Added `math.is_equal()` and `math.EPSILON` (by @skotopes)

### Updated:
- Apps:
  - USB/BT Remote: Added back new UI for Mouse Clicker from OFW (by @gsurkov)
  - UL: USB/BT Remote: Fix Mouse Jiggler Stealth icon in BT (by @xMasterX)
- OFW: JS: Refactored and fixed `math` and `textbox` modules (by @nminaylov & @skotopes)
- OFW: GUI: Text Box rework (by @gornekich)
- OFW: Icons: Compression fixes & larger dimension support (by @hedger)
- OFW: FuriHal: Add flash ops stats, workaround bug in SHCI_C2_SetSystemClock (by @skotopes)

### Fixed:
- Storage: Fix process aliases in rename (by @Willy-JL)
- Desktop: Show "safe to unplug the USB cable" even when locked (by @Willy-JL)
- OFW: Settings: Refactor fixes (by @Astrrra)
- OFW: GUI: Fix calling both `view_free_model()` and `view_free()` (by @Willy-JL)

### Removed:
- Nothing
