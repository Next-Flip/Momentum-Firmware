### Added:
- UL: Sub-GHz: Add new protocols: Marantec24 (static 24 bit) and GangQi (static 34 bit) (by @xMasterX)
- OFW: GUI: Add up and down button drawing functions to GUI elements (by @DerSkythe)
- OFW: RPC: Support 5V on GPIO control for ext. modules (by @gsurkov)
- OFW: Toolbox: Proper integer parsing library `strint` (by @portasynthinca3)

### Updated:
- Apps:
  - WAV Player: Better fix for unresponsiveness, handle thread exit signal (by @CookiePLMonster)
  - Laster Tag: External Infrared board support (by @RocketGod-git), RFID support for ammo reload (by @jamisonderek)
  - ESP Flasher: Update blackmagic bin with WiFi Logs (by @DrZlo13)
  - Picopass: File loading improvements and fixes (by @bettse)
  - Quac!: Setting for external IR board support (by @daniilty), code improvements (by @rdefeo)
- CLI: Print plugin name on load fail (by @Willy-JL)
- OFW: Infrared: Add Airwell AW-HKD012-N91 to univeral AC remote (by @valeraOlexienko)
- OFW: GUI: Change dialog_ex text ownership model (by @skotopes)
- OFW: CCID: App changes and improvements (by @kidbomb)

### Fixed:
- RFID:
  - OFW: Fix detection of GProx II cards and false detection of other cards (by @Astrrra)
  - OFW: Fix Guard GProxII False Positive and 36-bit Parsing (by @zinongli)
- Loader: Warn about missing SD card for main apps (by @Willy-JL)
- OFW: NFC: Fix crash on Ultralight unlock (by @Astrrra)
- OFW: RPC: Broken file interaction fixes (by @RebornedBrain)
- OFW: GUI: Fix dialog_ex NULL ptr crash (by @Willy-JL)
- OFW: Furi: Clean up of LFS traces (by @hedger)
- OFW: Debug: Use proper hook for handle_exit in flipperapps (by @skotopes)
