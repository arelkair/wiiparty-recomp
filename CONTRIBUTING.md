# Contributing to Wii Party Recomp

I appreciate your support!

## Before anything else

- Never upload Nintendo's data: `main.dol`, `.rel` files, ISO images, textures, models, sounds, fonts, videos or memory dumps of the game. This applies to commits, issues and pull requests.
- `games/wiiparty/disc/`, `games/wiiparty/extracted/` and `reference/` are ignored by Git. Keep them that way.
- Use your own legally obtained copy of Wii Party.

## License

Contributions are licensed under GPL-3.0-or-later, like the rest of the project. By submitting a change you state that you have the right to submit it under that license.

## Third-party code

- Do not add code from other projects unless its license allows it. Check each file, not only the repository.
- Keep the copyright notices and license terms of every file you add.
- Record the origin in `THIRD_PARTY_NOTICES.md`: upstream path, revision, authors, license.
- If the license or the origin is unclear, do not add the code.
- Not accepted: code from InputEvelution/wp without a valid license, leaks, proprietary code, code of unknown origin.

## Code

- No comments in code files (`.c`, `.cpp`, `.h`, scripts). Documentation goes in `docs/DECOMP_PROGRESS.md`.
- Build outside the source tree, in `build/`.
- Do not commit generated code (`build/`), logs or temporary files.
- Do not replace real behavior with a stub without recording the reason in `docs/DECOMP_PROGRESS.md`.
- Separate recompiled code from PC-only changes.

## Build and test

```
python recompiler/recomp.py
cmake -S . -B build/out -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/out
ctest --test-dir build/out
```

The full setup is in `README.md`. A change that touches the runtime must also be run with the game. If it was not run, say so.

## Reporting problems

Include the commit, the operating system, the command line and the console output. For crashes, include the report the program prints (call stack and registers). Do not attach game files.

## Commits

- English, imperative mood, ending with a period. Example: `Fix module function splitting.`
- One logical change per commit.

## Documentation

Update `docs/DECOMP_PROGRESS.md` after each change that affects behavior. State what was verified and how. Do not claim something works if it was not tested.
