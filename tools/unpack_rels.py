import sys
from pathlib import Path

import lz11

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "extracted" / "files" / "rel"
TARGET = ROOT / "build" / "rel"


def main():
    TARGET.mkdir(parents=True, exist_ok=True)
    failures = 0
    for source in sorted(SOURCE.glob("*.rel.lz")):
        try:
            data = lz11.decompress(source.read_bytes())
        except (ValueError, IndexError) as error:
            print(f"{source.name}: {error}", file=sys.stderr)
            failures += 1
            continue
        (TARGET / source.with_suffix("").name).write_bytes(data)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
