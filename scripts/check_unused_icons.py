#!/usr/bin/env python3
import pathlib

root = pathlib.Path(__file__).parent / ".."
icons = root / "assets/icons"

def source_dir_uses_icon(dir: str, name: str):
    count = 0
    name = name.encode()
    for file in (root / dir).glob("**/*.c"):
        try:
            if name in file.read_bytes():
                count += 1
        except Exception:
            print(f"Faield to read {file}")
    return count

for category in icons.iterdir():
    if not category.is_dir():
        continue
    for icon in category.iterdir():
        if icon.is_dir() and (icon / "frame_rate").is_file():
            name = "&A_" + icon.name.replace("-", "_")
        else:
            name = "&I_" + "_".join(icon.name.split(".")[:-1]).replace("-", "_")
        count = 0
        for dir in ("applications", "furi", "lib", "targets"):
            count += source_dir_uses_icon(dir, name)
        print(f"{name} used {count} times")
        if count == 0:
            print(f"====== {name} is not used! ======")
