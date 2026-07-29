#!/usr/bin/env python3
"""Generate deterministic 1-bit Level99 source assets for the firmware pipeline."""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]

FONT = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "010", "010", "010"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
    "A": ("010", "101", "111", "101", "101"),
    "C": ("111", "100", "100", "100", "111"),
    "E": ("111", "100", "110", "100", "111"),
    "F": ("111", "100", "110", "100", "100"),
    "L": ("100", "100", "100", "100", "111"),
    "V": ("101", "101", "101", "101", "010"),
}


def text_size(text: str, scale: int) -> tuple[int, int]:
    return ((4 * len(text) - 1) * scale, 5 * scale)


def draw_text(
    draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, scale: int = 1
) -> None:
    x, y = xy
    for char in text:
        glyph = FONT[char]
        for row, bits in enumerate(glyph):
            for col, bit in enumerate(bits):
                if bit == "1":
                    draw.rectangle(
                        (
                            x + col * scale,
                            y + row * scale,
                            x + (col + 1) * scale - 1,
                            y + (row + 1) * scale - 1,
                        ),
                        fill=0,
                    )
        x += 4 * scale


def centered_text(
    draw: ImageDraw.ImageDraw, width: int, y: int, text: str, scale: int
) -> None:
    text_width, _ = text_size(text, scale)
    draw_text(draw, ((width - text_width) // 2, y), text, scale)


def save(image: Image.Image, relative: str) -> None:
    path = ROOT / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    image.convert("1").save(path, optimize=True)


def splash() -> None:
    image = Image.new("1", (128, 64), 1)
    draw = ImageDraw.Draw(image)
    draw.rectangle((1, 1, 126, 62), outline=0)
    draw.rectangle((4, 4, 123, 59), outline=0)
    centered_text(draw, 128, 15, "LEVEL99", 4)
    centered_text(draw, 128, 44, "L99", 2)
    save(image, "assets/slideshow/firstboot/frame_00.png")


def update_logo() -> None:
    image = Image.new("1", (62, 15), 1)
    draw = ImageDraw.Draw(image)
    centered_text(draw, 62, 2, "LEVEL99", 2)
    save(image, "assets/icons/Update/Updating_Logo_62x15.png")


def main_menu() -> None:
    image = Image.new("1", (14, 14), 1)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 13, 13), outline=0)
    draw_text(draw, (1, 4), "L", 1)
    draw_text(draw, (5, 4), "99", 1)
    for path in (ROOT / "assets/icons/MainMenu/Momentum_14").glob("frame_*.png"):
        image.save(path, optimize=True)


def control_center() -> None:
    image = Image.new("1", (16, 16), 1)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 15, 15), outline=0)
    draw_text(draw, (1, 5), "L", 1)
    draw_text(draw, (6, 5), "99", 1)
    save(image, "assets/icons/ControlCenter/CC_Momentum_16x16.png")


def can_icon() -> None:
    image = Image.new("1", (10, 10), 1)
    draw = ImageDraw.Draw(image)
    draw.rectangle((3, 3, 6, 6), fill=0)
    for x, y in ((1, 1), (8, 1), (1, 8), (8, 8)):
        draw.point((x, y), fill=0)
    draw.line((1, 1, 3, 3), fill=0)
    draw.line((8, 1, 6, 3), fill=0)
    draw.line((1, 8, 3, 6), fill=0)
    draw.line((8, 8, 6, 6), fill=0)
    draw.point((4, 4), fill=1)
    draw.point((5, 5), fill=1)
    save(image, "applications_user/level99_can/level99_can.png")


if __name__ == "__main__":
    splash()
    update_logo()
    main_menu()
    control_center()
    can_icon()
