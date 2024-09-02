### Added:
- Apps:
  - Games: Laser Tag (by @RocketGod-git & @jamisonderek)
- NFC: Added new Saflok parser (#196 by @zinongli & @xtruan & @zacharyweiss & @evilmog & @Arkwin)
- OFW: Desktop: New Procrastination dolphin animation (by @Astrrra)

### Updated:
- Apps:
  - Picopass: CVE-2024-41566, When keys are unknown emulate with a dummy MAC and ignore reader MACs (by @nvx)
  - Seader: Card parsing and saving UI and logic improvements (by @bettse)
  - Authenticator: Confirm token export on Flipper (by @akopachov)
  - NFC Playlist: Allow delay up to 12s (by @xtruan)
  - BLE Spam: Fix delay help section (by @Willy-JL)
- API: Publishing T5577 page 1 block count macro (by @zinongli)

### Fixed:
- Sub-GHz: Fix Acurite 986 temperature value conversion (by @Willy-JL)
- Desktop:
  - Fix disabling keybinds (by @Willy-JL)
  - Sanity check PIN length for good measure (by @Willy-JL)
  - Fix PIN locked with no PIN set edge case (by @Willy-JL)
- Settings: Fix duplicates in Power Settings when opening submenus (by @Willy-JL)
- RGB Backlight: Fix config migration (by @Willy-JL)
