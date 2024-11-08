#!/usr/bin/env python3
import pathlib

root = pathlib.Path(__file__).parent / ".."
icons = root / "assets/icons"


def count_icon_usages(name: str):
    count = 0
    name = name.encode()
    # EXTREMELY wasteful, but who cares
    for dir in ("applications", "furi", "lib", "targets"):
        for filetype in (".c", ".cpp", ".h", ".fam"):
            for file in (root / dir).glob(f"**/*{filetype}"):
                try:
                    if name in file.read_bytes():
                        count += 1
                except Exception:
                    print(f"Failed to read {file}")
    return count


if __name__ == "__main__":
    counts = {}

    for category in icons.iterdir():
        if not category.is_dir():
            continue
        for icon in category.iterdir():
            if icon.is_dir() and (icon / "frame_rate").is_file():
                name = "A_" + icon.name.replace("-", "_")
            elif icon.is_file() and icon.suffix == ".png":
                name = "I_" + "_".join(icon.name.split(".")[:-1]).replace("-", "_")
            else:
                continue
            counts[name[2:]] = count_icon_usages(name)

    for name, count in sorted(counts.items(), key=lambda x: x[1], reverse=True):
        print(f"{name} used {count} times")
