### Added:
- Apps:
  - Sub-GHz: ProtoPirate (by @RocketGod-git & @xMasterX & @zero-mega et al.)
- Sub-GHz:
  - UL: Cardin S449 protocol full support (64bit keeloq) (with Add manually, and all button codes) (use FSK12K modulation to read the remote) (by @xMasterX & @zero-mega)
  - UL: Beninca ARC AES128 protocol full support (128bit dynamic) (with Add manually, and 2 button codes) (by @xMasterX & @zero-mega)
  - UL: Treadmill37 protocol support (37bit static) (by @xMasterX)
  - UL: New modulation FSK with 12KHz deviation (by @xMasterX)
  - UL: KingGates Stylo 4k Add manually and button switch support and refactoring of encoder (by @xMasterX)
  - UL: Stilmatic button 9 support (two buttons hold simulation) (mapped on arrow keys) (by @xMasterX)
  - UL: Sommer last button code 0x6 support (mapped on arrow keys) (by @xMasterX)
  - UL: Add 390MHz and 430.5MHz to default hopper list (6 elements like in OFW) (works well with Hopper RSSI level set for your enviroment) (by @xMasterX)
- UL: Docs: Add [full list of supported SubGHz protocols](https://github.com/Next-Flip/Momentum-Firmware/blob/dev/documentation/SubGHzSupportedSystems.md) and their frequencies/modulations that can be used for reading remotes (by @xMasterX)

### Updated:
- Apps:
  - CAN Tools: Parity with DBC format, support importing DBC files (by @MatthewKuKanich)
  - ESP Flasher: Bump Marauder 1.9.1 (by @justcallmekoko), Marauder 1.9.0 support (by @H4W9)
  - FlipSocial: Autocomplete, keyboard improvements, bugfixes (by @jblanked)
  - Geometry Dash: Major refactor, bugfixes and performance improvements, rename from Geometry Flip (by @gooseprjkt)
  - HC-SR04 Distance Sensor: Option to change measure units (by @Tyl3rA)
  - IconEdit: Save/Send animations, settings tab with canvas scale and cursor guides, bugfixes (by @rdefeo)
  - NFC Login: Code refactor, bugfixes, renamed from NFC PC Login (by @Play2BReal)
  - Seader: SAM ATR3 support, better IFSC/IFSD handling, various improvements (by @bettse)
  - Seos Compatible: Seos write support, various improvements (by @aaronjamt)
  - Sub-GHz Scheduler: Added new interval times, bugfixes and improvements (by @shalebridge)
  - Unitemp: Numerous improvements from @MLAB-project fork (by @MLAB-project)
  - UL: Update Sub-GHz apps for FM12K modulation (by @xMasterX)
- Sub-GHz:
  - UL: Counter editor refactoring (by @Dmitry422)
  - UL: Alutech AT-4N & Nice Flor S turbo speedup (by @Dmitry422)
  - UL: Sommer fm2 in Add manually now uses FM12K modulation (Sommer without fm2 tag uses FM476) (try this if regular option doesn't work for you) (by @xMasterX)
  - UL: Replaced Cars ignore option with Revers RB2 protocol ignore option (by @xMasterX)

### Fixed:
- Sub-GHz:
  - UL: Fixed button mapping for FAAC RC/XT (by @xMasterX)
  - UL: Possible Sommer timings fix (by @xMasterX)
  - UL: Various fixes (by @xMasterX)
  - UL: Nice Flor S remove extra uint64 variable (by @xMasterX)
- NFC:
  - Fix sending 32+ byte ISO 15693-3 commands (by @WillyJL)
  - UL: Fix LED not blinking at SLIX unlock (by @xMasterX)
- UL: UI: Some small changes (by @xMasterX)

### Removed:
- Sub-GHz:
  - Removed Starline, ScherKhan and Kia protocols from main Sub-GHz app, they can be decoded with `Apps > Sub-GHz > ProtoPirate` external app
  - Disabled X10 and Hormann Bisecur protocols due to flash space constraints and very limited usefulness, Momentum now has same protocol list as Unleashed
