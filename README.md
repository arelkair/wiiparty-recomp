# Wii Party Recomp

Static recompilation of Wii Party for native PC.

> **Status:** early development. Nothing is playable yet.

## Legal notice

This project does not include any copyrighted assets, game code or binaries from Nintendo. You must provide your own legally obtained copy of Wii Party. This project is not affiliated with or endorsed by Nintendo.

## How it works

The original game executable is analyzed and translated into C++ source code, which is then compiled natively for the target platform. The game data is read at runtime from the copy you provide.

## Requirements

- A legally obtained copy of Wii Party (NTSC-U / PAL / NTSC-J: [indicate supported regions])
- CMake 3.20 or newer
- A C++17 compiler (MSVC, GCC or Clang)
- [Additional dependencies]

## Building

    git clone https://github.com/arelkair/wiiparty-recomp.git
    cd wiiparty-recomp
    cmake -B build
    cmake --build build --config Release

## Usage

1. Extract the game executable from your own copy.
2. Place it in [expected path].
3. Run [tool name] to generate the recompiled sources.
4. Build the project and launch it.

## Roadmap

- [ ] Executable analysis and function detection
- [ ] Recompilation of game code
- [ ] Graphics backend
- [ ] Audio
- [ ] Input
- [ ] Playable build

## Contributing

Issues and pull requests are welcome. Please do not share or upload copyrighted game files anywhere in this repository, including issues and pull requests.

## License

MIT. See [LICENSE](LICENSE).
