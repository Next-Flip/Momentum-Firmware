### Breaking Changes:
- VGM: Reworked color customization functionality over RPC (by @HaxSam & @Willy-JL)
  - Better rainbow support, more responsive config, custom fore/back-ground
  - If you used this, need to reflash your VGM and reconfigure the colors

### Added:
- Sub-GHz:
  - New Legrand doorbell protocol (by @user890104)
  - OFW: Princeton protocol add custom guard time (by @Skorpionm)
- FBT: New `SKIP_EXTERNAL` toggle and `EXTRA_EXT_APPS` config option (by @Willy-JL)
- OFW: USB/CCID: Add initial ISO7816 support (by @kidbomb)
- OFW: FBT/VsCode: Tweaks for cdb generation for clangd (by @hedger)

### Updated:
- Apps:
  - VGM Tool: Add new RGB VGM firmware to support Flipper FW changes (by @HaxSam)
  - Picopass: Add acknowledgements page (by @bettse)
  - Authenticator: Fix URL format (by @akopachov)
  - Many apps updated for new message queue (by @Willy-JL)
- OFW: Furi: wrap message queue in container, prepare it for epoll (by @skotopes)

### Fixed:
- Archive: Fix favorite's parent folders thinking they are favorited too (by @Willy-JL)
- FBT: Consistent version/branch info, fix gitorigin (by @Willy-JL)
- OFW: Accessor: Disable expansion service on start (by @skotopes)

### Removed:
- API: Removed `Rgb565Color` and `rgb565cmp()` since VGM colors use normal RGB colors now
