import math
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
SIZE = 1024
SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
PLAYER_COLORS = [(0, 142, 224), (228, 38, 54), (46, 176, 72), (250, 196, 0)]


def rounded_square(draw, box, radius, fill):
    draw.rounded_rectangle(box, radius=radius, fill=fill)


def background():
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    gradient = Image.new("RGBA", (SIZE, SIZE))
    top = (255, 92, 168)
    bottom = (214, 24, 120)
    pixels = gradient.load()
    for y in range(SIZE):
        t = y / (SIZE - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
        for x in range(SIZE):
            pixels[x, y] = color
    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle((32, 32, SIZE - 32, SIZE - 32), radius=220, fill=255)
    image.paste(gradient, (0, 0), mask)
    shine = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    ImageDraw.Draw(shine).ellipse((-200, -520, SIZE + 200, 420), fill=(255, 255, 255, 46))
    shine_mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(shine_mask).rounded_rectangle((32, 32, SIZE - 32, SIZE - 32), radius=220, fill=255)
    image.paste(Image.alpha_composite(image, shine), (0, 0), shine_mask)
    return image


def confetti(image):
    random.seed(7)
    layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    spots = [(200, 190), (820, 230), (170, 760), (860, 800), (500, 130), (140, 470), (880, 520), (560, 900), (300, 880), (760, 110)]
    for index, (x, y) in enumerate(spots):
        color = PLAYER_COLORS[index % len(PLAYER_COLORS)] + (255,)
        angle = random.uniform(0, math.pi)
        length = 70
        width = 26
        dx = math.cos(angle) * length / 2
        dy = math.sin(angle) * length / 2
        nx = -math.sin(angle) * width / 2
        ny = math.cos(angle) * width / 2
        draw.polygon([(x - dx + nx, y - dy + ny), (x + dx + nx, y + dy + ny), (x + dx - nx, y + dy - ny), (x - dx - nx, y - dy - ny)], fill=color)
    return Image.alpha_composite(image, layer)


def die(image, scale=1.0, pips=((150, 150), (410, 150), (280, 270), (150, 390), (410, 390)), pip=46, shadow_blur=24):
    face = Image.new("RGBA", (560, 560), (0, 0, 0, 0))
    draw = ImageDraw.Draw(face)
    rounded_square(draw, (20, 20, 540, 540), 120, (255, 255, 255, 255))
    rounded_square(draw, (20, 470, 540, 540), 60, (228, 228, 236, 255))
    rounded_square(draw, (20, 20, 540, 500), 120, (255, 255, 255, 255))
    for cx, cy in pips:
        draw.ellipse((cx - pip, cy - pip, cx + pip, cy + pip), fill=(34, 34, 48, 255))
    if scale != 1.0:
        face = face.resize((int(560 * scale), int(560 * scale)), Image.LANCZOS)
    rotated = face.rotate(-14, resample=Image.BICUBIC, expand=True)
    shadow = Image.new("RGBA", rotated.size, (0, 0, 0, 0))
    shadow.paste((90, 0, 40, 110), (0, 0), rotated.split()[3])
    shadow = shadow.filter(ImageFilter.GaussianBlur(shadow_blur))
    x = (SIZE - rotated.width) // 2
    y = (SIZE - rotated.height) // 2 + 10
    image.alpha_composite(shadow, (x + 18, y + 30))
    image.alpha_composite(rotated, (x, y))
    return image


def main():
    image = die(confetti(background()))
    small = die(background(), 1.2, ((140, 140), (280, 280), (420, 420)), 70, 12)
    target = ROOT / "res" / "wiiparty.ico"
    frames = [(small if s <= 32 else image).resize((s, s), Image.LANCZOS) for s in SIZES]
    frames[-1].save(target, append_images=frames[:-1], sizes=[(s, s) for s in SIZES])
    image.resize((256, 256), Image.LANCZOS).save(ROOT / "docs" / "icon.png")
    print(target)


if __name__ == "__main__":
    main()
