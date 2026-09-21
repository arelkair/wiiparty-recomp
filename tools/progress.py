import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WIDTH = 720
ROW = 34
TOP = 96
LEFT = 24
LABEL = 250
BAR = 360


def read_rows():
    with open(ROOT / "analysis" / "progress.csv", newline="", encoding="utf-8") as handle:
        return [(row["component"], int(row["percent"])) for row in csv.DictReader(handle)]


def render(rows):
    overall = round(sum(percent for _, percent in rows) / len(rows))
    height = TOP + ROW * len(rows) + 56
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{height}" viewBox="0 0 {WIDTH} {height}" role="img" aria-label="Project progress {overall} percent">',
        f'<rect width="{WIDTH}" height="{height}" rx="14" fill="#ffffff" stroke="#d0d0d0"/>',
        f'<text x="{LEFT}" y="44" font-family="Helvetica, Arial, sans-serif" font-size="22" font-weight="700" fill="#111111">Wii Party Recomp</text>',
        f'<text x="{LEFT}" y="70" font-family="Helvetica, Arial, sans-serif" font-size="13" fill="#555555">Estimated progress toward a playable native build</text>',
        f'<text x="{WIDTH - LEFT}" y="52" text-anchor="end" font-family="Helvetica, Arial, sans-serif" font-size="40" font-weight="700" fill="#111111">{overall}%</text>',
    ]
    for index, (name, percent) in enumerate(rows):
        y = TOP + index * ROW
        fill = round(BAR * percent / 100)
        out.append(f'<text x="{LEFT}" y="{y + 15}" font-family="Helvetica, Arial, sans-serif" font-size="14" fill="#111111">{name}</text>')
        out.append(f'<rect x="{LEFT + LABEL}" y="{y + 3}" width="{BAR}" height="16" rx="8" fill="#ececec"/>')
        if fill > 0:
            out.append(f'<rect x="{LEFT + LABEL}" y="{y + 3}" width="{max(fill, 16)}" height="16" rx="8" fill="#111111"/>')
        out.append(f'<text x="{WIDTH - LEFT}" y="{y + 16}" text-anchor="end" font-family="Helvetica, Arial, sans-serif" font-size="14" fill="#111111">{percent}%</text>')
    footer = TOP + ROW * len(rows) + 24
    out.append(f'<text x="{LEFT}" y="{footer}" font-family="Helvetica, Arial, sans-serif" font-size="11" fill="#777777">Equal-weight average of the components above. Estimates, not measurements. Basis in analysis/progress.csv.</text>')
    out.append("</svg>")
    return "\n".join(out) + "\n", overall


def main():
    text, overall = render(read_rows())
    target = ROOT / "docs" / "progress.svg"
    target.write_text(text, encoding="utf-8")
    print(f"overall {overall}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
