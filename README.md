# Maple Valley U — 3DS Visual Novel

Episodic college-drama visual novel for Nintendo 3DS homebrew (.3dsx).

- **Season 1 — One Pace at a Time** (MVU Track & Field): London Sterling × Duncan "Diggy" Williams
- **Season 2 — MVU Women's Soccer** ("Play Hard. Gossip Harder.")

## Build
Requires devkitPro (devkitARM, libctru, citro2d/citro3d).
```
make            # -> maple-valley-u.3dsx
```
Copy the .3dsx to `/3ds/` on your SD card and launch from the Homebrew Launcher.

## Layout
- `source/` — engine (C++): script interpreter, dialogue box, choices, variables, save
- `romfs/script/s1`, `s2` — episode scripts (`.mvs`, plain text, see docs/SCRIPT_FORMAT.md)
- `romfs/gfx` — converted sprites/backgrounds (.t3x); sources in `gfx/`
- `docs/` — series bible, cast, season outlines
- `art/portraits` — cropped character portraits (faceclaims, placeholders until final art); `tools/png2tex.py` converts them
