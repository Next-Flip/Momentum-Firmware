# Possible errors and what to do

When installing firmware with WebUpdater, Flipper Lab, or Flipper Mobile App, `/int/.region_data` file is created on SD card.
This file has information on allowed frequencies for country you are located in. When this is not present, SubGHz app will say "Region in not provisioned" when transmitting.

On Official Firmware, SubGHz app does not open when region not provisioned, and also receiving is not allowed if frequency is not allowed in your region.
On Momentum Firmware, SubGHz app does not restrict receiving in any way, only transmit. If transmit not allowed, it will tell you why and explain what to do. Here is more info:
- Region in not provisioned: `/int/.region_data` not found, update/reinstall firmware using WebUpdater, Flipper Lab, or Flipper Mobile App
- Frequency outside of region range: not allowed in your country, you can use Bypass Region (read below)
- Frequency outside of default range: not officially supported by Flipper, you can use Extend bands (READ BELOW!!!!)
- Frequency is outside of supported range: will not work with Flipper in any way

## How to disable SubGHz region lock restriction

#### CC1101 Frequency range specs: 300-348 MHz, 386-464 MHz, and 778-928 MHz  (+ 350MHz and 467MHz was added to default range)

This setting will unlock whole CC1101 Frequency range specifications, regardless of your current country limits. Use with caution, and check local laws!!!

You can also do this when "Region is not provisioned", but this is discouraged: it is possible that frequency is allowed for you, the error means "I don't know what is allowed or not" because `/int/.region_data` file is missing. Better to update/reinstall firmware. But if this is not possible, Bypass Region works too.

You can enable in `Momentum > Protocols > SubGHz Bypass Region Lock`.

## How to extend SubGHz supported frequency range

#### CC1101 Frequency range specs: 300-348 MHz, 386-464 MHz, and 778-928 MHz  (+ 350MHz and 467MHz was added to default range)
#### This setting will extend to: 281-361 MHz, 378-481 MHz, and 749-962 MHz

1. Please do not do that unless you know what exactly you are doing
2. You don't need extended range for almost all use cases
3. Extending frequency range and transmitting on frequencies that outside of hardware specs can damage your hardware!
4. Flipper Devices team and/or Momentum FW developers are not responsible for any damage that can be caused by using CFW or extending frequency ranges!!!

If you really sure you need that change, enable in `Momentum > Protocols > SubGHz Extend Freq Bands`.