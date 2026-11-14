# piccolo

A small libretro frontend for **playing games**, with a retro-style UI in the
spirit of classic emulator GUIs: a menu bar, a LOAD GAME window, and a 5x5
pixel font drawn from scratch for piccolo.

piccolo is part of [Multitap](https://github.com/andres-asm/multitap) and is
built inside it (as the `frontends/piccolo` submodule): it uses Multitap's
libretro host, its shared frontend libraries (window, Vulkan, audio, the core
session) and its CMake helpers. It doesn't build on its own.

## What it does so far

- **GAME > LOAD GAME**: pick a core first, then a file. The file list shows
  only what that core says it can load (asked from the core itself, not from
  `.info` files). Cores that run without content can be started directly.
- The game fills the window at the core's aspect ratio, with sharp pixels.
- **Esc** shows and hides the menu while a game runs; the game pauses while
  the menu or the LOAD GAME window is up.
- **GAME**: reset, save state / load state (slot 0, also F2 / F4), quit.
- Save RAM and savestates are kept automatically.
- Keyboard and gamepad as the controller (fixed mapping for now).
- **RECENT** (the first menu entry): the last 10 games started.
- **CONFIG > GENERAL / VIDEO / SOUND**: settings, each one global or, where it
  makes sense, just for the running core or game (the SCOPE at the top).
  Video: aspect ratio, integer / auto scaling, filters, overscan crop (IMAGE
  SETTINGS), window and fullscreen, Vsync / Exact timing, low latency. Sound:
  output device, volume, latency, rate control, resampler quality.
- English and Spanish (GENERAL > LANGUAGE).

Not there yet: input mapping, state slots, shaders, archives.

## Automatic settings

piccolo tunes a few things by itself, so the out-of-the-box experience is low
latency without stutter. Each can be turned off when it causes trouble.

| What | Default | What it does | Kept |
|------|---------|--------------|------|
| SOUND > LATENCY: AUTO | on | Starts low and raises the audio latency only when the sound runs out (never below what the device needs). Comes down again after long quiet stretches. Stalls while the window has no focus don't count. | per core, in `settings/audio_latency.cfg` (only while AUTO is chosen) |
| SYNC > AUTO FRAME DELAY | on | Each game starts at the longest frame delay the display allows (12 ms at 60 Hz) and lowers it 1 ms each time a frame is missed. The first 5 seconds don't count (games stall on their own while starting). After a minute of play in a row without a miss it locks; pausing, the menu or another window start that minute over. Needs Vsync timing and SYNC > WAIT FOR PRESENT (also on by default: after presenting, wait until the frame is on screen, or where the GPU can't tell, until the GPU has finished it, like a "hard GPU sync"; it can cost speed in heavy cores). | per game, in `settings/frame_delay.cfg`: the game starts locked at it next time. SEARCH AGAIN (shown under it) forgets it and searches again. |
| SYNC > CATCH UP MISSED FRAMES | on | In Vsync timing, when the system stalls and a frame comes late, runs the core frames it missed, so the game and its sound don't fall behind (one picture repeats instead). | nothing to keep |
| GENERAL > GAME MODE | on | While a game is loaded, asks [GameMode](https://github.com/FeralInteractive/gamemode) (Linux) for full CPU speed, so the clock doesn't drop in bursts. Needs GameMode installed and, for the CPU governor, the user in the `gamemode` group. | nothing to keep |

A locked frame delay can be too long for a scene that is heavier than what
the game showed while it was searching: frames are missed and the sound may
cut out. Then use SEARCH AGAIN, or turn AUTO FRAME DELAY off and set FRAME
DELAY by hand.

## Where things go

Everything piccolo keeps is in one folder, so moving it to another machine is
moving that folder: `~/.local/share/piccolo/` on Linux (or
`$XDG_DATA_HOME/piccolo`), `~/Library/Application Support/piccolo/` on macOS,
and next to `piccolo.exe` on Windows.

| Path | What |
|------|------|
| `cores/` | the cores piccolo offers (drop `*_libretro.so` files here) |
| `saves/<core>/<game>.srm` | save RAM |
| `states/<core>/<game>.state<N>` | savestates |
| `system/` | BIOS and other files cores ask for |
| `settings/piccolo.cfg` | the settings (global, per core, per game) |
| `settings/last_used.cfg` | last core and folder, window size and position |
| `settings/recent.cfg` | the RECENT list |
| `settings/audio_latency.cfg` | automatic audio latency learned per core |
| `settings/frame_delay.cfg` | automatic frame delay found per game |
| `settings/imgui.ini` | window layout |
| `core_info_cache.cfg` | what each core said about itself (refreshed when a core file changes) |

The first time it runs, piccolo copies `cores/`, `saves/` and `states/` from the
old `~/.host-libretro/` folder if there is one. The old folder is left as it is.

## Building

Clone Multitap with its submodules and build it; piccolo is one of its targets:

```sh
git clone --recursive https://github.com/andres-asm/multitap.git
cd multitap
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
~/.local/share/piccolo/piccolo     # normal builds copy it there
```

## Layout

| Path | What |
|------|------|
| `src/main.cpp` | the program: game picture, menu, Esc, actions |
| `src/retro_ui.*` | the retro look: themed Dear ImGui widgets, bevels, drop-shadowed labels |
| `src/pixel_font.*` | the 5x5 font (glyphs written as `#`/`.` pictures) |
| `src/pixel_font_imgui.*` | the font as a Dear ImGui font (custom font loader) |
| `src/file_picker.*` | the LOAD GAME window |
| `src/menus.*` | menu contents |
| `tools/pixel_font_preview.cpp` | writes a PNG of every glyph; `--check` validates the font (CTest) |

## License

MIT, see [`LICENSE`](LICENSE).
