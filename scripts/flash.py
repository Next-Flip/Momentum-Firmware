#!/usr/bin/env python3

import argparse
import logging
from flipper.app import App
from flipper.assets.coprobin import CoproBinary
from flipper.cube import CubeProgrammer

STATEMENT = "AGREE_TO_LOSE_FLIPPER_FEATURES_THAT_USE_CRYPTO_ENCLAVE"


class Main(App):
    def init(self):
        self._setup_subcommands()

    def _setup_subcommands(self):
        self.subparsers = self.parser.add_subparsers(help="sub-command help")

        self._setup_command_wipe()
        self._setup_command_core1bootloader()
        self._setup_command_core1firmware()
        self._setup_command_core1()
        self._setup_command_core2fus()
        self._setup_command_core2radio()

    def _setup_command_wipe(self):
        parser_wipe = self.subparsers.add_parser("wipe", help="Wipe MCU Flash")
        self._addArgsSWD(parser_wipe)
        parser_wipe.set_defaults(func=self.wipe)

    def _setup_command_core1bootloader(self):
        parser_core1bootloader = self.subparsers.add_parser(
            "core1bootloader", help="Flash Core1 Bootloader"
        )
        self._addArgsSWD(parser_core1bootloader)
        parser_core1bootloader.add_argument(
            "bootloader", type=str, help="Bootloader binary"
        )
        parser_core1bootloader.set_defaults(func=self.core1bootloader)

    def _setup_command_core1firmware(self):
        parser_core1firmware = self.subparsers.add_parser(
            "core1firmware", help="Flash Core1 Firmware"
        )
        self._addArgsSWD(parser_core1firmware)
        parser_core1firmware.add_argument(
            "firmware", type=str, help="Firmware binary"
        )
        parser_core1firmware.set_defaults(func=self.core1firmware)

    def _setup_command_core1(self):
        parser_core1 = self.subparsers.add_parser(
            "core1", help="Flash Core1 Bootloader and Firmware"
        )
        self._addArgsSWD(parser_core1)
        parser_core1.add_argument("bootloader", type=str, help="Bootloader binary")
        parser_core1.add_argument("firmware", type=str, help="Firmware binary")
        parser_core1.set_defaults(func=self.core1)

    def _setup_command_core2fus(self):
        parser_core2fus = self.subparsers.add_parser(
            "core2fus", help="Flash Core2 Firmware Update Service"
        )
        self._addArgsSWD(parser_core2fus)
        parser_core2fus.add_argument(
            "--statement",
            type=str,
            help="NEVER FLASH FUS, IT WILL ERASE CRYPTO ENCLAVE",
            required=True,
        )
        parser_core2fus.add_argument(
            "fus_address", type=str, help="Firmware Update Service Address"
        )
        parser_core2fus.add_argument(
            "fus", type=str, help="Firmware Update Service Binary"
        )
        parser_core2fus.set_defaults(func=self.core2fus)

    def _setup_command_core2radio(self):
        parser_core2radio = self.subparsers.add_parser(
            "core2radio", help="Flash Core2 Radio stack"
        )
        self._addArgsSWD(parser_core2radio)
        parser_core2radio.add_argument("radio", type=str, help="Radio Stack Binary")
        parser_core2radio.add_argument(
            "--addr",
            dest="radio_address",
            help="Radio Stack Binary Address, as per release_notes",
            type=lambda x: int(x, 16),
            default=0,
            required=False,
        )
        parser_core2radio.set_defaults(func=self.core2radio)

    def _addArgsSWD(self, parser):
        parser.add_argument(
            "--port", type=str, help="Port to connect: swd or usb1", default="swd"
        )
        parser.add_argument("--serial", type=str, help="ST-Link Serial Number")

    def _getCubeParams(self):
        return {
            "port": self.args.port,
            "serial": self.args.serial,
        }

    def wipe(self):
        self.logger.info("Wiping flash")
        cp = CubeProgrammer(self._getCubeParams())
        self._set_rdp("0xBB")
        self._set_rdp("0xAA")
        self.logger.info("Complete")
        return 0

    def _set_rdp(self, value):
        self.logger.info(f"Setting RDP to {value}")
        cp = CubeProgrammer(self._getCubeParams())
        cp.setOptionBytes({"RDP": (value, "rw")})
        self.logger.info("Verifying RDP")
        r = cp.checkOptionBytes({"RDP": (value, "rw")})
        assert r is True
        self.logger.info(f"Result: {r}")

    def core1bootloader(self):
        self._flash("Flashing bootloader", "0x08000000", self.args.bootloader)
        return 0

    def core1firmware(self):
        self._flash("Flashing firmware", "0x08008000", self.args.firmware)
        return 0

    def core1(self):
        self.core1bootloader()
        self.core1firmware()
        return 0

    def _flash(self, action_msg, address, binary):
        self.logger.info(action_msg)
        cp = CubeProgrammer(self._getCubeParams())
        cp.flashBin(address, binary)
        self.logger.info("Complete")
        cp.resetTarget()

    def core2fus(self):
        if self.args.statement != STATEMENT:
            self.logger.error(
                "PLEASE DON'T. THIS FEATURE INTENDED ONLY FOR FACTORY FLASHING"
            )
            return 1
        self._flash("Flashing Firmware Update Service", self.args.fus_address, self.args.fus)
        return 0

    def core2radio(self):
        stack_info = CoproBinary(self.args.radio)
        if not stack_info.is_stack():
            self.logger.error("Not a Radio Stack")
            return 1
        self.logger.info(f"Will flash {stack_info.img_sig.get_version()}")

        radio_address = self.args.radio_address or stack_info.get_flash_load_addr()
        if radio_address > 0x080E0000:
            self.logger.error("I KNOW WHAT YOU DID LAST SUMMER")
            return 1

        self.logger.info("Removing Current Radio Stack")
        cp = CubeProgrammer(self._getCubeParams())
        cp.deleteCore2RadioStack()
        self._flash("Flashing Radio Stack", radio_address, self.args.radio)
        return 0


if __name__ == "__main__":
    Main()()
