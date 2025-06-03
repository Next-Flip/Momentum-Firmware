### Added:
- UL: Sub-GHz: Add keeloq ironlogic aka il100 smart clone cloners support (by @xMasterX & Vitaly)
- UL: iButton: Add TM01x Dallas write support (by @Leptopt1los)
- UL: Display: Backlight option "Always ON" (by @Dmitry422)

### Updated:
- Apps:
  - Authenticator: New options to have space between groups of digits (by @akopachov)
  - Camera Suite: Handle 128x128 image, fix image rotation bug (by @rnadyrshin)
  - Combo Cracker: Many usability improvements (by @CharlesTheGreat77)
  - ESP Flasher: Bump Marauder 1.6.2 (by @justcallmekoko), FlipperHTTP 2.0 (by @jblanked)
  - Flame RNG: New App Icon (by @Kuronons), Improved the RNG using the hardware RNG and some bit mixing (by @OrionW06)
  - FlipWiFi: Added Deauthentication mode (by @jblanked)
  - Passy: Capitalize document number (by @bettse)
  - Picopass: Bugfixes and refactoring (by @bettse)
  - Portal Of Flipper: Implement auth for the xbox 360 (by @sanjay900)
  - Quac: Fix link imports not working, fix RAW Sub-GHz files (by @xMasterX & @WillyJL), add Sub-GHz duration setting (by @rdefeo)
  - Seos Compatible: Add support for reading Seader files that have SIO, Add custom zero key ADF OID (by @bettse)
  - WiFi Marauder: Support for new commands from ESP32Marauder 1.6.x (by @justcallmekoko)
  - VGM Tool: Fixed RGB firmware UART regression (by @WillyJL)
  - UL: Sub-GHz Playlist: Add support for custom modulation presets, remake with txrx library and support for dynamic signals, cleanup code (by @xMasterX)
- OFW: Infrared: Add text scroll to remote buttons (by @956MB)
- Sub-GHz:
  - UL: Rename and extend Alarms ignore option with Hollarm & GangQi (by @xMasterX)
  - UL: Add 462.750 MHz to default subghz freqs list (by @xMasterX)
  - UL: V2 Phoenix show counter value (by @xMasterX)

### Fixed:
- CLI:
  - Fix crash when opening CLI/qFlipper/WebUpdater if some unexpected files are present in `/ext/apps_data/cli/plugins` (by @WillyJL)
  - Fix crash with `ir universal` command (by @WillyJL)
  - Fix crash with `date` command (by @WillyJL)
  - Fix temporary `nfc apdu` command (by @WillyJL)
  - OFW: Fix subghz chat command (by @GameLord2011)
- NFC:
  - Fix card info not being parsed when using Extra Actions > Read Specific Card Type (by @WillyJL)
  - UL: Fix clipper date timestamp (by @luu176)
- BadKB: Fix key combos main keys being case sensitive (by @WillyJL)
- Sub-GHz:
  - UL: Fix CAME 24bit decoder (by @xMasterX)
  - UL: Tune holtek ht12x to decode holtek only and not conflict with came 12bit (by @xMasterX)
  - UL: Fix Rename scene bug, that was replacing file name with random name when Rename is opened then closed then opened again (by @xMasterX)

### Removed:
- Nothing
