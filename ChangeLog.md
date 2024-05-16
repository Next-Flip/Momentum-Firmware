### Breaking Changes:
- OFW: Icons: Some icons replaced and renamed
  - If your Asset Packs use these icons they need to be updated
  - Pre-included asset packs are already updated to new icons
  - `Settings/Cry_dolph_55x52` -> `Settings/dolph_cry_49x54`
  - `About/CertificationChina1_122x47` -> `About/CertificationChina1_124x47`

### Added:
- Apps:
  - Infrared: Cross Remote (by @leedave)
  - Games: Color Guess (by @leedave)
- MNTM Settings: Add warning screens for SubGHz bypass and extend (by @Willy-JL)
- SubGHz: Show reason for TX blocked (by @Willy-JL)
- SubGHz: New decoder API `get_string_brief` for short info of a received signal (#119 by @user890104)
- SubGHz: New APIs `furi_hal_subghz_check_tx(freq)` and `subghz_devices_check_tx(dev, freq)` to know if and why TX is blocked (by @Willy-JL)
- OFW: NFC: Skylanders plugin (by @bettse)
- OFW: Desktop: New Akira animation (by @Astrrra)
- OFW: Loader: Add support for R_ARM_REL32 relocations (by @Sameesunkaria)
- OFW: BLE: New connection parameters negotiation scheme (by @skotopes)
- OFW: GUI: Add `ViewHolder` to API (by @nminaylov)

### Updated:
- Apps:
  - UL: BT/USB Remote: Split into Mouse Jiggler and Mouse Jiggler Stealth (by @xMasterX)
  - Magspoof: GUI and Settings fixes (by @zacharyweiss)
- SubGHz: Increased deduplication threshold (500ms to 600ms) to fit Hormann BiSecure remotes  (#119 by @user890104)
- OFW: Infrared: Updated universals assets (by @hakuyoku2011)
- OFW: Settings: Settings menu refactoring (by @Astrrra)
- OFW: FuriHal: Move version init to early stage (by @skotopes)
- OFW: JS: Submenu module refactored (by @nminaylov)

### Fixed:
- OFW: SubGHz: Fix memory corrupt in read raw view crash (by @DrZlo13)
- SubGHz: Improved readability of Hormann BiSecur signals (#119 by @user890104)
- SubGHz: External modules follow extended and bypass settings correctly (by @Willy-JL)
- SubGHz: Fixed restoring RX only frequency (by @Willy-JL)
- SubGHz: Fixed crash when setting frequencies near range limits (by @Willy-JL)

### Removed:
- Nothing
