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
  - Tools: Tone Generator (by @GEMISIS)
- OFW: NFC: Add Slix capabilities, some bugfixes (by @gornekich)
- OFW: JS: Added `math.is_equal()` and `math.EPSILON` (by @skotopes)

### Updated:
- Apps:
  - USB/BT Remote: Added back new UI for Mouse Clicker from OFW (by @gsurkov)
  - SubGHz Playlist: Fix region lock crash, show when send fails (by @Willy-JL)
  - Quac!: Fix SubGHz protocols and crash (by @Willy-JL)
  - Seader: Fix for TLSF allocator crashes (by @Willy-JL & @bettse)
  - FlipBIP: Minor UI cleanup (by @xtruan)
  - Countdown Timer: Fixes and improvements (by @puppable & @0w0mewo)
  - Reversi: Algorithm improvements (by @achistyakov)
  - IFTTT Btn: Fix memleaks and wrong free's (by @Willy-JL)
  - UL: USB/BT Remote: Fix Mouse Jiggler Stealth icon in BT (by @xMasterX)
- OFW: JS: Refactored and fixed `math` and `textbox` modules (by @nminaylov & @skotopes)
- OFW: GUI: Text Box rework (by @gornekich)
- OFW: Icons: Compression fixes & larger dimension support (by @hedger)
- OFW: FuriHal: Add flash ops stats, workaround bug in SHCI_C2_SetSystemClock (by @skotopes)

### Fixed:
- NFC: Fix Mifare DESFire reading (by @Willy-JL)
- Storage: Fix process aliases in rename (by @Willy-JL)
- Desktop: Show "safe to unplug the USB cable" even when locked (by @Willy-JL)
- GUI: Some text and UI fixes (by @Willy-JL)
- Main Menu: Fix crash with DEBUG=1 with custom apps (by @Willy-JL)
- UL: RFID: Electra fix non-initialized encoded epilogue on render (by @Leptopt1los)
- OFW: NFC: Fix changing UID (by @gornekich)
- OFW: Settings: Refactor fixes (by @Astrrra)
- OFW: GUI: Fix calling both `view_free_model()` and `view_free()` (by @Willy-JL)
- OFW: Archive: Fix condition race on exit (by @skotopes)
- OFW: FuriHalFlash: Fix obsolete-format delay (by @hedger)

### Removed:
- NFC: Removed `nfc_device_get_abbreviated_name()` API, only used by NFC app in Momentum and in NFC Maker
