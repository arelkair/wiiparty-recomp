import hashlib
import sys
import tarfile
import urllib.request
from pathlib import Path

VERSION = "3.4.16"
URL = f"https://github.com/libsdl-org/SDL/releases/download/release-{VERSION}/SDL3-devel-{VERSION}-mingw.tar.gz"
SHA256 = "c7ef65bd72eabac6e5b535411dbd8d5824d0aab24fd62ff8812666b336f18a9c"

root = Path(__file__).resolve().parent.parent
deps = root / "build" / "deps"
archive = deps / "sdl3.tar.gz"
target = deps / f"SDL3-{VERSION}"


def main():
    if (target / "x86_64-w64-mingw32" / "lib" / "cmake" / "SDL3").is_dir():
        print(f"SDL3 {VERSION} already in {target}")
        return 0
    deps.mkdir(parents=True, exist_ok=True)
    if not archive.exists():
        print(f"downloading {URL}")
        urllib.request.urlretrieve(URL, archive)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != SHA256:
        archive.unlink()
        print(f"checksum mismatch: {digest}", file=sys.stderr)
        return 1
    with tarfile.open(archive) as tar:
        tar.extractall(deps, filter="data")
    print(f"SDL3 {VERSION} extracted to {target}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
