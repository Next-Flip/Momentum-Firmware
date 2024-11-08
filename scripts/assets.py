#!/usr/bin/env python3

import os
import shutil
import pathlib

from flipper.app import App
from flipper.assets.icon import file2image

ICONS_SUPPORTED_FORMATS = ["png"]

ICONS_TEMPLATE_H_HEADER = """#pragma once

#include <stddef.h>
#include <gui/icon.h>

"""
ICONS_TEMPLATE_H_ICON_NAME = "extern const Icon {name};\n"
ICONS_TEMPLATE_H_ICON_PATHS = """
typedef struct {
    const Icon* icon;
    const char* path;
} IconPath;

extern const IconPath ICON_PATHS[];
extern const size_t ICON_PATHS_COUNT;
"""

ICONS_TEMPLATE_C_HEADER = """#include "{assets_filename}.h"

#include <gui/icon_i.h>

"""
ICONS_TEMPLATE_C_FRAME = "const uint8_t {name}[] = {data};\n"
ICONS_TEMPLATE_C_DATA = "const uint8_t* const {name}[] = {data};\n"
ICONS_TEMPLATE_C_ICONS = "const Icon {name} = {{.width={width},.height={height},.frame_count={frame_count},.frame_rate={frame_rate},.frames=_{name}}};\n"
ICONS_TEMPLATE_C_ICON_PATH = '    {{&{name}, "{path}"}},\n'
ICONS_TEMPLATE_C_ICON_PATHS = """
const IconPath ICON_PATHS[] = {{
#ifndef FURI_RAM_EXEC
{icon_paths}
#endif
}};
const size_t ICON_PATHS_COUNT = COUNT_OF(ICON_PATHS);
"""

MAX_IMAGE_WIDTH = 2**16 - 1
MAX_IMAGE_HEIGHT = 2**16 - 1


class Main(App):
    def init(self):
        # command args
        self.subparsers = self.parser.add_subparsers(help="sub-command help")
        self.parser_icons = self.subparsers.add_parser(
            "icons", help="Process icons and build icon registry"
        )
        self.parser_icons.add_argument("input_directory", help="Source directory")
        self.parser_icons.add_argument("output_directory", help="Output directory")
        self.parser_icons.add_argument(
            "--filename",
            help="Base filename for file with icon data",
            required=False,
            default="assets_icons",
        )
        self.parser_icons.add_argument(
            "--fw-bundle",
            dest="fw_bundle",
            help="Bundle all icons and path info, only for use in firmware blob",
            default=0,
            type=int,
            required=False,
        )
        self.parser_icons.add_argument(
            "--add-include",
            dest="add_include",
            help="Add assets_icons.h include drop-in for apps",
            default=0,
            type=int,
            required=False,
        )

        self.parser_icons.set_defaults(func=self.icons)

        self.parser_manifest = self.subparsers.add_parser(
            "manifest", help="Create directory Manifest"
        )
        self.parser_manifest.add_argument("local_path", help="local_path")
        self.parser_manifest.add_argument(
            "--timestamp",
            help="timestamp value to embed",
            default=0,
            type=int,
            required=False,
        )
        self.parser_manifest.set_defaults(func=self.manifest)

        self.parser_copro = self.subparsers.add_parser(
            "copro", help="Gather copro binaries for packaging"
        )
        self.parser_copro.add_argument("cube_dir", help="Path to Cube folder")
        self.parser_copro.add_argument("output_dir", help="Path to output folder")
        self.parser_copro.add_argument(
            "--cube_ver", dest="cube_ver", help="Cube version", required=True
        )
        self.parser_copro.add_argument(
            "--stack_type", dest="stack_type", help="Stack type", required=True
        )
        self.parser_copro.add_argument(
            "--stack_file",
            dest="stack_file",
            help="Stack file name in copro folder",
            required=True,
        )
        self.parser_copro.add_argument(
            "--stack_addr",
            dest="stack_addr",
            help="Stack flash address, as per release_notes",
            type=lambda x: int(x, 16),
            default=0,
            required=False,
        )
        self.parser_copro.set_defaults(func=self.copro)

        self.parser_dolphin = self.subparsers.add_parser(
            "dolphin", help="Assemble dolphin resources"
        )
        self.parser_dolphin.add_argument(
            "-s",
            "--symbol-name",
            help="Symbol and file name in dolphin output directory",
            default=None,
        )
        self.parser_dolphin.add_argument(
            "input_directory", help="Dolphin source directory"
        )
        self.parser_dolphin.add_argument(
            "output_directory", help="Dolphin output directory"
        )
        self.parser_dolphin.set_defaults(func=self.dolphin)

        self.parser_packs = self.subparsers.add_parser(
            "packs", help="Assemble asset packs"
        )
        self.parser_packs.add_argument("input_directory", help="Packs source directory")
        self.parser_packs.add_argument(
            "output_directory", help="Packs output directory"
        )
        self.parser_packs.set_defaults(func=self.packs)

    def _icon2header(self, file):
        image = file2image(file)
        if image.width > MAX_IMAGE_WIDTH or image.height > MAX_IMAGE_HEIGHT:
            raise Exception(
                f"Image {file} is too big ({image.width}x{image.height} vs. {MAX_IMAGE_WIDTH}x{MAX_IMAGE_HEIGHT})"
            )
        return image.width, image.height, image.data_as_carray()

    def _iconIsSupported(self, filename):
        extension = filename.lower().split(".")[-1]
        return extension in ICONS_SUPPORTED_FORMATS

    def icons(self):
        self.logger.debug("Converting icons")
        icons_c = open(
            os.path.join(self.args.output_directory, f"{self.args.filename}.c"),
            "w",
            newline="\n",
        )
        icons_c.write(
            ICONS_TEMPLATE_C_HEADER.format(assets_filename=self.args.filename)
        )
        icons = []
        paths = []
        symbols = pathlib.Path(__file__).parent.parent
        if "UFBT_HOME" in os.environ:
            symbols /= "sdk_headers/f7_sdk"
        symbols = (symbols / "targets/f7/api_symbols.csv").read_text()
        api_has_icon = lambda name: f"Variable,+,{name},const Icon," in symbols
        # Traverse icons tree, append image data to source file
        for dirpath, dirnames, filenames in os.walk(self.args.input_directory):
            self.logger.debug(f"Processing directory {dirpath}")
            dirnames.sort()
            filenames.sort()
            if not filenames:
                continue
            if "frame_rate" in filenames:
                self.logger.debug("Folder contains animation")
                icon_name = "A_" + os.path.split(dirpath)[1].replace("-", "_")
                icon_in_api = api_has_icon(icon_name)
                if not self.args.fw_bundle and icon_in_api:
                    self.logger.info(
                        f"{self.args.filename}: ignoring duplicate icon {icon_name}"
                    )
                    continue
                width = height = None
                frame_count = 0
                frame_rate = 0
                frame_names = []
                for filename in sorted(filenames):
                    fullfilename = os.path.join(dirpath, filename)
                    if filename == "frame_rate":
                        frame_rate = int(open(fullfilename, "r").read().strip())
                        continue
                    elif not self._iconIsSupported(filename):
                        continue
                    self.logger.debug(f"Processing animation frame {filename}")
                    temp_width, temp_height, data = self._icon2header(fullfilename)
                    if width is None:
                        width = temp_width
                    if height is None:
                        height = temp_height
                    assert width == temp_width
                    assert height == temp_height
                    frame_name = f"_{icon_name}_{frame_count}"
                    frame_names.append(frame_name)
                    icons_c.write(
                        ICONS_TEMPLATE_C_FRAME.format(name=frame_name, data=data)
                    )
                    frame_count += 1
                assert frame_rate > 0
                assert frame_count > 0
                icons_c.write(
                    ICONS_TEMPLATE_C_DATA.format(
                        name=f"_{icon_name}", data=f'{{{",".join(frame_names)}}}'
                    )
                )
                icons_c.write("\n")
                icons.append((icon_name, width, height, frame_rate, frame_count))
                if self.args.fw_bundle and icon_in_api:
                    path = dirpath.removeprefix(self.args.input_directory)[1:]
                    paths.append((icon_name, path.replace("\\", "/")))
            else:
                # process icons
                for filename in filenames:
                    if not self._iconIsSupported(filename):
                        continue
                    self.logger.debug(f"Processing icon {filename}")
                    icon_name = "I_" + "_".join(filename.split(".")[:-1]).replace(
                        "-", "_"
                    )
                    icon_in_api = api_has_icon(icon_name)
                    if not self.args.fw_bundle and icon_in_api:
                        self.logger.info(
                            f"{self.args.filename}: ignoring duplicate icon {icon_name}"
                        )
                        continue
                    fullfilename = os.path.join(dirpath, filename)
                    width, height, data = self._icon2header(fullfilename)
                    frame_name = f"_{icon_name}_0"
                    icons_c.write(
                        ICONS_TEMPLATE_C_FRAME.format(name=frame_name, data=data)
                    )
                    icons_c.write(
                        ICONS_TEMPLATE_C_DATA.format(
                            name=f"_{icon_name}", data=f"{{{frame_name}}}"
                        )
                    )
                    icons_c.write("\n")
                    icons.append((icon_name, width, height, 0, 1))
                    if self.args.fw_bundle and icon_in_api:
                        path = fullfilename.removeprefix(self.args.input_directory)[1:]
                        paths.append(
                            (icon_name, path.replace("\\", "/").rsplit(".", 1)[0])
                        )
        # Create array of images:
        self.logger.debug("Finalizing source file")
        for name, width, height, frame_rate, frame_count in icons:
            icons_c.write(
                ICONS_TEMPLATE_C_ICONS.format(
                    name=name,
                    width=width,
                    height=height,
                    frame_rate=frame_rate,
                    frame_count=frame_count,
                )
            )
        if not self.args.fw_bundle:
            icons_c.write("\n")
        else:
            icon_paths = "\n".join(
                ICONS_TEMPLATE_C_ICON_PATH.format(name=name, path=path)
                for name, path in paths
            )
            icons_c.write(ICONS_TEMPLATE_C_ICON_PATHS.format(icon_paths=icon_paths))
        icons_c.close()

        # Create Public Header
        self.logger.debug("Creating header")
        icons_h = open(
            os.path.join(self.args.output_directory, f"{self.args.filename}.h"),
            "w",
            newline="\n",
        )
        icons_h.write(ICONS_TEMPLATE_H_HEADER)
        for name, width, height, frame_rate, frame_count in icons:
            icons_h.write(ICONS_TEMPLATE_H_ICON_NAME.format(name=name))
        if self.args.fw_bundle:
            icons_h.write(ICONS_TEMPLATE_H_ICON_PATHS)
        if self.args.add_include:
            icons_h.write("#include <assets_icons.h>\n")
        icons_h.close()
        self.logger.debug("Done")
        return 0

    def manifest(self):
        from flipper.assets.manifest import Manifest

        directory_path = os.path.normpath(self.args.local_path)
        if not os.path.isdir(directory_path):
            self.logger.error(f'"{directory_path}" is not a directory')
            exit(255)

        manifest_file = os.path.join(directory_path, "Manifest")
        old_manifest = Manifest()
        if os.path.exists(manifest_file):
            self.logger.info("Manifest is present, loading to compare")
            old_manifest.load(manifest_file)
        self.logger.info(
            f'Creating temporary Manifest for directory "{directory_path}"'
        )
        new_manifest = Manifest(self.args.timestamp)
        new_manifest.create(directory_path)

        self.logger.info("Comparing new manifest with existing")
        only_in_old, changed, only_in_new = Manifest.compare(old_manifest, new_manifest)
        for record in only_in_old:
            self.logger.debug(f"Only in old: {record}")
        for record in changed:
            self.logger.info(f"Changed: {record}")
        for record in only_in_new:
            self.logger.debug(f"Only in new: {record}")
        if any((only_in_old, changed, only_in_new)):
            self.logger.info(
                f"Manifest updated ({len(only_in_new)} new, {len(only_in_old)} removed, {len(changed)} changed)"
            )
            new_manifest.save(manifest_file)
        else:
            self.logger.info("Manifest is up-to-date!")

        self.logger.info("Complete")

        return 0

    def copro(self):
        from flipper.assets.copro import Copro

        self.logger.info("Bundling coprocessor binaries")
        copro = Copro()
        try:
            self.logger.info("Loading CUBE info")
            copro.loadCubeInfo(self.args.cube_dir, self.args.cube_ver)
            self.logger.info("Bundling")
            copro.bundle(
                self.args.output_dir,
                self.args.stack_file,
                self.args.stack_type,
                self.args.stack_addr,
            )
        except Exception as e:
            self.logger.error(f"Failed to bundle: {e}")
            return 1
        self.logger.info("Complete")

        return 0

    def dolphin(self):
        from flipper.assets.dolphin import Dolphin

        self.logger.info("Processing Dolphin sources")
        dolphin = Dolphin()
        self.logger.info("Loading data")
        dolphin.load(self.args.input_directory)
        self.logger.info("Packing")
        dolphin.pack(self.args.output_directory, self.args.symbol_name)
        self.logger.info("Complete")

        return 0

    def packs(self):
        import asset_packer

        self.logger.info("Packing custom asset packs")
        asset_packer.pack(
            self.args.input_directory,
            self.args.output_directory,
            self.logger.info,
        )
        self.logger.info("Finished custom asset packs")

        return 0


if __name__ == "__main__":
    Main()()
