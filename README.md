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
- **CORE OPTIONS** (a tab of the window any CONFIG entry opens): the running core's options, kept per
  core, per content folder or per game, and a list of what is stored for any
  core (see [Core options](#core-options)).
- English and Spanish (GENERAL > LANGUAGE).

Not there yet: input mapping, state slots, shaders, archives.

## Settings and overrides

Every setting has a global value. Some can also be set **for one core** or
**for one game** (on one core): an override. The SCOPE selector at the top of
SETTINGS says where a change goes; it opens on the most specific scope the
running game already uses, so edits keep going to that override. A setting
overridden for the running core shows a **C** in front of it, for the running
game a **G**; RESET removes the override (back to the value below).

Overrides you set are stored in `settings/piccolo.cfg`: the global values
first, then a section per core (`[core:snes9x]`) and per game
(`[game:snes9x/Super Mario World (USA)]`). The core name is the core file's
short name, the game name the content file's name without extension.

| Can be overridden per core / game | Global only |
|-----------------------------------|-------------|
| PICTURE: aspect ratio, scaling, auto max offscreen, filter, position, crop (top, bottom, left, right) | GENERAL: language, pause in menu, pause on focus loss, screensaver, game mode, folders |
| SYNC: timing, catch up missed frames, frames in flight, wait for present, frame delay, auto frame delay | DISPLAY: fullscreen, fullscreen type, window mode, aspect, height, scale, remember |
| SOUND: volume, latency | SOUND: output device, mute, rate control, max speed change, resampler quality |

### Automatic overrides

Some values piccolo **finds and keeps by itself**, per core or per game, without
you setting an override. They are not in `piccolo.cfg` and don't show a C or G
marker: they live in their own files, so deleting one of them (with piccolo
closed) only forgets what was learned, never your settings.

| What | Per | Stored in | Kept when | Forget it |
|------|-----|-----------|-----------|-----------|
| Audio latency learned by SOUND > LATENCY: AUTO | core | `settings/audio_latency.cfg` (`snes9x = 52`) | latency is AUTO; written when another game starts and on quit | delete the line or the file |
| Frame delay found by SYNC > AUTO FRAME DELAY | game (on one core) | `settings/frame_delay.cfg` (section `[snes9x/Super Mario World (USA)]`, `delay_ms = 11`) | auto frame delay is on and the search locked | SYNC > SEARCH AGAIN, or delete the section or the file |

Other state piccolo keeps (not per core or game): `settings/last_used.cfg`
(last core and folder, window size and position), `settings/recent.cfg` (the
RECENT list), `settings/imgui.ini` (window layout) and `core_info_cache.cfg`.

## Core options

SETTINGS > CORE OPTIONS edits the running core's options live. SAVE TO picks
where a change is kept, like the SCOPE of the other tabs but with a folder
layer:

| Layer | Section in `settings/core_options.cfg` | Applies to |
|-------|----------------------------------------|------------|
| THIS CORE | `[snes9x]` | every game on the core |
| THIS FOLDER | `[snes9x/dir:Super Nintendo]` | games in a folder with that name (by name, not path: moving the library keeps it) |
| THIS GAME | `[snes9x/game:Super Mario World (USA)]` | one game |

The most specific layer wins (game, folder, core, then the core's own
default), so a core that runs several systems (GBA and GBC) can keep them
apart. Only values that differ from the layer below are written. Markers in
front of an option: **C** core, **D** folder, **G** game; RESET removes it from
the layer being edited. The stored values reach the core as it declares its
options, before it reads them, so options that only apply at start work too.

**When an option breaks a core.** Every core's option list (names, values,
defaults) is also kept in `settings/core_options_cache.cfg`, written as soon as
the core declares them. So STORED VALUES, at the bottom of the tab, lists and
deletes what is stored for **any** core that has run before, even one that now
crashes on start: pick it in CORE, then DELETE a value, DELETE SECTION, or
DELETE ALL FOR THIS CORE. Keys the core no longer declares are marked.

**Safe start.** piccolo writes `settings/running.cfg` before a game loads and
removes it when the game ends properly (an option change during the session
is noted in it at once). If it's still there at the next launch, that session
crashed or hung. Starting the same core again, when it has stored options,
asks first: STORED OPTIONS, CORE DEFAULTS THIS TIME (nothing deleted; fix the
culprit in CORE OPTIONS), DELETE STORED OPTIONS, or CANCEL. An option changed
in the middle of a game is only written when SETTINGS closes, a game starts or
piccolo quits: a change that crashes the core at once is not kept.

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
| `settings/core_options.cfg` | core options per core, folder and game |
| `settings/core_options_cache.cfg` | each core's option list (to clean a core that won't start) |
| `settings/running.cfg` | only while a game runs (safe start) |
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
