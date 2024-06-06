### Breaking Changes:
- VGM: Reworked color customization functionality over RPC (by @HaxSam & @Willy-JL)
  - Better rainbow support, more responsive config, custom fore/back-ground
  - If you used this, need to reflash your VGM and reconfigure the colors

### Added:
- Sub-GHz:
  - New Legrand doorbell protocol (by @user890104)
  - OFW: Princeton protocol add custom guard time (by @Skorpionm)
- NFC:
  - OFW: Mifare Plus detection support (by @Astrrra)
  - OFW: Write to ultralight cards is now possible (by @RebornedBrain)
- FBT: New `SKIP_EXTERNAL` toggle and `EXTRA_EXT_APPS` config option (by @Willy-JL)
- Desktop: Added TV animation from OFW which was missing (internal on OFW)
- UL: BadKB: Add Finnish keyboard layout (by @nicou)
- OFW: USB/CCID: Add initial ISO7816 support (by @kidbomb)
- OFW: FBT/VsCode: Tweaks for cdb generation for clangd (by @hedger)

### Updated:
- Apps:
  - VGM Tool: Add new RGB VGM firmware to support Flipper FW changes (by @HaxSam)
  - Picopass: Add acknowledgements page (by @bettse)
  - Authenticator: Fix URL format (by @akopachov)
  - NFC Playlist: Various fixes (by @acegoal07)
  - BMI160 Air Mouse: Add support for LSM6DSO (by @alex-vg & @ginkage)
  - Many apps updated for new refactors (by @Willy-JL & @xMasterX)
- OFW: Furi: Use static synchronisation primitives, prepare for event loop (by @gsurkov & @skotopes)

### Fixed:
- Archive: Fix favorite's parent folders thinking they are favorited too (by @Willy-JL)
- FBT: Consistent version/branch info, fix gitorigin (by @Willy-JL)
- AssetPacker: Pack pre-compiled icons and fonts too (by @Willy-JL)
- OFW: NFC: Fixed infinite loop in dictionary attack scene (by @RebornedBrain)
- OFW: Accessor: Disable expansion service on start (by @skotopes)
- OFW: Cleanup of various warnings from clangd (by @hedger)

### Removed:
- API: Removed `Rgb565Color` and `rgb565cmp()` since VGM colors use normal RGB colors now
