# CLAUDE.md — piccolo

Working brief for Claude Code sessions in this repo, the project's workspace.
Read this first, then `STYLE.md` (mandatory for all code) and
`docs/design-notes.md` (full design record). The project as a whole (vision,
family of repos, history) is in the multitap repo's CLAUDE.md; you don't need
that checkout to work here.

## What this is

**piccolo** is a libretro frontend for actually playing games, part of the
Multitap project: sync options, a curated shader list, input mapping, scaling
options, save/load states. **No game library** (no ROM scanning, playlists or
metadata). Its UI recreates the look and feel of a classic emulator GUI with
ImGui ("retro-style UI"), with piccolo's own 5x5 pixel font. The reference is
ZSNES's GUI (naming it is fine: a theme is called ZSNES). It is GPL: never copy
its font, logo or graphics, everything is redrawn. See "piccolo and the
host-libretro test frontend" in `docs/design-notes.md`.

Owner: Andrés (former RetroArch developer, `andres-asm` on GitHub). He wants
direct feedback: say when something is wrong, including his ideas. Reply in the
language he writes in (English or Spanish).

## Repositories (github.com/multitap-org, since 2026-11-19)

piccolo builds on its own: every dependency is a submodule in `deps/`,
Multitap's libraries included. The project is two families: **hosts** (run
cores, no GUI) and **frontends** (GUI).

- `deps/common/hosts-common`        hosts-common: host code (no GUI) + the project's CMake
                                     helpers (`cmake/multitap.cmake`); added first
- `deps/common/frontends-common`    frontends-common: window, Vulkan, audio, settings, input router
- `deps/libretro/host-libretro`     host-libretro: the libretro host (no GUI), libretro_core_info, tests
- `deps/libretro/frontend-libretro` frontend-libretro: core session, core options, core catalog
- `deps/test-libretro`              test-libretro (the Test libretro core), optional (`update = none`)
- `deps/piccolo-shaders`            piccolo-shaders: the curated slang presets (branch `piccolo` of a trimmed
                                     slang-shaders, per-file licenses), copied to shaders/presets/; optional.
                                     Rebased on upstream: tag the pinned commit (piccolo-YYYY-MM-DD) first
- third-party: `deps/SDL`, `deps/imgui`, `deps/libretro-common`, `deps/libsamplerate`,
  `deps/gamemode`, `deps/librashader`, `deps/miniz` (zip reading only), `deps/7zip`
  (only its C 7z decoder + SHA-256, no PPMd), `deps/enet` (UDP connections), `deps/monocypher`
  (X25519, ChaCha20-Poly1305, BLAKE2b, SHA-512 from optional/; EdDSA for the core downloader's
  signatures); libcurl from the system (Linux) for the core downloader

Other repos: libretro-test-frontend (the host's test frontend, same libraries,
test-libretro always), multitap (README + STYLE.md: the family), imguisw (fork
used by test-libretro). Later: piccoletto (streaming companion, Android first),
Multitap (modern multi-host frontend).

**A library change:** edit it in its submodule (the four are on `main`), build
and test here, commit and push THERE first, then commit piccolo's pointer.
Code shared between frontends goes into frontends-common; code shared between
libretro frontends into frontend-libretro. Never copy code between frontends.
Hosts never use SDL or frontend code.

## Ground rules

1. **Follow `STYLE.md`.** Modern C++ written like C: C++20, no `auto`, no lambdas,
   no templates/inheritance/virtual/operator overloading, no exceptions/RTTI,
   no `#define` of our own, no default arguments (two functions instead), plain
   structs + `module_verb()` free functions, explicit fixed-width types,
   `enum class`, `constexpr`. `std::vector` / `std::string` / `std::string_view`
   are fine outside hot paths; `<filesystem>` only inside `host_fs`, error_code
   overloads only. Nothing allocates per frame. Andrés doesn't use
   auto/lambdas/templates himself, so plain code he can read wins. If something
   seems to need a banned construct, ask first.
2. **One feature at a time.** One step per commit. Describe the plan (what, why,
   how you'll check it) before large work; when Andrés asks for answers or
   mockups only, don't implement. When he says he's only reporting/testing,
   collect items and change nothing until he says "go".
3. **Decisions vs recommendations.** In `docs/design-notes.md`, "Decisions (Andrés)"
   are settled. "Recommendations (from Claude, not yet adopted)" are proposals:
   don't implement them without his OK.
4. **Third-party `deps/` are read-only.** Submodules pinned to tags (or a commit).
   Never edit, copy or vendor them. From `deps/libretro-common` use **only**
   `include/libretro*.h` (never its other headers or `.c` files; one is GPL).
5. **Readable over clever.** A human must be able to debug any file. Comment the
   *why*, especially Vulkan synchronization.
6. **Commit and push when a feature works**, without asking: library submodules
   (and test-libretro) first, then piccolo with the pointers. Branch, then
   `merge --ff-only` into main, push. Trailer:
   `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
   Big features go on a branch until Andrés says to merge.
7. **Checks.** Larger changes: `build/` with warnings as errors, `build-asan/`,
   ctest in both. **Small changes** (a label, a spacing, a string): no build, no
   ctest, no ASan; just edit, commit and push.
8. **Parallel sessions:** work in your own git worktree; never switch the main
   checkout's branch unless this session owns it. To commit to another branch,
   use a temporary worktree.
9. `CLAUDE.md` is committed (keep it current); `docs/` stays untracked (notes go
   to `docs/design-notes.md`).
   Ask before anything destructive.

## Stack

- Rendering: raw Vulkan 1.3 (dynamic rendering, synchronization2), 1.1 path for
  1.1-only drivers. Not SDL_GPU. We own the swapchain.
- Platform: SDL3 for window, input, audio only.
- UI: Dear ImGui (official SDL3 + Vulkan backends, unmodified), themed, with
  piccolo's pixel font. No RmlUi in piccolo.
- Shaders (next milestone): our own pass 0 + librashader loaded at runtime
  (`librashader_ld.h`, MIT headers; ABI 2, API 5).
- Build: CMake ≥ 3.25, Ninja. Targets: Linux, Windows, macOS; Android (branch).

## Layout

```
CMakeLists.txt            top-level build: options, SDL, Vulkan, imgui, libretro_api, the libraries, piccolo
STYLE.md                  code rules (mandatory)
src/
  main.cpp                the app: game fills the window, menus, hotkeys, input frame -> core session
  retro_ui.*              retro-style UI from themed ImGui: menu bar, status line, theme, bevels, combos
  pixel_font.*            piccolo's own 5x5 font (static lib `piccolo_font`), pixel_font_imgui.* as an ImGui font
  menus.*                 menu contents and actions (data only)
  strings.txt             ALL of piccolo's UI text in English (key = text); lang/<code>.po translations
  piccolo_text.*          piccolo_text(strings, Str::x): the text in the current language
  piccolo_settings.*      piccolo's settings table (settings/piccolo.cfg)
  settings_ui.* settings_window.* image_window.* piccolo_window.*   SETTINGS, IMAGE SETTINGS, display
  file_picker.*           LOAD GAME: core first, files filtered by its extensions; zip/7z: LOAD
                          (whole / the one loadable file / folder, peeked on selection) or BROWSE;
                          FOLDER: the game folders (SETTINGS > GENERAL, "+") or HOME;
                          CHOOSE FOLDER mode (file_picker_open_folder) for picking a folder
  recent_games.*          RECENT menu
  quick_menu.*            QUICK MENU: SAVESTATES, CORE OPTIONS, CONTROLS (ports, remap: mapping, turbo,
                          combos, macros, mouse / lightgun)
  input_window.*          INPUT: SETTINGS, INTERFACE, HOTKEYS, DEVICES
  map_window.*            MAP CONTROLLER wizard; pad_picture.* the controller drawing
  net_window.*            SETTINGS > NETWORK tab: listener settings, pairing code, paired devices
  net_games.*             games from devices with FULL access: cores, game folders, load (net_content)
  net_remote.*            settings (SETTINGS' pages, the device's language) and actions from devices (net_settings)
  core_options_ui.* safe_start_window.*
  cores_window.*          TOOLS > CORES: the core downloader's window (core_updater in frontend-libretro)
layouts/                  controller layouts (<Maker>/<model>.cfg)
android/                  Android stub (SDL from deps/)
web/                      the browser build (Emscripten, WebGL 2, pthreads; branch wasm): build-core.sh (SameBoy
                          as a static archive), build.sh; src/bundled_files_web.cpp registers the linked core
  main_loop.*             who drives the loop: desktop while, or the browser (main_loop_web: rAF, save on
                          leave, piccolo_web_open); storage.* the browser's piccolo folder on OPFS (WasmFS)
tools/                    pixel_font_preview, strings_check, recent_games_test, stream_measure (remote play
                          codec on real games), lsan-suppressions.txt
deps/common/hosts-common/     host_common: host_log, host_fs (mkdir -p, read, atomic write; the only
                              <filesystem> user), dynamic_library; cmake/multitap.cmake
deps/common/frontends-common/ frontend_common: app_shell (SDL + window + Vulkan + swapchain + renderer + ImGui),
                              video_texture / video_output (what frontends draw the picture with: Vulkan's
                              picture + shader_chain, or the GLES path's gl_texture + gl_picture),
                              gl_context (GLES fallback when Vulkan isn't usable: docs/gl.md, branch gl),
                              vk_* (context, swapchain, texture, barrier), renderer, debug_ui, audio_output (+DRC),
                              frame_timing, frame_delay, rate_control, frame_delta (lossless
                              frame codec for remote play), settings_file / settings (layers global ->
                              core -> game), app_paths, viewport, picture, window_size, localization,
                              input_devices / input_bindings (the router) / input_config / input_sdl /
                              input_sdl_mapping / input_layouts, archive (zip/7z, "a.7z#file" paths) /
                              content_cache (extracted files, LRU); frontend_download (signature, net_http,
                              file_hash: the core downloader; built where libcurl is found);
                              frontend_net (optional, with deps/enet):
                              net_host (ENet + LAN discovery on one UDP port), net_noise (Noise XX),
                              net_link / net_session (encrypted channels per peer), net_cpace (PAKE),
                              net_trust / net_pairing / net_access (host) / net_client (6-digit pairing,
                              access levels gamepad / stream / full), net_input + input_net (remote pads
                              in the router, game-only), net_content (browse / load games: messages) +
                              net_content_folders (host: game folders as virtual paths, never left),
                              net_service (the network on its own thread; remote play is coded and sent
                              there), net_stream (remote play packets), net_settings (settings and
                              actions described by the host), net_speed (speed
                              test), net_wire (fields);
                              tools/strings_generator, net_probe; tests/
deps/libretro/host-libretro/  host_libretro: libretro_core / host / environment / video / audio / options / input;
                              tools/core_info.cpp (libretro_core_info); tests/ (test core, unit tests, Test libretro runs)
deps/libretro/frontend-libretro/ frontend_libretro: core_session, core_catalog, core_options_store / cache,
                              keyboard_input, fixed_input, content_archive (which cores take archives
                              whole, extraction before loading), core_index / core_updater (the core
                              downloader; core_updater_none.cpp with MULTITAP_CORE_UPDATER=OFF); tests/
```

## Setup (first time on a machine)

```sh
git clone --recursive https://github.com/multitap-org/piccolo.git
git -C piccolo submodule update --init --recursive --checkout deps/test-libretro   # optional, for all tests
for d in deps/common/hosts-common deps/common/frontends-common deps/libretro/host-libretro deps/libretro/frontend-libretro; do git -C piccolo/$d checkout main; done
```

System packages: Vulkan loader + headers (1.3+), Khronos validation layers
(Debug builds), and on Linux the SDL build deps for X11/Wayland and audio (SDL
silently skips backends whose dev packages are missing). Ubuntu/Debian:

```sh
sudo apt install libvulkan-dev vulkan-validationlayers pkg-config \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev \
    libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols \
    libdecor-0-dev libegl-dev libgl-dev libdrm-dev libgbm-dev \
    libpipewire-0.3-dev libpulse-dev libasound2-dev libudev-dev libibus-1.0-dev
```

## Build, run, check

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMULTITAP_WARNINGS_AS_ERRORS=ON
cmake --build build && ctest --test-dir build
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMULTITAP_WARNINGS_AS_ERRORS=ON -DMULTITAP_SANITIZE=ON
cmake --build build-asan && (cd build-asan && LSAN_OPTIONS=suppressions=$PWD/../tools/lsan-suppressions.txt ctest)
./build/piccolo                    # normal builds in the main checkout also copy it to ~/.local/share/piccolo/
                                   # (not from a worktree: MULTITAP_COPY_FROM_WORKTREE=ON to copy)
MULTITAP_GPU=gl ./build/piccolo     # the OpenGL ES path (GLES 3); -DMULTITAP_GL_ES2=ON builds test like the Tegra 4
web/build-core.sh && web/build.sh  # wasm (emsdk 6.0.13 sourced, rgbds): SameBoy linked into web/build/wasm/piccolo.{js,wasm}
android/build-apk.sh [--legacy]    # --legacy: armv7, Android 5+, GLES 2 only (-DMULTITAP_GPU_VULKAN=OFF), the SHIELD Portable
./build/deps/libretro/host-libretro/libretro_core_info [--frames N] [--set KEY=VALUE]... <core> [content]
```

67 tests with test-libretro (48 without). Before calling a larger change done:
warnings-as-errors clean, ctest in both builds, no Vulkan validation messages,
no ASan/UBSan reports beyond `tools/lsan-suppressions.txt`, resizing and
quitting work. Benchmarks: Release build (warnings-as-errors OFF: two harmless
-O2 warnings), `XDG_DATA_HOME=<scratch>`, `taskset -c 4-7` on the RK3588.
Low-end test target: the RK3588 ARM board (Mali-G610, Mesa panvk).

## Current state (2026-11-19)

- piccolo 0.3: plays games through frontend-libretro; LOAD GAME, RECENT,
  SETTINGS (global / core / game layers), IMAGE SETTINGS, QUICK MENU
  (savestates 1-9 with thumbnails, core options, controls), fast forward,
  native pixel formats, frame delay, localization (es), games from zip / 7z
  (extracted into <root>/cache, trimmed to 4 GiB; cores never get virtual paths).
- Input (done for this version): N:N ports (hotkeys and UI from port 1 + the
  keyboard), per-core remaps with per-game copies, combos and macros (recorder)
  per core, turbo (row box + TURBO BUTTON for the session), mouse / pointer /
  lightgun from one cursor per port (several mice), MAP CONTROLLER wizard,
  layouts, numbered controller names, PORTS setting. Left for later: touch
  (drag mode, DS pointer), crosshair, Joy-Cons, per-port unlink, radial menu,
  overlay, on-screen messages redesign.
- Untested on hardware: SN30 over USB (unknown pad), Genesis / Saturn layouts,
  Shield touchpad pointer, mouse grab, lightguns (Zapper, Super Scope).
- Network (merged 2026-11-19; pending work in docs/netcode-pending.md): one UDP port
  (ENet) with LAN discovery, Noise XX encryption, 6-digit pairing, access levels
  gamepad / stream / full; remote pads (standard pads remapped here, or RetroPad mapped
  on the phone), what runs, actions (save / load state, slot, fast forward, pause,
  reset, close), settings, controllers per port, game folders browsed and games loaded
  from the client. SETTINGS > NETWORK. Remote play: lossless frames (frame_delta) and audio streamed
  from the network thread; piccoletto's side not written yet.
- Next: shaders (milestone 6): librashader is in deps/, not wired yet.
- Release plan (early December): piccolo + piccoletto (Android: remote input,
  streaming). Branches `android`, `big-picture` still need adapting to deps/.
- Known: Release builds fail with warnings-as-errors (GCC -O2 only:
  `instance_layer_available` unused outside Debug, a -Wformat-truncation in a
  log note). Pending: imgui docking branch.

## Design points to preserve

- We own the swapchain on purpose (present_wait pacing, present-mode switching,
  librashader and core compositing in our own command buffers).
- `SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO)` must stay, or validation warnings
  (SDL_LOG_CATEGORY_GPU) are hidden.
- host-libretro: libretro callbacks have no userdata, so one `thread_local`
  current instance is set/cleared by `host_enter()`/`host_leave()` around EVERY
  call into the core. Hosts never use SDL. Never keep the pointer a core passes
  to video_refresh: the host copies every frame. Frontends hand input as a whole
  `LibretroInputState` per frame; the host never polls devices.
- The input router (frontends-common) knows no SDL and no libretro; layers
  HOTKEY > INTERFACE > GAME; what's held entering a layer is ignored until let go.
