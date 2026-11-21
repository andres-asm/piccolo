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

## Features

piccolo 0.3. Platforms: **Linux** (Flatpak), **Windows** (zip), **Android**
(APK, Android 9+; a legacy APK for Android 5+ with OpenGL ES 2) and the
**browser** (WebAssembly, SameBoy and Snes9x built in: [get-piccolo.app/play](https://get-piccolo.app/play/)).
macOS builds but isn't tested.

### What's there

**Playing**
- **GAME > LOAD GAME**: pick a core first, then a file. The list shows only
  what that core says it loads (asked from the core, not from `.info` files).
  Games inside **zip / 7z** archives: the whole archive, the one loadable file,
  or a folder in it. Game folders of your own (SETTINGS > GENERAL) show up in
  the picker.
- **RECENT** (the first menu entry): the last 10 games started.
- Save RAM and **savestates** (slots 1-9, each with a thumbnail) kept
  automatically. **Fast forward**. **Run-ahead** (one setting, for cores
  with their own run-ahead).
- **TOOLS > CORES**: a core downloader from a signed index (non-free cores
  hidden unless you turn them on).
- **Core options** per core, per folder or per game, with a safe start after a
  crash (see [Core options](#core-options)).
- A core's text request gets piccolo's text box (a libretro extension of
  Multitap's). **ON-SCREEN KEYBOARD** (INPUT > SETTINGS) brings up the
  system's keyboard for text fields and for cores that ask for one (AUTO: on
  for phones, TVs and handhelds such as the Steam Deck, off on a PC).

**Picture and sound**
- Vulkan, with an **OpenGL ES** fallback (GLES 3, or GLES 2 on old devices).
- Aspect ratio, integer / auto scaling, crop, filters, window and fullscreen.
- **Shaders**: a curated list of presets through librashader (loaded at run
  time; without it the plain picture is drawn), plus piccolo's own first pass:
  picture controls and color profiles (GBA, GBC, DS, PSP, NTSC...). Picked in
  the quick menu's SHADER tab; a preset too slow for the machine asks KEEP /
  REVERT. On OpenGL ES, piccolo's own GL presets.
- Low latency out of the box: Vsync or exact timing, frame delay that tunes
  itself, audio latency that tunes itself, catch-up of missed frames, GameMode
  on Linux (see [Automatic settings](#automatic-settings)).

**Input**
- Any number of controllers on any number of ports; hotkeys and the UI from
  port 1 and the keyboard. PORTS setting, pause when controllers disconnect.
- **MAP CONTROLLER** wizard, controller layouts, per-core remaps with
  per-game copies, **turbo**, **combos** and **macros** (recorded from the pad).
- Mouse, pointer and lightgun from one cursor per port (several mice).

**Network** (SETTINGS > NETWORK)
- Pair a phone or another piccolo with a 6-digit code; everything is
  encrypted. Access levels per device: **GAMEPAD** (play with its
  controllers), **STREAM** (also get the picture and sound), **FULL** (also
  settings, loading games, taking the game), **OWNER** (also copy games, saves
  and states to and from the shared folders).
- **piccoletto**, the companion app (Android): a phone as a gamepad,
  **remote play** (piccolo streams the game, lossless, with its sound),
  DEVICES (who plays on which port), and **GO HEADLESS** (the phone takes the
  game, piccolo hides behind a tray icon).
- Across the internet through a beacon (IPv6 and IPv4, hole punching);
  WebRTC data channels where libdatachannel is built in.
- **TOOLS > SHARE**: browse another piccolo's game folders (OWNER access) and
  copy games, saves and states both ways, side by side.

**The rest**
- The retro UI: themes (BLUE, PURPLE, ZSNES, BLACK, your own), the 5x5 pixel
  font, scaled to the window (see [Interface themes](#interface-themes)).
- English and Spanish (GENERAL > LANGUAGE).
- Settings global, per core or per game (see [Settings and overrides](#settings-and-overrides)).

### Not there yet

- **Rewind** (being designed: realtime with scrub, snapshots, replay, export
  to video).
- **Netplay** (playing one game together from two piccolos), achievements,
  cheats, video recording.
- Touch controls on screen (drag mode, DS pointer), crosshair, Joy-Cons,
  overlays, a radial menu, redesigned on-screen messages.
- A console / TV interface and a phone interface (planned), the imgui docking
  branch.
- Android: picking folders on SD cards and USB drives (SAF).
- Windows' and KDE's on-screen keyboards.
- Remote play in the browser (the browser build has no network yet), the
  browser's file panel, saves import / export there.
- Releases and self-update (planned: releases.get-piccolo.app), subsystems
  in the UI (the host supports them), libretro extensions beyond run-ahead and
  text input (multitap adapters, media slots).
- Not tried on real hardware yet: some pads (SN30 over USB), the Genesis /
  Saturn layouts, lightguns (Zapper, Super Scope), the legacy APK on a real
  Tegra 4.

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
pauses while it's open). Tabs:

- **SAVESTATES**: slots 1 to 9. Arrows, the keys 1-9 or the mouse pick one
  (it's also the slot F2 / F4 use); used slots are lit. The selected slot
  shows its thumbnail and date. SAVE (S) and LOAD (L) act on it; Enter only
  picks a slot, so nothing is loaded or overwritten by accident. Also RESET
  and CLOSE GAME.
- **CORE OPTIONS**: the running core's options (see [Core options](#core-options)).
- **CONTROLS**: who is on which port, the core's device per port, REMAP
  (mapping, turbo, combos, macros, mouse / lightgun).
- **SHADER**: the preset for the core or the game (CHOOSE), and FILTER.

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

SETTINGS > INTERFACE sets the look. THEME sets everything at once:

| Theme | Look |
|-------|------|
| BLUE | navy and flat, no shadows (the default) |
| PURPLE | the same in atomic purple |
| ZSNES | a purple background, a blue gradient menu bar with a box behind each menu title, grey menus with a magenta highlight, blue windows with gradient title bars and black fields, raised buttons and check boxes, sunken lists, drop shadows under windows and text, an orange arrow cursor |
| BLACK | flat and dark, never a shadow; the menus are the bar's color |

More themes can be added as files: `themes/<name>.cfg`, next to the program or
in piccolo's folder, with the same `ui_` keys as `settings/piccolo.cfg`. THEME
lists the built-in themes and these, sorted by name, then CUSTOM.
`themes/example.cfg` (GRAY) comes with piccolo and explains each key.

SAVE THEME... saves the colors as they are as one of your own themes, asking
for its name: `themes/<name>-custom.cfg` in piccolo's folder (the `-custom`
ending keeps it from replacing a theme that comes with piccolo). Your own
themes are in CUSTOM THEME, which works when THEME is CUSTOM.

Every color can also be changed on its own (THEME then shows CUSTOM), as
can MENU TITLE BOXES, DROP SHADOWS and THEME CURSOR (piccolo draws its own
pixel arrow instead of the system's, while the menu or a window is up).
Raised and sunken edges (buttons, check boxes, scroll bars, color swatches,
lists) appear when a theme's button edges have two colors; with one color,
as in BLUE, PURPLE and BLACK, everything stays flat. The background color shows
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
| `cache/` | games extracted from zip / 7z (trimmed to 4 GiB) |
| `shaders/` | your own shader presets (piccolo's curated ones come with it) |
| `themes/` | your own themes |
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

Other platforms (details in [`CLAUDE.md`](CLAUDE.md#build-run-check) and `docs/packaging.md`):

```sh
MULTITAP_GPU=gl ./build/piccolo       # try the OpenGL ES path on the desktop
android/build-apk.sh [--legacy]       # the APK (--legacy: Android 5+, armv7, GLES 2)
web/build-core.sh snes9x && web/build.sh snes9x   # the browser build (emsdk; sameboy too), web/package.sh for the zip
```

CI builds the Flatpak, the Windows zip (MSYS2 UCRT64, x64 and ARM64), the APKs
and the web zip (with a Docker image that serves it with the headers it needs).

A change to one of Multitap's libraries is made in its submodule (commit and
push it there first), then piccolo's pointer to it is committed.

## Layout

`src/` is the program (menus, windows, the retro UI, the pixel font);
`android/`, `web/` and `flatpak/` are the platform builds; `deps/` the
libraries. The full map, file by file, is in [`CLAUDE.md`](CLAUDE.md#layout).

## License

MIT, see [`LICENSE`](LICENSE).
