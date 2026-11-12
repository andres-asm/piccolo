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

Not there yet: the CONFIG dialogs (input mapping, video and sound options),
state slots, scaling options, shaders, archives.

## Where things go

Cores, saves, savestates and settings live in `~/.host-libretro/`, shared with
Multitap's libretro test frontend:

| Path | What |
|------|------|
| `cores/` | the cores piccolo offers (drop `*_libretro.so` files here) |
| `saves/<core>/<game>.srm` | save RAM |
| `states/<core>/<game>.state<N>` | savestates |
| `piccolo.cfg` | last core and folder |
| `core_info_cache.cfg` | what each core said about itself (refreshed when a core file changes) |

## Building

Clone Multitap with its submodules and build it; piccolo is one of its targets:

```sh
git clone --recursive https://github.com/andres-asm/multitap.git
cd multitap
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
~/.host-libretro/piccolo     # normal builds copy it there
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
