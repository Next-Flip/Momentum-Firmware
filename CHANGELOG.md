### Breaking Changes:
- Lockscreen: Separate 'Allow RPC While Locked' settings for USB/BLE (#343 by @956MB)
  - Both default to OFF like before
  - If you had enabled this option before, you will need to re-enable

### Added:
- Apps:
  - Games: Pinball0 (by @rdefeo)
  - GPIO: FlipperHTTP: FlipWorld (by @jblanked)
  - NFC: Metroflip (by @luu176)
  - USB: USB Game Controller (by @expected-ingot)
- Archive: Setting to show dynamic path in file browser statusbar (#322 by @956MB)
- CLI: Add `clear` and `cls` commands, add `did you mean ...?` command suggestion (#342 by @dexvleads)
- Main Menu: Add coverflow menu style (#314 by @CodyTolene)
- BadKB: Added german Mac keyboard Layout (#325 by @Cloudy261)
- UL: Sub-GHz: Jolly Motors support with add manually (by @pkooiman & @xMasterX)
- OFW: Desktop: Add winter animations (by @Astrrra)
- API:
  - Added `canvas_draw_icon_animation_ex()` to draw animated icons resized (#314 by @CodyTolene)
  - OFW: Added `flipper_format_write_empty_line()` (by @janwiesemann)
- OFW: Furi: Pipe support (by @portasynthinca3)
- OFW: Furi: Thread stdin support (by @portasynthinca3)
- OFW: RPC: Command to send a signal once (by @Astrrra)
- OFW: Add VCP break support (by @gsurkov)

### Updated:
- Apps:
  - BT/USB Remote: Add PTT support for Gather (by @SapphicCode)
  - Chess: Fix illegal move bug (by @956MB)
  - Color Guess: Simplify app code (by @leedave)
  - Countdown Timer: Default to 60 seconds on open (by @andrejka27)
  - Cross Remote: Fix Sub-GHz actions rolling code support, animations for transmit, allow interrupting chain (by @leedave), loop transmit feature (by @miccayo)
  - ESP Flasher: Add c3 and c6 to s3 option (by @jaylikesbunda), update bins for Marauder to 1.2.0 (by @justcallmekoko) and FlipperHTTP to 1.6.1 (by @jblanked)
  - FlipBIP: Refactor to make adding coins easier (by @xtruan)
  - FlipLibrary: Wikipedia, dog facts, random quotes, weather, asset price, predictions, trivia, advice, uuid and many more, bug fixes (by @jblanked), holidays, improvements to connectivity and progress (by @jamisonderek)
  - FlipSocial: Improved authentication, loading screens, many bug fixes, bio and friend counts, new feed screen with posted time, search users and contacts, home announcements and notifications, private feed option, endless feed (by @jblanked), RPC_KEYBOARD support (by @jamisonderek)
  - FlipStore: Many bugfixes, support downloading ESP32 and VGM firmwares and Github repos, allow deleting apps, memory fixes, update Marauder, use Flipper catalog API (by @jblanked), more improvements (by @jamisonderek)
  - FlipTrader: Improved progress display, added connectivity check on startup (by @jamisonderek)
  - FlipWeather: Stability improvements (by @jblanked), improved progress display, added connectivity check on startup (by @jamisonderek)
  - FlipWiFi: Improve error handling, update scan loading and parsing, many bug/crash fixes, max 100 network scan, add some fast commands (by @jblanked), add connectivity check on startup (by @jamisonderek)
  - KeyCopier: Support for formats AR4, M1, AM7, Y2, Y11, S22, NA25, CO88, LW4, LW5, NA12, RU45, H75, B102, Y159, KA14, YM63, SFIC, RV (by @HonestLocksmith)
  - NFC Maker: Allow setting custom UID, code cleanup (by @Willy-JL), show extra symbols for WiFi SSID/Password and Emails (by @956MB)
  - Nightstand: Show battery percentage and show AM/PM in timer mode (by @956MB)
  - Oscilloscope: Add simple spectrum analyser and basic software scaling support (by @anfractuosity)
  - Picopass: Handle write key retry when a different card is presented, save SR as legacy from saved menu (by @bettse)
  - Pokemon Trade Tool: Update to gblink v0.63 which includes saving/loading of pin configurations for the EXT link interface, bug fixes (by @kbembedded)
  - Snake 2.0: Progress saving, endless mode, game timer, fruit positioning bugfixes (by @Willzvul)
  - uPython: Enabled extra functions for the `random` module, optimized speaker note constants to save space (by @ofabel)
  - WebCrawler: New BROWSE option to read HTML pages, many bugfixes (by @jblanked), improved progress display, added connectivity check on startup (by @jamisonderek)
  - WiFi Marauder: AirTag Spoof, flipper blespam, sniff airtag and flipper, list airtag (by @0xchocolate)
  - UL: NFC Magic: Added possibility to write 7b MFC to Gen1 tags (by @mishamyte)
  - UL: Unitemp: Fixed handling of hPa units (by @shininghero)
  - UL: Fixed apps for firmware USB CDC callback changes (by @xMasterX)
- Infrared: Update universal bluray remote (#348 by @jaylikesbunda)
- NFC:
  - OFW: Replace mf_classic_dict.nfc with Proxmark3 version (by @onovy)
  - OFW: More station IDs for Clipper plugin (by @ted-logan)
- OFW: Infrared: Add IR command for NAD DR2 D7050 D3020 (by @nikos9742)

### Fixed:
- Desktop: Fixed Wardriving animation design (by @Davim09)
- Main Menu: Fix MNTM style battery percent off by 1 (#339 by @956MB)
- OFW: Fix lost BadBLE keystrokes (by @Astrrra)
- OFW: GPIO: Fix USB UART Bridge Crash by increasing system stack size (by @Astrrra)
- OFW: Loader: Fix BusFault in handling of OOM (by @Willy-JL)
- NFC:
  - XERO: Fix issue with MFC key recovery state machine performing key reuse early (by @noproto)
  - OFW: Plantain parser Last payment amount fix (by @mxcdoam)
  - OFW: Fix skylander ID reading (by @bettse)
  - OFW: Fix MIFARE Plus detection (by @GMMan)
  - OFW: Fix ISO15693 stuck in wrong mode (by @RebornedBrain)
  - OFW: Fix MFUL PWD_AUTH command creation when 0x00 in password (by @GMMan)
  - OFW: Fix typo for `mf_classic_key_cahce_get_next_key()` function (by @luu176)
- OFW: U2F: Fix message digest memory leak (by @GMMan)
- OFW: JS: SDK workaround incorrect serial port handling by OS (by @portasynthinca3)
- OFW: FBT: Fix invalid path errors on Windows with UTF8 paths (by @Alex4386)

### Removed:
- NFC: Previous fix for ISO15693 stuck in wrong mode (#225)
  - Removes APIs `nfc_iso15693_detect_mode()`, `nfc_iso15693_force_1outof4()`, `nfc_iso15693_force_1outof256()`
