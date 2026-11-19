# piccolo

A small libretro frontend for **playing games**, with a retro-style UI in the
spirit of classic emulator GUIs: a menu bar, a LOAD GAME window, and a 5x5
pixel font drawn from scratch for piccolo.

piccolo is part of the [Multitap](https://github.com/multitap-org/multitap)
project and builds on its own: Multitap's libraries come in as submodules,
with every other dependency, in `deps/`:

- `deps/common/hosts-common`: code shared by core hosts, and the CMake helpers;
- `deps/libretro/host-libretro`: the libretro host;
- `deps/common/frontends-common`: window, Vulkan, audio, settings, input;
- `deps/libretro/frontend-libretro`: the core session, core options, cores.

## What it does so far

- **GAME > LOAD GAME**: pick a core first, then a file. The file list shows
  only what that core says it can load (asked from the core itself, not from
  `.info` files). Cores that run without content can be started directly.
- The game fills the window at the core's aspect ratio, with sharp pixels.
- **Esc** shows and hides the menu while a game runs; the game pauses while
  the menu or the LOAD GAME window is up.
- **GAME**: reset, save state / load state (the current slot, also F2 / F4), quit.
- **F1: quick menu** while a game runs (see [Quick menu](#quick-menu)).
- Save RAM and savestates are kept automatically.
- Keyboard and gamepad as the controller (fixed mapping for now).
- **RECENT** (the first menu entry): the last 10 games started.
- **CONFIG > GENERAL / INTERFACE / VIDEO / SOUND**: settings, each one global or, where it
  makes sense, just for the running core or game (the SCOPE at the top).
  Video: aspect ratio, integer / auto scaling, filters, overscan crop (IMAGE
  SETTINGS), window and fullscreen, Vsync / Exact timing, low latency. Sound:
  output device, volume, latency, rate control, resampler quality.
- **CONFIG > INTERFACE**: the look (see [Interface themes](#interface-themes)).
- **Core options**: the running core's options, kept per core, per content
  folder or per game, changed in the quick menu; TOOLS > OPTIONS MANAGER
  shows and deletes what is stored for any core (see
  [Core options](#core-options)).
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
| | INTERFACE: theme, menu title boxes, shadows, theme cursor, colors |
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

## Quick menu

F1 while a game runs opens the quick menu (F1 or Esc closes it; the game
pauses while it's open). Two tabs:

- **SAVESTATES**: slots 1 to 9. Arrows, the keys 1-9 or the mouse pick one
  (it's also the slot F2 / F4 use); used slots are lit. The selected slot
  shows its thumbnail and date. SAVE (S) and LOAD (L) act on it; Enter only
  picks a slot, so nothing is loaded or overwritten by accident. Also RESET
  and CLOSE GAME.
- **CORE OPTIONS**: the running core's options (see [Core options](#core-options)).

Each state is `states/<core>/<game>.state<N>`, with a thumbnail next to it,
`<game>.state<N>.png` (also written by F2): the picture as it is shown (the
Picture settings' aspect ratio and crop), at x1.

## Core options

The quick menu's CORE OPTIONS tab edits the running core's options live.
SAVE TO picks where a change is kept, like the SCOPE of SETTINGS but with a
folder layer:

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
the core declares them. So TOOLS > OPTIONS MANAGER lists and
deletes what is stored for **any** core that has run before, even one that now
crashes on start: pick it in CORE, then DELETE a value, DELETE SECTION, or
DELETE ALL FOR THIS CORE. Keys the core no longer declares are marked.

**Safe start.** piccolo writes `settings/running.cfg` before a game loads and
removes it when the game ends properly (an option change during the session
is noted in it at once). If it's still there at the next launch, that session
crashed or hung. Starting the same core again, when it has stored options,
asks first: STORED OPTIONS, CORE DEFAULTS THIS TIME (nothing deleted; fix the
culprit in CORE OPTIONS), DELETE STORED OPTIONS, or CANCEL. An option changed
in the middle of a game is only written when the quick menu closes (or the
game ends, another one starts, piccolo quits): a change that crashes the core at once is not kept.

## Interface themes

CONFIG > INTERFACE sets the look. THEME sets everything at once:

| Theme | Look |
|-------|------|
| PICCOLO | blue and flat, no shadows |
| CLASSIC | a purple background, a blue gradient menu bar with a box behind each menu title, grey menus with a magenta highlight, blue windows with gradient title bars and black fields, raised buttons and check boxes, sunken lists, drop shadows under windows and text, an orange arrow cursor |
| MODERN | flat and dark, never a shadow; the menus are the bar's color |

Every color can also be changed on its own (THEME then shows CUSTOM), as
can MENU TITLE BOXES, DROP SHADOWS and THEME CURSOR (piccolo draws its own
pixel arrow instead of the system's, while the menu or a window is up).
Raised and sunken edges (buttons, check boxes, scroll bars, color swatches,
lists) appear when a theme's button edges have two colors; with one color,
as in PICCOLO and MODERN, everything stays flat. The background color shows
only without a game: around a running game it's black. The interface
settings are global and live in `settings/piccolo.cfg` (`ui_` keys).

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
| `states/<core>/<game>.state<N>` | savestates (slots 1-9), each with a `.png` thumbnail |
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

Clone piccolo with its submodules and build it:

```sh
git clone --recursive https://github.com/multitap-org/piccolo.git
cd piccolo
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
~/.local/share/piccolo/piccolo     # normal builds copy it there
ctest --test-dir build             # the tests (piccolo's and its libraries')
```

Optional: the Test libretro core (`deps/test-libretro`, not cloned by
default) is built and copied into piccolo's `cores/`, and adds the host's
checks with it to the tests:

```sh
git submodule update --init --recursive --checkout deps/test-libretro
```

A change to one of Multitap's libraries is made in its submodule (commit and
push it there first), then piccolo's pointer to it is committed.

## Layout

| Path | What |
|------|------|
| `src/main.cpp` | the program: game picture, menu, Esc, actions |
| `src/retro_ui.*` | the retro look: the themes, themed Dear ImGui widgets, bevels, text shadows, scroll bars, cursor |
| `src/pixel_font.*` | the 5x5 font (glyphs written as `#`/`.` pictures) |
| `src/pixel_font_imgui.*` | the font as a Dear ImGui font (custom font loader) |
| `src/file_picker.*` | the LOAD GAME window |
| `src/menus.*` | menu contents |
| `tools/pixel_font_preview.cpp` | writes a PNG of every glyph; `--check` validates the font (CTest) |

## License

MIT, see [`LICENSE`](LICENSE).
