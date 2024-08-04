### Added:
- OFW: RFID: Add GProxII support (by @BarTenderNZ)
- OFW: iButton: Support ID writing (by @Astrrra)
- OFW: FBT: Add `-Wundef` to compiler options (by @hedger)

### Updated:
- Apps:
  - Seader: Remove some optional asn1 fields (by @bettse)
  - NFC Playlist: Fix extension check and error messages (by @acegoal07)
  - Various app fixes for `-Wundef` option (by @Willy-JL)
- OFW: NFC: Refactor detected protocols list (by @Astrrra)
- OFW: CCID: App refactor (by @kidbomb)
- OFW: Furi: Update string documentation (by @skotopes)
- OFW: FBT: Toolchain v39 (by @hedger)

### Fixed:
- GUI: Fix Dark Mode after XOR canvas color, like in NFC dict attack (by @Willy-JL)
- OFW: NFC: Fix plantain balance string (by @Astrrra)
- OFW: JS: Ensure proper closure of variadic function in `mjs_array` (by @derskythe)

### Removed:
- Storage: Remove LFS, all config on SD Card (by @skotopes)
