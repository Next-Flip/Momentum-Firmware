### Breaking Changes:
- OFW: JS: Renamed `textbox.emptyText()` to `textbox.clearText()`
  - If your JS scripts use these functions they need to be updated
  - Same functionality, just different naming chosen upstream

### Added:
- Apps:
  - Tools: Quac! (by @rdefeo)
  - GPIO: MALVEKE app suite (by @EstebanFuentealba)
  - GPIO: Pokemon Trading (by @EstebanFuentealba)
- OFW: NFC: Add Slix capabilities, some bugfixes (by @gornekich)
- OFW: JS: Added `math.is_equal()` and `math.EPSILON` (by @skotopes)

### Updated:
- Apps:
  - USB/BT Remote: Added back new UI for Mouse Clicker from OFW (by @gsurkov)
  - UL: USB/BT Remote: Fix Mouse Jiggler Stealth icon in BT (by @xMasterX)
- OFW: JS: Refactored and fixed `math` and `textbox` modules (by @nminaylov & @skotopes)

### Fixed:
- Storage: Fix process aliases in rename (by @Willy-JL)
- OFW: Settings: Refactor fixes (by @Astrrra)
- OFW: GUI: Fix calling both `view_free_model()` and `view_free()` (by @Willy-JL)

### Removed:
- Nothing
