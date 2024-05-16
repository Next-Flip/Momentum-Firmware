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
- SubGHz: New API `furi_hal_subghz_check_tx(frequency)` to know why TX is blocked (by @Willy-JL)
- OFW: NFC: Skylanders plugin (by @bettse)
- OFW: Desktop: New Akira animation (by @Astrrra)
- OFW: Loader: Add support for R_ARM_REL32 relocations (by @Sameesunkaria)
- OFW: BLE: New connection parameters negotiation scheme (by @skotopes)

### Updated:
- Apps:
  - UL: BT/USB Remote: Split into Mouse Jiggler and Mouse Jiggler Stealth (by @xMasterX)
  - Magspoof: GUI and Settings fixes (by @zacharyweiss)
- SubGHz: Increased deduplication threshold (500ms to 600ms) to fit Hormann BiSecure remotes  (#119 by @user890104)
- OFW: Infrared: Updated universals assets (by @hakuyoku2011)
- OFW: Settings: Settings menu refactoring (by @Astrrra)
- OFW: FuriHal: Move version init to early stage (by @skotopes)

### Fixed:
- SubGHz: Improved readability of Hormann BiSecur signals (#119 by @user890104)

### Removed:
- Nothing
