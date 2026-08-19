#!/usr/bin/env python3
"""Script to pack and recover asset packs for Momentum.

I recommend installing the mntm-asset-packer package from PyPI instead of using this script directly.
more info here: https://github.com/notnotnescap/mntm-asset-packer
This is a modification of the original asset_packer script by @Willy-JL
"""

import importlib.metadata
import io
import os
import pathlib
import re
import shutil
import struct
import sys
import time
import typing
from pathlib import Path

import heatshrink2
from PIL import Image, ImageOps

HELP_MESSAGE = """The asset packer converts animations with a specific structure to be \
efficient and compatible with the asset pack system used in Momentum.
More info: https://github.com/Kuronons/FZ_graphics

Usage :
    \033[32mmntm-asset-packer \033[0;33;1mhelp\033[0m
        \033[3mDisplays this message
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1mversion\033[0m
        \033[3mDisplays the version of the asset packer
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1mcreate <Asset Pack Name>\033[0m
        \033[3mCreates a directory with the correct file structure that can be used to prepare for the packing process.
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1mpack <./path/to/AssetPack>\033[0m
        \033[3mPacks the specified asset pack into './asset_packs/AssetPack'
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1m<./path/to/AssetPack>\033[0m
        \033[3mSame as 'mntm-asset-packer pack <./path/to/AssetPack>'
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1mpack all\033[0m
        \033[3mPacks all asset packs in the current directory into './asset_packs/'
        \033[0m
    \033[32mmntm-asset-packer\033[0m
        \033[3mSame as 'mntm-asset-packer pack all'
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1mrecover <./asset_packs/AssetPack>\033[0m
        \033[3mRecovers the png frame(s) from a compiled assets for the specified asset pack. The recovered asset pack is saved in './recovered/AssetPack'
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1mrecover all\033[0m
        \033[3mRecovers all asset packs in './asset_packs/' into './recovered/'
        \033[0m
    \033[32mmntm-asset-packer \033[0;33;1mconvert <./path/to/AssetPack>\033[0m
        \033[3mConverts all anim frames to .png files and renames them to the correct format. (requires numbers in filenames)
        \033[0m
"""

EXAMPLE_MANIFEST = """Filetype: Flipper Animation Manifest
Version: 1

Name: example_anim
Min butthurt: 0
Max butthurt: 18
Min level: 1
Max level: 30
Weight: 8
"""

EXAMPLE_META = """Filetype: Flipper Animation
Version: 1
# More info on meta settings:
# https://flipper.wiki/tutorials/Animation_guide_meta/Meta_settings_guide/

Width: 128
Height: 64
Passive frames: 24
Active frames: 0
Frames order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
Active cycles: 0
Frame rate: 2
Duration: 3600
Active cooldown: 0

Bubble slots: 0
"""


try:
    VERSION = importlib.metadata.version("mntm-asset-packer")
except importlib.metadata.PackageNotFoundError:
    VERSION = "1.2.1 (standalone mode)"
    # this means the script is being used directly with python
    # instead of using the python package


def convert_to_bm(img: "Image.Image | pathlib.Path") -> bytes:
    """Converts an image to a bitmap."""
    if not isinstance(img, Image.Image):
        img = Image.open(img)

    with io.BytesIO() as output:
        img = img.convert("1")
        img = ImageOps.invert(img)
        img.save(output, format="XBM")
        xbm = output.getvalue()

    f = io.StringIO(xbm.decode().strip())
    data = f.read().strip().replace("\n", "").replace(" ", "").split("=")[1][:-1]
    data_str = data[1:-1].replace(",", " ").replace("0x", "")
    data_bin = bytearray.fromhex(data_str)

    # compressing the image
    data_encoded_str = heatshrink2.compress(data_bin, window_sz2=8, lookahead_sz2=4)
    data_enc = bytearray(data_encoded_str)
    data_enc = bytearray([len(data_enc) & 0xFF, len(data_enc) >> 8]) + data_enc

    # marking the image as compressed
    if len(data_enc) + 2 < len(data_bin) + 1:
        return b"\x01\x00" + data_enc
    return b"\x00" + data_bin


def convert_to_bmx(img: "Image.Image | pathlib.Path") -> bytes:
    """Converts an image to a bmx that contains image size info."""
    if not isinstance(img, Image.Image):
        img = Image.open(img)

    data = struct.pack("<II", *img.size)
    data += convert_to_bm(img)
    return data


def recover_from_bm(bm: "bytes | pathlib.Path", width: int, height: int) -> Image.Image:
    """Converts a bitmap back to a png (same as convert_to_bm but in reverse).

    The resulting png will not always be the same as the original image as some
    information is lost during the conversion.
    """
    if not isinstance(bm, bytes):
        bm = bm.read_bytes()

    if bm.startswith(b"\x01\x00"):
        data_dec = heatshrink2.decompress(bm[4:], window_sz2=8, lookahead_sz2=4)
    else:
        data_dec = bm[1:]

    img = Image.new("1", (width, height))

    pixels = []
    num_target_pixels = width * height
    for byte_val in data_dec:
        for i in range(8):
            if len(pixels) < num_target_pixels:
                pixels.append(1 - ((byte_val >> i) & 1))
            else:
                break
        if len(pixels) >= num_target_pixels:
            break

    img.putdata(pixels)

    return img


def recover_from_bmx(bmx: "bytes | pathlib.Path") -> Image.Image:
    """Converts a bmx back to a png (same as convert_to_bmx but in reverse)."""
    if not isinstance(bmx, bytes):
        bmx = bmx.read_bytes()

    width, height = struct.unpack("<II", bmx[:8])
    return recover_from_bm(bmx[8:], width, height)


def copy_file_as_lf(src: "pathlib.Path", dst: "pathlib.Path") -> None:
    """Copy file but replace Windows Line Endings with Unix Line Endings."""
    dst.write_bytes(src.read_bytes().replace(b"\r\n", b"\n"))


def pack_anim(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Packs an anim."""
    if not (src / "meta.txt").is_file():
        print(f'\033[31mNo meta.txt found in "{src.name}" anim.\033[0m')
        return
    if not any(re.match(r"frame_\d+\.(png|bm)", file.name) for file in src.iterdir()):
        print(
            f'\033[31mNo frames with the required format found in "{src.name}" anim.\033[0m',
        )
        try:
            input(
                "Press [Enter] to convert and rename the frames or [Ctrl+C] to cancel\033[0m",
            )
        except KeyboardInterrupt:
            sys.exit(0)
        print()
        convert_and_rename_frames(src, print)

    dst.mkdir(parents=True, exist_ok=True)
    for frame in src.iterdir():
        if not frame.is_file():
            continue
        if frame.name == "meta.txt":
            copy_file_as_lf(frame, dst / frame.name)
        elif frame.name.startswith("frame_"):
            if frame.suffix == ".png":
                (dst / frame.with_suffix(".bm").name).write_bytes(convert_to_bm(frame))
            elif frame.suffix == ".bm" and not (dst / frame.name).is_file():
                shutil.copyfile(frame, dst / frame.name)


def recover_anim(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Converts a bitmap to a png."""
    if not Path.exists(src):
        print(f'\033[31mError: "{src}" not found\033[0m')
        return
    if not any(re.match(r"frame_\d+.bm", file.name) for file in src.iterdir()):
        print(
            f'\033[31mNo frames with the required format found in "{src.name}" anim.\033[0m',
        )
        return

    dst.mkdir(parents=True, exist_ok=True)

    width = 128
    height = 64
    meta = src / "meta.txt"
    if Path.exists(meta):
        shutil.copyfile(meta, dst / meta.name)
        with Path.open(meta, encoding="utf-8") as f:
            for line in f:
                if line.startswith("Width:"):
                    width = int(line.split(":")[1].strip())
                elif line.startswith("Height:"):
                    height = int(line.split(":")[1].strip())
    else:
        print(f"meta.txt not found, assuming width={width}, height={height}")

    for file in src.iterdir():
        if file.is_file() and file.suffix == ".bm":
            img = recover_from_bm(file, width, height)
            img.save(dst / file.with_suffix(".png").name)


def pack_animated_icon(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Packs an animated ico."""
    if not (src / "frame_rate").is_file() and not (src / "meta").is_file():
        return
    dst.mkdir(parents=True, exist_ok=True)
    frame_count = 0
    frame_rate = None
    size = None
    files = [file for file in src.iterdir() if file.is_file()]
    for frame in sorted(files, key=lambda x: x.name):
        if not frame.is_file():
            continue
        if frame.name == "frame_rate":
            frame_rate = int(frame.read_text().strip())
        elif frame.name == "meta":
            shutil.copyfile(frame, dst / frame.name)
        else:
            dst_frame = dst / f"frame_{frame_count:02}.bm"
            if frame.suffix == ".png":
                if not size:
                    size = Image.open(frame).size
                dst_frame.write_bytes(convert_to_bm(frame))
                frame_count += 1
            elif frame.suffix == ".bm":
                if frame.with_suffix(".png") not in files:
                    shutil.copyfile(frame, dst_frame)
                    frame_count += 1
    if size is not None and frame_rate is not None:
        (dst / "meta").write_bytes(struct.pack("<IIII", *size, frame_rate, frame_count))


def recover_animated_icon(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Recovers an animated icon."""
    meta_file_path = src / "meta"

    if not meta_file_path.is_file():
        return

    unpacked_meta_data = None
    try:
        with Path.open(meta_file_path, "rb") as f:
            expected_bytes_count = struct.calcsize("<IIII")
            data_bytes = f.read(expected_bytes_count)
            if len(data_bytes) < expected_bytes_count:
                print(f"Error: Meta file '{meta_file_path}' is too short or corrupted.")
                return
            unpacked_meta_data = struct.unpack("<IIII", data_bytes)
    except struct.error:
        print(
            f"Error: Failed to unpack meta file '{meta_file_path}'. It might be corrupted.",
        )
        return
    except OSError as e:  # Catch file-related IO errors
        print(f"Error reading meta file '{meta_file_path}': {e}")
        return

    # unpacked_meta_data should be (width, height, frame_rate, frame_count)
    image_width = unpacked_meta_data[0]
    image_height = unpacked_meta_data[1]
    frame_rate_value = unpacked_meta_data[2]
    number_of_frames = unpacked_meta_data[3]

    dst.mkdir(parents=True, exist_ok=True)
    for i in range(number_of_frames):
        frame_bm_file_path = src / f"frame_{i:02}.bm"
        if not frame_bm_file_path.is_file():
            print(f"Warning: Frame file '{frame_bm_file_path}' not found. Skipping.")
            continue  # skip this frame if the .bm file is missing

        try:
            frame = recover_from_bm(frame_bm_file_path, image_width, image_height)
            frame.save(dst / f"frame_{i:02}.png")
        except (OSError, ValueError) as e:
            print(f"Error recovering or saving frame '{frame_bm_file_path}': {e}")
            continue  # skip to the next frame if an error occurs

    (dst / "frame_rate").write_text(str(frame_rate_value))


def pack_static_icon(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Packs a static icon."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.suffix == ".png":
        dst.with_suffix(".bmx").write_bytes(convert_to_bmx(src))
    elif src.suffix == ".bmx" and not dst.is_file():
        shutil.copyfile(src, dst)


def recover_static_icon(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Recovers a static icon."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.suffix == ".bmx":
        recover_from_bmx(src).save(dst.with_suffix(".png"))


def pack_font(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Packs a font."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.suffix == ".c":
        try:
            code = (
                src.read_bytes()
                .split(b' U8G2_FONT_SECTION("')[1]
                .split(b'") =')[1]
                .strip()
            )
        except IndexError:
            print(f'\033[31mError: "{src.name}" is not a valid font file.\033[0m')
            return
        font = b""
        for line in code.splitlines():
            if line.count(b'"') == 2:
                font += (
                    line[line.find(b'"') + 1 : line.rfind(b'"')]
                    .decode("unicode_escape")
                    .encode("latin_1")
                )
        font += b"\0"
        dst.with_suffix(".u8f").write_bytes(font)
    elif src.suffix == ".u8f":
        if not dst.is_file():
            shutil.copyfile(src, dst)


# recover font is not implemented


def convert_and_rename_frames(
    directory: "str | pathlib.Path", logger: typing.Callable
) -> None:
    """Converts all frames to png and renames them "frame_N.png".

    (requires the image name to contain the frame number)
    """
    already_formatted = True
    for file in directory.iterdir():
        if (
            file.is_file()
            and file.suffix in (".jpg", ".jpeg", ".png")
            and not re.match(r"frame_\d+.png", file.name)
        ):
            already_formatted = False
            break
    if already_formatted:
        logger(f'"{directory.name}" anim is formatted')
        return

    try:
        print(
            f'\033[31mThis will convert all frames for the "{directory.name}" anim to png and rename them.\nThis action is irreversible, make sure to back up your files if needed.\n\033[0m',
        )
        input("Press [Enter] if you wish to continue or [Ctrl+C] to cancel")
    except KeyboardInterrupt:
        sys.exit(0)
    print()
    index = 1

    for file in sorted(directory.iterdir(), key=lambda x: x.name):
        if file.is_file() and file.suffix in (".jpg", ".jpeg", ".png"):
            filename = file.stem
            if re.search(r"\d+", filename):
                filename = f"frame_{index}.png"
                index += 1
            else:
                filename = f"{filename}.png"

            img = Image.open(file)
            img.save(directory / filename)
            file.unlink()


def convert_and_rename_frames_for_all_anims(
    directory_for_anims: "str | pathlib.Path",
    logger: typing.Callable,
) -> None:
    """Formats all anim frames correctly.

    Converts all frames to png and renames them "frame_N.png for all anims in the given anim folder.
    (requires the image name to contain the frame number)
    """
    for anim in directory_for_anims.iterdir():
        if anim.is_dir():
            convert_and_rename_frames(anim, logger)


def pack_specific(
    asset_pack_path: "str | pathlib.Path",
    output_directory: "str | pathlib.Path",
    logger: typing.Callable,
) -> None:
    """Packs a specific asset pack."""
    asset_pack_path = pathlib.Path(asset_pack_path)
    output_directory = pathlib.Path(output_directory)

    if not asset_pack_path.is_dir():
        logger(f"\033[31mError: '{asset_pack_path}' is not a directory\033[0m")
        return

    if not any(
        (asset_pack_path / d).is_dir() for d in ["Anims", "Icons", "Fonts", "Passport"]
    ):
        logger(
            f"\033[37mInfo: '{asset_pack_path}' is not a valid asset pack (Make sure it contains an 'Anims', 'Icons', 'Fonts' or 'Passport' directory), skipping.\033[0m"
        )
        return

    logger(f"Packing '\033[3m{asset_pack_path.name}\033[0m'")

    packed = output_directory / asset_pack_path.name

    if packed.exists():
        try:
            if packed.is_dir() and not packed.is_symlink():
                shutil.rmtree(packed, ignore_errors=True)
            else:
                packed.unlink()
        except (OSError, shutil.Error):
            logger(f"\033[31mError: Failed to remove existing pack: '{packed}'\033[0m")
            return

    # packing anims
    if (asset_pack_path / "Anims/manifest.txt").exists():
        (packed / "Anims").mkdir(
            parents=True,
            exist_ok=True,
        )  # ensure that the "Anims" directory exists
        copy_file_as_lf(
            asset_pack_path / "Anims/manifest.txt",
            packed / "Anims/manifest.txt",
        )
        manifest = (asset_pack_path / "Anims/manifest.txt").read_bytes()

        # Find all the anims in the manifest
        for anim in re.finditer(rb"Name: (.*)", manifest):
            anim_name = (
                anim.group(1)
                .decode()
                .replace("\\", "/")
                .replace("/", os.sep)
                .replace("\r", "\n")
                .strip()
            )
            logger(
                f"Compiling anim '\033[3m{anim_name}\033[0m' for '\033[3m{asset_pack_path.name}\033[0m'",
            )
            pack_anim(
                asset_pack_path / "Anims" / anim_name, packed / "Anims" / anim_name
            )

    # packing icons
    if (asset_pack_path / "Icons").is_dir():
        for icons in (asset_pack_path / "Icons").iterdir():
            if not icons.is_dir() or icons.name.startswith("."):
                continue
            for icon in icons.iterdir():
                if icon.name.startswith("."):
                    continue
                if icon.is_dir():
                    logger(
                        f"Compiling icon for pack '{asset_pack_path.name}': {icons.name}/{icon.name}",
                    )
                    pack_animated_icon(icon, packed / "Icons" / icons.name / icon.name)
                elif icon.is_file() and icon.suffix in (".png", ".bmx"):
                    logger(
                        f"Compiling icon for pack '{asset_pack_path.name}': {icons.name}/{icon.name}",
                    )
                    pack_static_icon(icon, packed / "Icons" / icons.name / icon.name)

    # packing fonts
    if (asset_pack_path / "Fonts").is_dir():
        for font in (asset_pack_path / "Fonts").iterdir():
            if (
                not font.is_file()
                or font.name.startswith(".")
                or font.suffix not in (".c", ".u8f")
            ):
                continue
            logger(f"Compiling font for pack '{asset_pack_path.name}': {font.name}")
            pack_font(font, packed / "Fonts" / font.name)

    logger(f"\033[32mFinished packing '\033[3m{asset_pack_path.name}\033[23m'\033[0m")
    logger(f"Saved to: '\033[33m{packed}\033[0m'")


def recover_specific(
    asset_pack_path: "str | pathlib.Path",
    output_directory: "str | pathlib.Path",
    logger: typing.Callable,
) -> None:
    """Recovers a specific asset pack."""
    asset_pack_path = pathlib.Path(asset_pack_path)
    output_directory = pathlib.Path(output_directory)

    if not asset_pack_path.is_dir():
        logger(f"\033[31mError: '{asset_pack_path}' is not a directory\033[0m")
        return

    if not any(
        (asset_pack_path / d).is_dir() for d in ["Anims", "Icons", "Fonts", "Passport"]
    ):
        logger(
            f"\033[37mInfo: '{asset_pack_path}' is not a valid asset pack (Make sure it contains an 'Anims', 'Icons', 'Fonts' or 'Passport' directory), skipping.\033[0m"
        )
        return

    logger(f"Recovering '\033[3m{asset_pack_path.name}\033[0m'")
    recovered = output_directory / asset_pack_path.name

    if recovered.exists():
        try:
            if recovered.is_dir():
                shutil.rmtree(recovered, ignore_errors=True)
            else:
                recovered.unlink()
        except (OSError, shutil.Error):
            logger(
                f"\033[31mError: Failed to remove existing pack: '{recovered}'\033[0m"
            )

    # recovering anims
    if (asset_pack_path / "Anims").is_dir():
        (recovered / "Anims").mkdir(
            parents=True,
            exist_ok=True,
        )  # ensure that the "Anims" directory exists

        # copy the manifest if it exists
        if (asset_pack_path / "Anims/manifest.txt").exists():
            shutil.copyfile(
                asset_pack_path / "Anims/manifest.txt",
                recovered / "Anims/manifest.txt",
            )

        # recover all the anims in the Anims directory
        for anim in (asset_pack_path / "Anims").iterdir():
            if not anim.is_dir() or anim.name.startswith("."):
                continue
            logger(
                f"Recovering anim '\033[3m{anim}\033[0m' for '\033[3m{asset_pack_path.name}\033[0m'"
            )
            recover_anim(anim, recovered / "Anims" / anim.name)

    # recovering icons
    if (asset_pack_path / "Icons").is_dir():
        for icons in (asset_pack_path / "Icons").iterdir():
            if not icons.is_dir() or icons.name.startswith("."):
                continue
            for icon in icons.iterdir():
                if icon.name.startswith("."):
                    continue
                if icon.is_dir():
                    logger(
                        f"Recovering icon for pack '{asset_pack_path.name}': {icons.name}/{icon.name}"
                    )
                    recover_animated_icon(
                        icon, recovered / "Icons" / icons.name / icon.name
                    )
                elif icon.is_file() and icon.suffix == ".bmx":
                    logger(
                        f"Recovering icon for pack '{asset_pack_path.name}': {icons.name}/{icon.name}"
                    )
                    recover_static_icon(
                        icon, recovered / "Icons" / icons.name / icon.name
                    )

    # recovering fonts
    if (asset_pack_path / "Fonts").is_dir():
        logger("Fonts recovery not implemented yet")

    logger(
        f"\033[32mFinished recovering '\033[3m{asset_pack_path.name}\033[23m'\033[0m"
    )
    logger(f"Saved to: '\033[33m{recovered}\033[0m'")


def pack_all_asset_packs(
    source_directory: "str | pathlib.Path",
    output_directory: "str | pathlib.Path",
    logger: typing.Callable,
) -> None:
    """Packs all asset packs in the source directory."""
    try:
        print(
            "This will pack all asset packs in the current directory."
            "The resulting asset packs will be saved to './asset_packs'\n",
        )
        input("Press [Enter] if you wish to continue or [Ctrl+C] to cancel")
    except KeyboardInterrupt:
        sys.exit(0)
    print()

    source_directory = pathlib.Path(source_directory)
    output_directory = pathlib.Path(output_directory)

    for source in source_directory.iterdir():
        # Skip folders that are definitely not meant to be packed
        if source == output_directory:
            continue
        if (
            not source.is_dir()
            or source.name.startswith(".")
            or source.name in ("venv", "recovered")
        ):
            continue

        pack_specific(source, output_directory, logger)


def recover_all_asset_packs(
    source_directory: "str | pathlib.Path",
    output_directory: "str | pathlib.Path",
    logger: typing.Callable,
) -> None:
    """Recovers all asset packs in the source directory."""
    try:
        print(
            "This will recover all asset packs in the current directory."
            "The resulting asset packs will be saved to './recovered'\n",
        )
        input("Press [Enter] if you wish to continue or [Ctrl+C] to cancel")
    except KeyboardInterrupt:
        sys.exit(0)
    print()

    source_directory = pathlib.Path(source_directory)
    output_directory = pathlib.Path(output_directory)

    for source in source_directory.iterdir():
        # Skip folders that are definitely not meant to be recovered
        if source == output_directory:
            continue
        if (
            not source.is_dir()
            or source.name.startswith(".")
            or source.name in ("venv", "recovered")
        ):
            continue

        recover_specific(source, output_directory, logger)


def create_asset_pack(
    asset_pack_name: str,
    output_directory: "str | pathlib.Path",
    logger: typing.Callable,
) -> None:
    """Creates the file structure for an asset pack."""
    if not isinstance(output_directory, pathlib.Path):
        output_directory = pathlib.Path(output_directory)

    # check for illegal characters
    if not re.match(r"^[a-zA-Z0-9_\- ]+$", asset_pack_name):
        logger(f"\033[31mError: '{asset_pack_name}' contains illegal characters\033[0m")
        return

    if (output_directory / asset_pack_name).exists():
        logger(
            f"\033[31mError: {output_directory / asset_pack_name} already exists\033[0m"
        )
        return

    generate_example_files = (
        input("Create example for anim structure? (y/N) : ").lower() == "y"
    )

    (output_directory / asset_pack_name / "Anims").mkdir(parents=True)
    (output_directory / asset_pack_name / "Icons").mkdir(parents=True)
    (output_directory / asset_pack_name / "Fonts").mkdir(parents=True)
    (output_directory / asset_pack_name / "Passport").mkdir(parents=True)
    # creating "manifest.txt" file
    if generate_example_files:
        (output_directory / asset_pack_name / "Anims" / "manifest.txt").touch()
        with Path.open(
            output_directory / asset_pack_name / "Anims" / "manifest.txt",
            "w",
            encoding="utf-8",
        ) as f:
            f.write(EXAMPLE_MANIFEST)
        (output_directory / asset_pack_name / "Anims" / "example_anim").mkdir(
            parents=True,
        )
        (
            output_directory / asset_pack_name / "Anims" / "example_anim" / "meta.txt"
        ).touch()
        with Path.open(
            output_directory / asset_pack_name / "Anims" / "example_anim" / "meta.txt",
            "w",
            encoding="utf-8",
        ) as f:
            f.write(EXAMPLE_META)

    logger(f"Created asset pack '{asset_pack_name}' in '{output_directory}'")


def main() -> None:
    """Main function."""
    if len(sys.argv) <= 1:
        # If no arguments are provided, pack all
        here = pathlib.Path.cwd()
        start = time.perf_counter()
        pack_all_asset_packs(here, here / "asset_packs", logger=print)
        end = time.perf_counter()
        print(f"\nFinished in {round(end - start, 2)}s\n")
        return

    # if the first argument is a directory, pack that directory
    if len(sys.argv) == 2 and pathlib.Path(sys.argv[1]).is_dir():
        pack_specific(sys.argv[1], pathlib.Path.cwd() / "asset_packs", logger=print)
        return

    match sys.argv[1]:
        case "version" | "--version" | "-v":
            print(f"mntm-asset-packer {VERSION}")

        case "help" | "-h" | "--help":
            print(HELP_MESSAGE)
            return

        case "create":
            if len(sys.argv) >= 3:
                asset_pack_name = " ".join(sys.argv[2:])
                create_asset_pack(asset_pack_name, pathlib.Path.cwd(), logger=print)
                return
            print(HELP_MESSAGE)

        case "pack" | "compile":
            if len(sys.argv) == 3:
                here = pathlib.Path.cwd()
                start = time.perf_counter()

                if sys.argv[2] == "all":
                    pack_all_asset_packs(
                        here,
                        here / "asset_packs",
                        logger=print,
                    )
                else:
                    pack_specific(
                        sys.argv[2],
                        pathlib.Path.cwd() / "asset_packs",
                        logger=print,
                    )

                end = time.perf_counter()
                print(f"\nFinished in {round(end - start, 2)}s\n")
                return
            print(HELP_MESSAGE)

        case "recover":
            if len(sys.argv) == 3:
                here = pathlib.Path.cwd()
                start = time.perf_counter()

                if sys.argv[2] == "all":
                    recover_all_asset_packs(
                        here / "asset_packs",
                        here / "recovered",
                        logger=print,
                    )
                else:
                    recover_specific(
                        sys.argv[2],
                        pathlib.Path.cwd() / "recovered",
                        logger=print,
                    )

                end = time.perf_counter()
                print(f"Finished in {round(end - start, 2)}s")
                return
            print(HELP_MESSAGE)

        case "convert":
            if len(sys.argv) == 3:
                convert_and_rename_frames_for_all_anims(
                    pathlib.Path(sys.argv[2]) / "Anims", logger=print
                )
                return
            print(HELP_MESSAGE)

        case _:
            print(HELP_MESSAGE)


if __name__ == "__main__":
    main()
