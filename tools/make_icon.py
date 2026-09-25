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


def svg():
    top = "#%02x%02x%02x" % (255, 92, 168)
    bottom = "#%02x%02x%02x" % (214, 24, 120)
    face = (
        '<rect x="20" y="20" width="520" height="520" rx="120" fill="#ffffff"/>'
        '<rect x="20" y="470" width="520" height="70" rx="60" fill="#e4e4ec"/>'
        '<rect x="20" y="20" width="520" height="480" rx="120" fill="#ffffff"/>'
    )
    pips = "".join(f'<circle cx="{x}" cy="{y}" r="70" fill="#222230"/>' for x, y in ((140, 140), (280, 280), (420, 420)))
    placement = "translate(512 522) rotate(14) scale(1.2) translate(-280 -280)"
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{SIZE}" height="{SIZE}" viewBox="0 0 {SIZE} {SIZE}">
<defs>
<linearGradient id="background" x1="0" y1="0" x2="0" y2="{SIZE - 1}" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="{top}"/><stop offset="1" stop-color="{bottom}"/></linearGradient>
<clipPath id="tile"><rect x="32" y="32" width="{SIZE - 64}" height="{SIZE - 64}" rx="220"/></clipPath>
<filter id="blur" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="12"/></filter>
</defs>
<g clip-path="url(#tile)">
<rect width="{SIZE}" height="{SIZE}" fill="url(#background)"/>
<ellipse cx="512" cy="-50" rx="712" ry="470" fill="#ffffff" fill-opacity="0.18"/>
</g>
<g transform="translate(18 30)" filter="url(#blur)"><g transform="{placement}"><rect x="20" y="20" width="520" height="520" rx="120" fill="#5a0028" fill-opacity="0.43"/></g></g>
<g transform="{placement}">{face}{pips}</g>
</svg>
"""


def main():
    image = die(confetti(background()))
    small = die(background(), 1.2, ((140, 140), (280, 280), (420, 420)), 70, 12)
    target = ROOT / "res" / "wiiparty.ico"
    frames = [(small if s <= 32 else image).resize((s, s), Image.LANCZOS) for s in SIZES]
    frames[-1].save(target, append_images=frames[:-1], sizes=[(s, s) for s in SIZES])
    (ROOT / "res" / "wiiparty.svg").write_text(svg(), encoding="utf-8")
    print(target)


if __name__ == "__main__":
    main()
