#!/usr/bin/env python3
"""Generate AURELIA OS artwork without external image assets.

The wallpapers are original layered mountain scenes in the AURELIA palette.
Run from any host with Pillow installed:

    python distro/scripts/generate-artwork.py
"""
from __future__ import annotations

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
INCLUDE = ROOT / "config" / "includes.chroot"
WIDTH, HEIGHT = 3840, 2160
GOLD = (214, 184, 120)
IVORY = (244, 240, 231)
MIDNIGHT = (9, 10, 14)
BLUE = (73, 183, 228)
DEEP_BLUE = (7, 22, 33)
FONT = ASSETS / "fonts" / "PlayfairDisplay.ttf"


def gradient(top: tuple[int, int, int], bottom: tuple[int, int, int]) -> Image.Image:
    column = Image.new("RGB", (1, HEIGHT))
    pixels = column.load()
    for y in range(HEIGHT):
        t = y / max(1, HEIGHT - 1)
        t = t * t * (3.0 - 2.0 * t)
        pixels[0, y] = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
    return column.resize((WIDTH, HEIGHT), Image.Resampling.BILINEAR).convert("RGBA")


def smoothstep(t: float) -> float:
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def ridge_value(x: int, base: int, amplitude: int, points: list[tuple[int, int]]) -> int:
    pos = x / max(1, WIDTH - 1) * 1000.0
    for i in range(len(points) - 1):
        x0, y0 = points[i]
        x1, y1 = points[i + 1]
        if pos <= x1 or i == len(points) - 2:
            span = max(1, x1 - x0)
            t = smoothstep((pos - x0) / span)
            height = y0 + (y1 - y0) * t
            return int(base - amplitude * height / 100.0)
    return base


def mountain_layer(
    image: Image.Image,
    points: list[tuple[int, int]],
    base: int,
    amplitude: int,
    color: tuple[int, int, int],
    rim: tuple[int, int, int],
    rim_alpha: int,
) -> None:
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    ridge = [(x, ridge_value(x, base, amplitude, points)) for x in range(WIDTH)]
    for x, y in ridge:
        draw.line((x, y, x, HEIGHT), fill=color + (255,), width=1)
    draw.line(ridge, fill=rim + (rim_alpha,), width=2, joint="curve")
    image.alpha_composite(overlay)


def add_pines(image: Image.Image, side: str) -> None:
    """Frame the scene with original layered pine silhouettes."""
    random.seed(2604 if side == "left" else 2605)
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for index in range(18):
        if side == "left":
            x = random.randint(-30, int(WIDTH * 0.18))
        else:
            x = random.randint(int(WIDTH * 0.82), WIDTH + 30)
        base = HEIGHT + random.randint(-30, 80)
        height = random.randint(int(HEIGHT * 0.20), int(HEIGHT * 0.58))
        half = random.randint(22, 76)
        color = (3, 5, 9) if index % 4 else (7, 9, 15)
        draw.line((x, base, x, base - height * 0.42), fill=color + (255,),
                  width=max(3, half // 5))
        for layer in range(7):
            y = base - height * (0.18 + 0.115 * layer)
            span = half * (1.0 - 0.105 * layer)
            if span < 4:
                break
            apex = y - height * 0.18
            draw.polygon([(x, apex), (x - span, y), (x + span, y)],
                         fill=color + (238,))
            draw.line([(x, apex), (x - span, y)], fill=(214, 184, 120, 14), width=1)
    image.alpha_composite(overlay)


def add_stars(image: Image.Image, count: int = 190) -> None:
    random.seed(26042026)
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for i in range(count):
        x = random.randrange(WIDTH)
        y = random.randrange(int(HEIGHT * 0.56))
        radius = 1 if i % 19 else 2
        alpha = random.randint(45, 175)
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=(232, 226, 212, alpha))
    image.alpha_composite(overlay)


def add_sun(image: Image.Image, x: int, y: int, radius: int, color: tuple[int, int, int]) -> None:
    glow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(glow)
    for step in range(18, 0, -1):
        r = int(radius * (1.0 + step * 0.34))
        alpha = max(2, 9 - step // 3)
        draw.ellipse((x - r, y - r, x + r, y + r), fill=color + (alpha,))
    glow = glow.filter(ImageFilter.GaussianBlur(radius * 0.22))
    image.alpha_composite(glow)
    disc = Image.new("RGBA", image.size, (0, 0, 0, 0))
    ddraw = ImageDraw.Draw(disc)
    ddraw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=(242, 220, 168, 84))
    disc = disc.filter(ImageFilter.GaussianBlur(24))
    image.alpha_composite(disc)


def dark_wallpaper() -> Image.Image:
    image = gradient((6, 8, 15), (27, 21, 16))
    add_stars(image)
    add_sun(image, int(WIDTH * 0.72), int(HEIGHT * 0.39), 120, GOLD)

    far = [(0, 22), (120, 46), (210, 18), (330, 72), (450, 35), (560, 88),
           (690, 30), (820, 62), (920, 26), (1000, 44)]
    mid = [(0, 30), (160, 20), (280, 70), (380, 32), (520, 85),
           (640, 38), (760, 92), (860, 24), (1000, 50)]
    near = [(0, 40), (140, 62), (240, 20), (370, 52), (500, 25),
            (620, 78), (740, 22), (880, 58), (1000, 34)]

    haze = Image.new("RGBA", image.size, (0, 0, 0, 0))
    hdraw = ImageDraw.Draw(haze)
    for i in range(120):
        y = int(HEIGHT * 0.41) + i * 2
        if y >= HEIGHT:
            break
        alpha = i // 4 if i < 45 else (120 - i) // 6
        hdraw.line((0, y, WIDTH, y), fill=(214, 184, 120, max(0, alpha)))
    image.alpha_composite(haze)

    mountain_layer(image, far, int(HEIGHT * 0.62), int(HEIGHT * 0.20),
                   (27, 29, 39), GOLD, 34)
    mist = Image.new("RGBA", image.size, (0, 0, 0, 0))
    mdraw = ImageDraw.Draw(mist)
    for i in range(42):
        y = int(HEIGHT * 0.57) + i * 4
        mdraw.line((0, y, WIDTH, y), fill=(224, 212, 189, max(1, 9 - i // 6)))
    image.alpha_composite(mist)

    mountain_layer(image, mid, int(HEIGHT * 0.74), int(HEIGHT * 0.24),
                   (16, 19, 27), (182, 154, 104), 42)
    mountain_layer(image, near, int(HEIGHT * 0.91), int(HEIGHT * 0.17),
                   (6, 8, 13), GOLD, 52)

    # A low layer of mist gives the scene depth without hiding the taskbar.
    fog = Image.new("RGBA", image.size, (0, 0, 0, 0))
    fdraw = ImageDraw.Draw(fog)
    for i in range(28):
        y = int(HEIGHT * 0.84) + i * 3
        fdraw.line((0, y, WIDTH, y), fill=(201, 189, 168, max(1, 4 - i // 8)))
    image.alpha_composite(fog)
    add_pines(image, "left")
    add_pines(image, "right")
    return image


def light_wallpaper() -> Image.Image:
    image = gradient((250, 247, 240), (211, 203, 190))
    add_sun(image, int(WIDTH * 0.68), int(HEIGHT * 0.35), 170, (217, 189, 142))

    far = [(0, 15), (180, 44), (340, 24), (470, 62), (630, 34),
           (760, 74), (900, 30), (1000, 54)]
    mid = [(0, 28), (180, 54), (350, 30), (520, 68), (700, 34),
           (840, 76), (1000, 42)]
    near = [(0, 26), (160, 55), (300, 24), (470, 58), (650, 30),
            (820, 62), (1000, 38)]

    mountain_layer(image, far, int(HEIGHT * 0.62), int(HEIGHT * 0.18),
                   (169, 176, 186), (224, 204, 169), 70)
    mountain_layer(image, mid, int(HEIGHT * 0.76), int(HEIGHT * 0.22),
                   (130, 140, 152), (214, 188, 149), 76)
    mountain_layer(image, near, int(HEIGHT * 0.93), int(HEIGHT * 0.16),
                   (96, 106, 121), (204, 176, 138), 82)
    mist = Image.new("RGBA", image.size, (0, 0, 0, 0))
    mdraw = ImageDraw.Draw(mist)
    for i in range(48):
        y = int(HEIGHT * 0.665) + i * 4
        mdraw.line((0, y, WIDTH, y), fill=(250, 248, 242, max(1, 12 - i // 5)))
    image.alpha_composite(mist)
    return image


def logo_image(size: int = 1024) -> Image.Image:
    """AURELIA's hexagonal network mark, drawn from primitives."""
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    center = size / 2
    outer_radius = size * 0.355
    inner_radius = size * 0.205

    def hexagon(radius: float) -> list[tuple[float, float]]:
        points = []
        for index in range(6):
            angle = math.radians(-90 + index * 60)
            points.append((center + radius * math.cos(angle),
                           center + radius * math.sin(angle)))
        return points

    outer = hexagon(outer_radius)
    inner = hexagon(inner_radius)
    outer_width = max(5, size // 82)
    inner_width = max(3, size // 145)
    draw.line(outer + [outer[0]], fill=BLUE + (255,), width=outer_width,
              joint="curve")
    draw.line(inner + [inner[0]], fill=(117, 213, 242, 235), width=inner_width,
              joint="curve")
    for index in range(6):
        draw.line((outer[index][0], outer[index][1], inner[index][0], inner[index][1]),
                  fill=(61, 170, 216, 235), width=max(2, size // 190))
        for point, radius in ((outer[index], size * 0.038), (inner[index], size * 0.021)):
            x, y = point
            draw.ellipse((x - radius, y - radius, x + radius, y + radius),
                         fill=DEEP_BLUE + (255,), outline=(128, 225, 250, 255),
                         width=max(2, size // 210))
            draw.ellipse((x - radius * 0.42, y - radius * 0.42,
                          x + radius * 0.42, y + radius * 0.42),
                         fill=(196, 243, 255, 245))
    return image


def novair_logo_image(size: int = 1024) -> Image.Image:
    """NOVAIR's own orbital N mark, distinct from the AURELIA monogram."""
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    inset = int(size * 0.035)
    radius = int(size * 0.22)
    draw.rounded_rectangle(
        (inset, inset, size - inset, size - inset),
        radius=radius,
        fill=DEEP_BLUE + (255,),
        outline=(25, 111, 151, 255),
        width=max(2, size // 160),
    )
    ring_box = (int(size * 0.17), int(size * 0.17), int(size * 0.83), int(size * 0.83))
    draw.ellipse(ring_box, outline=BLUE + (165,), width=max(2, size // 220))
    draw.arc(ring_box, 200, 340, fill=(190, 239, 255, 235), width=max(3, size // 90))
    for x, y in ((size * 0.30, size * 0.28), (size * 0.73, size * 0.38),
                 (size * 0.64, size * 0.74), (size * 0.27, size * 0.63)):
        r = max(6, size // 52)
        draw.ellipse((x - r, y - r, x + r, y + r), fill=BLUE + (255,))
        draw.ellipse((x - r // 2, y - r // 2, x + r // 2, y + r // 2),
                     fill=(226, 250, 255, 235))
    font = ImageFont.truetype(str(FONT), int(size * 0.40))
    bbox = draw.textbbox((0, 0), "N", font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text(((size - tw) / 2 - bbox[0], (size - th) / 2 - bbox[1] - size * 0.01), "N",
              font=font, fill=(238, 251, 255, 255))
    return image


def center_crop(image: Image.Image, width: int, height: int) -> Image.Image:
    ratio = max(width / image.width, height / image.height)
    resized = image.resize((int(image.width * ratio + 0.5), int(image.height * ratio + 0.5)),
                           Image.Resampling.LANCZOS)
    left = (resized.width - width) // 2
    top = (resized.height - height) // 2
    return resized.crop((left, top, left + width, top + height))


def save(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, "PNG", optimize=True)
    print("artwork:", path.relative_to(ROOT))


def install_artwork(wall_dark: Image.Image, wall_light: Image.Image) -> None:
    preview = center_crop(wall_dark, 1200, 675)
    save(wall_dark, ASSETS / "wallpapers" / "aurelia-solstice.png")
    save(wall_light, ASSETS / "wallpapers" / "aurelia-ivory.png")
    save(preview, ASSETS / "preview" / "aurelia-preview.png")

    logo = logo_image()
    save(logo, ASSETS / "branding" / "aurelia-logo.png")
    save(logo, INCLUDE / "usr/share/icons/hicolor/512x512/apps/aurelia-logo.png")
    save(logo, INCLUDE / "etc/calamares/branding/aurelia/logo.png")

    novair = novair_logo_image()
    save(novair, ASSETS / "branding" / "novair-logo.png")
    for icon_size in (512, 256, 48):
        save(novair.resize((icon_size, icon_size), Image.Resampling.LANCZOS),
             INCLUDE / f"usr/share/icons/hicolor/{icon_size}x{icon_size}/apps/novair.png")

    save(wall_dark, INCLUDE / "usr/share/wallpapers/AureliaSolstice/contents/images/3840x2160.png")
    save(wall_light, INCLUDE / "usr/share/wallpapers/AureliaIvory/contents/images/3840x2160.png")
    save(center_crop(wall_dark, 1920, 1080), INCLUDE / "usr/share/sddm/themes/aurelia/background.png")

    installer = center_crop(wall_dark, 1920, 1080).convert("RGBA")
    overlay = Image.new("RGBA", installer.size, (5, 6, 10, 92))
    installer = Image.alpha_composite(installer, overlay)
    save(installer, INCLUDE / "etc/calamares/branding/aurelia/background.png")

    grub = center_crop(wall_dark, 1920, 1080).convert("RGBA")
    grub = Image.alpha_composite(grub, Image.new("RGBA", grub.size, (5, 6, 10, 72)))
    save(grub, INCLUDE / "boot/grub/themes/aurelia/background.png")


def main() -> None:
    dark = dark_wallpaper()
    light = light_wallpaper()
    install_artwork(dark, light)
    print("artwork OK")


if __name__ == "__main__":
    main()
