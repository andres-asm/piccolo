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
ZSNES's GUI (naming it is fine: a theme is called ZSNES). ZSNES is GPL and piccolo is MIT: never copy
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
- `deps/common/retroui`             retroui: the look shared with piccoletto: the 5x5 pixel font, themes,
                                     themed ImGui widgets (menu bar from data, windows, buttons...), layers over
                                     the game, pad picture, ports_view (the ports' tabs and DEVICES board, same in
                                     piccoletto) + port_indicators.h; `pixel_font_preview` (its test checks the font)
- `deps/libretro/host-libretro`     host-libretro: the libretro host (no GUI), libretro_core_info, tests
- `deps/libretro/frontend-libretro` frontend-libretro: core session, core options, core catalog
- `deps/test-libretro`              test-libretro (the Test libretro core), optional (`update = none`)
- `deps/cores`                      multitap-org/cores: every core's recipe (source, patches, build per platform);
                                     the browser build builds its cores with it (`update = none`, web only).
                                     A core recipe changes THERE (all platforms, web included), never in piccolo
- `deps/beacon`                     multitap-beacon (multitap-org/multitap-beacon, MIT): the introduction server for
                                     connections across the internet (STUN-like + hole punching, never a relay);
                                     frontends-common builds only its `beacon_wire` library (tests also its server code)
- `deps/libdatachannel`             libdatachannel v0.24.6 (MPL-2.0, accepted 2026-11-20): WebRTC data channels natively
                                     (desktop, Android meet browsers); static, no WebSocket / media; OpenSSL for DTLS.
                                     Its own submodules: plog, usrsctp, libjuice (`git submodule update --init` there)
- `deps/piccolo-shaders`            piccolo-shaders: the curated slang presets (branch `piccolo` of a trimmed
                                     slang-shaders, per-file licenses), copied to shaders/presets/; optional.
                                     Rebased on upstream: tag the pinned commit (piccolo-YYYY-MM-DD) first
- third-party: `deps/SDL`, `deps/imgui`, `deps/libretro-common`, `deps/libsamplerate`,
  `deps/gamemode`, `deps/librashader`, `deps/miniz` (zip reading only), `deps/7zip`
  (only its C 7z decoder + SHA-256, no PPMd), `deps/enet` (zpl-c's ENet: UDP connections, IPv6 dual-stack), `deps/monocypher`
  (X25519, ChaCha20-Poly1305, BLAKE2b, SHA-512 from optional/; EdDSA for the core downloader's
  signatures), `deps/qrcodegen` (Nayuki's QR Code generator, MIT, v1.8.0: its C file only; the pair
  link as a QR); libcurl from the system (Linux) for the core downloader

Other repos: libretro-test-frontend (the host's test frontend, same libraries,
test-libretro always), multitap (README + STYLE.md: the family), imguisw (fork
used by test-libretro). Later: piccoletto (streaming companion, Android first),
Multitap (modern multi-host frontend).

**A library change:** edit it in its submodule (the five are on `main`), build
and test here, commit and push THERE first, then commit piccolo's pointer.
Code shared between frontends goes into frontends-common (plumbing) or retroui
(the look); code shared between libretro frontends into frontend-libretro.
Never copy code between frontends.
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
   to `docs/design-notes.md`), except `docs/compilation.md`: the build guide for
   other people, one section per platform, committed (keep it current when the
   build, its packages or its submodules change).
   Ask before anything destructive.

## Stack

- Rendering: raw Vulkan 1.3 (dynamic rendering, synchronization2), 1.1 path for
  1.1-only drivers. Not SDL_GPU. We own the swapchain. OpenGL ES fallback (GLES 3,
  GLES 2 with MULTITAP_GL_ES2) when Vulkan isn't usable, and WebGL 2 in the browser.
- Platform: SDL3 for window, input, audio only.
- UI: Dear ImGui (official SDL3 + Vulkan backends, unmodified), themed, with
  piccolo's pixel font. No RmlUi in piccolo.
- Shaders: our own pass 0 + librashader loaded at run time by our own loader
  (librashader's MIT C headers; ABI 2, API >= 5).
- Build: CMake ≥ 3.25, Ninja. Targets: Linux, Windows, Android (+ legacy armv7 GLES 2),
  the browser (Emscripten, pthreads); macOS (no CI, state unknown).

## Layout

```
CMakeLists.txt            top-level build: options, SDL, Vulkan, imgui, libretro_api, the libraries, piccolo
STYLE.md                  code rules (mandatory)
src/
  main.cpp                the app: game fills the window, menus, hotkeys, input frame -> core session
  menus.*                 menu contents and actions (data only; main builds retroui's menu bar data from them)
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
                          combos, macros, mouse / lightgun), SHADER (scope, preset with CHOOSE, FILTER; only here, not in SETTINGS)
  input_window.*          INPUT: SETTINGS, INTERFACE, HOTKEYS, DEVICES
  onscreen_pad.*          the touch gamepad (retroui's touch pad, piccoletto's layouts): TOUCH PAD in the router
                          (a RetroPad device), over the game (upright: under the picture if it fits), MENU =
                          QUICK MENU, SETTINGS > INPUT > ON-SCREEN GAMEPAD (SHOW / OPACITY / VIBRATE, EDIT LAYOUT)
  ports_widget.*          the ports over the game: fills retroui's ports_view (tabs that rise when a port
                          changes, peeking under the menu; DEVICES) from the router; DEVICES' pads (hotkey.ports:
                          each moves itself LEFT / RIGHT, port 1 picks any with UP / DOWN)
  map_window.*            MAP CONTROLLER wizard (the controller drawing: retroui's pad_picture)
  net_window.*            SETTINGS > NETWORK tab: listener settings, pairing code, paired devices
  net_games.*             games from devices with FULL access: cores, game folders, load (net_content)
  net_remote.*            settings (SETTINGS' pages, the device's language) and actions from devices (net_settings)
  headless.*              GO HEADLESS: a streaming device takes the game (window hidden behind a tray icon)
  core_options_ui.* safe_start_window.*
  shader_check.* shader_check_window.*   a preset just applied that's too slow: KEEP / REVERT (15)
  text_input_window.*     the text box for a core's TEXT request (libretro extension EXT_SET_TEXT_INPUT)
  cores_window.*          TOOLS > CORES: the core downloader's window (core_updater in frontend-libretro)
  share_window.*          TOOLS > SHARE: two panes (here / another piccolo with OWNER access), folders above,
                          files below, arrows in the middle copy games, saves and states (net_share_*)
  app_icon.* bundled_files_android.* bundled_files_web.*   window icon; files packed in the APK / linked core in the browser
layouts/                  controller layouts (<Maker>/<model>.cfg)
android/                  Android APK: build-apk.sh (--legacy: GLES 2, armeabi-v7a, Android 5+), res/ (icons,
                          banner), res-debug/ (debug variant)
web/                      the browser build (Emscripten, WebGL 2, pthreads): build-core.sh <core>
                          (runs deps/cores' recipe for the web: sameboy, snes9x), build.sh <core> (one wasm per core); src/bundled_files_web.cpp registers the linked core
  page/                   piccolo's page: core.html, piccolo-page.js (OPEN GAME, drops), piccolo-files.js
                          (the FILES panel); the shared half (the start, the folder on OPFS, sw.js) is
                          frontends-common's web/, and app_loop / app_storage its C++ side
  site/                   the websites' shared look (piccolo-site's assets/v1): package.sh fetches it into
                          _site/v1 (this committed copy without the network) and fills the start page's bar
                          and footer; PICCOLO_WEB_CHANNEL stable / dev (DEV pill + commit)
tools/                    strings_check, recent_games_test, stream_measure (remote play
                          codec on real games), netplay_measure (which netplay mode a core and game
                          suit: determinism, exact states, cost), lsan-suppressions.txt
deps/common/hosts-common/     host_common: host_log, host_fs (mkdir -p, read, atomic write; the only
                              <filesystem> user), dynamic_library; cmake/multitap.cmake
deps/common/frontends-common/ frontend_common: app_shell (SDL + window + Vulkan + swapchain + renderer + ImGui), app_loop (who drives
                              the loop: a while, or the browser's animation frames) / app_storage (the folder on
                              OPFS in the browser) + web/ (the pages' shared scripts, sw.js),
                              video_texture / video_output (what frontends draw the picture with: Vulkan's
                              picture + shader_chain, or the GLES path's gl_texture + gl_picture),
                              gl_context / gl_texture / gl_picture / gl_shader_chain (the GLES fallback when Vulkan
                              isn't usable: docs/gl.md), app_shell_vulkan / debug_ui_vulkan (+ _none without Vulkan),
                              vk_* (context, swapchain, texture, barrier), renderer, debug_ui, text_input (the
                              on-screen keyboard), audio_output (+DRC), game_mode (Linux),
                              frame_timing, frame_delay, rate_control, frame_delta (lossless
                              frame codec for remote play), settings_file / settings (layers global ->
                              core -> game), app_paths, viewport, picture, window_size, localization,
                              input_devices / input_bindings (the router) / input_config / input_sdl /
                              input_sdl_mapping / input_layouts, archive (zip/7z, "a.7z#file" paths) /
                              content_cache (extracted files, LRU); frontend_download (signature, net_http,
                              file_hash: the core downloader; net_http over libcurl on Linux, WinHTTP on
                              Windows, HttpURLConnection through JNI on Android);
                              frontend_net (optional, with deps/enet):
                              net_host (ENet + LAN discovery on one UDP port), net_noise (Noise XX),
                              net_link / net_session (encrypted channels per peer), net_cpace (PAKE),
                              net_trust / net_pairing / net_access (host) / net_client (6-digit pairing,
                              on the LAN or across the internet with net_pair_link: the link / QR,
                              access levels gamepad / stream / full), net_input + input_net (remote pads
                              in the router, game-only), net_content (browse / load games: messages) +
                              net_content_folders (host: game folders as virtual paths, never left),
                              net_rtc (WebRTC under NetHost: net_rtc_datachannel / _emscripten (the browser,
                              net_webrtc.js; WebRTC only there) / _none; peers
                              carry their transport, the same channels and bytes as ENet),
                              net_beacon (across the internet: register with the beacon, meet,
                              punch, peer-reflexive), net_signal (WebRTC signaling over the beacon's
                              WebSocket), net_share / _client / _host / _engine / _folders / _link (SHARE:
                              listing and copying files, OWNER access), net_client_service (the client's
                              connection on its own thread, for piccoletto), net_service (the network on its own thread; remote play is coded and sent
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
git -C piccolo submodule update --init --checkout deps/cores                       # optional, for the browser build
for d in deps/common/hosts-common deps/common/frontends-common deps/common/retroui deps/libretro/host-libretro deps/libretro/frontend-libretro; do git -C piccolo/$d checkout main; done
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
web/build-core.sh snes9x && web/build.sh snes9x  # wasm (emsdk 6.0.13 sourced, deps/cores checked out): web/build/wasm/<core>/
web/package.sh                     # the zip with every core (sameboy and snes9x built first)
android/build-apk.sh [--legacy]    # --legacy: armv7, Android 5+, GLES 2 only (-DMULTITAP_GPU_VULKAN=OFF), the SHIELD Portable
./build/deps/libretro/host-libretro/libretro_core_info [--frames N] [--set KEY=VALUE]... <core> [content]
```

100 tests with test-libretro. Before calling a larger change done:
warnings-as-errors clean, ctest in both builds, no Vulkan validation messages,
no ASan/UBSan reports beyond `tools/lsan-suppressions.txt`, resizing and
quitting work. Benchmarks: Release build (warnings-as-errors OFF: two harmless
-O2 warnings), `XDG_DATA_HOME=<scratch>`, `taskset -c 4-7` on the RK3588.
Low-end test target: the RK3588 ARM board (Mali-G610, Mesa panvk).

## Current state (2026-11-20)

Everything below is on main (piccolo ec86c7f); main CI (Flatpak, Windows, Android, Web) is green.

- piccolo 0.3: plays games through frontend-libretro; LOAD GAME, RECENT,
  SETTINGS (global / core / game layers), IMAGE SETTINGS, QUICK MENU
  (SAVESTATES 1-9 with thumbnails, CORE OPTIONS, CONTROLS, SHADER), fast
  forward, native pixel formats, frame delay, RUN-AHEAD (one setting, for cores
  with their own run-ahead), localization (es), games from zip / 7z (extracted
  into <root>/cache, trimmed to 4 GiB; cores never get virtual paths). Retro UI
  scale: the biggest block that leaves 640 x 360. Window title shows the branch
  on builds from a branch other than main.
- Rendering: Vulkan, or the GLES fallback (renderer split behind video_texture /
  video_output; MULTITAP_GPU=gl forces it). Legacy APK (armv7, Android 5+, GLES 2)
  runs at 60 fps on the SHIELD's Tegra X1; a real Tegra 4 is untested. Open
  decision (docs/gl.md): Vulkan uses-feature required="false" in the main APK.
- Shaders: frontends-common's shader_chain loads librashader at run time (our
  own loader; ABI 2, API >= 5). Stage 0 = pass 0 (PICTURE CONTROLS + color
  profiles: GBA, GBC, DS, PSP, ... NTSC), stage 1 = the `video_shader_preset`
  setting or piccolo's scale preset for FILTER. Curated presets from
  deps/piccolo-shaders, chosen in QUICK MENU > SHADER; a preset too slow for the
  machine asks KEEP / REVERT (15). GLES: piccolo's own presets in shaders/gl/.
  librashader is in the Flatpak and the main APK (not the legacy one). Without
  librashader the plain picture path draws. Not done: preset parameters as
  knobs, frame blending.
- libretro extensions (Multitap's private environment calls, host-libretro
  `src/libretro_extensions.h`): run-ahead (0x4D01/02) and text input (0x4D03:
  KEYS opens the system's on-screen keyboard only when the core asks, TEXT shows
  piccolo's text box). Multitap adapters stay on the unmerged `multitap`
  branches until real cores are patched. Planned: media slots. Subsystems
  (retro_load_game_special, per-cart save RAM) are in host-libretro; no UI yet.
- On-screen keyboard: INPUT > SETTINGS > ON-SCREEN KEYBOARD (AUTO: phones, TVs,
  handhelds), for ImGui fields and core requests, Vulkan and GLES. Left:
  Windows' touch keyboard, KDE's through D-Bus, a network hook for piccoletto.
- TOOLS > CORES: the core downloader (core_updater in frontend-libretro, signed
  index; cores built in multitap-org/cores, served by cores-server); SHOW
  NON-FREE CORES off by default, confirmed when turned on; works on Linux,
  Windows (WinHTTP) and Android (JNI).
- Input (done for this version): N:N ports (hotkeys and UI from port 1 + the
  keyboard), per-core remaps with per-game copies, combos and macros (recorder)
  per core, turbo (row box + TURBO BUTTON for the session), mouse / pointer /
  lightgun from one cursor per port (several mice), MAP CONTROLLER wizard,
  layouts, numbered controller names, PORTS setting, PAUSE WHEN CONTROLLERS
  DISCONNECT. Left for later: touch (drag mode, DS pointer), crosshair,
  Joy-Cons, per-port unlink, radial menu, overlay, on-screen messages redesign.
- Untested on hardware: SN30 over USB (unknown pad), Genesis / Saturn layouts,
  Shield touchpad pointer, mouse grab, lightguns (Zapper, Super Scope).
- Network (pending work in docs/netcode-pending.md): on its own thread
  (net_service); one UDP port (zpl-c ENet, IPv6 dual-stack) with LAN discovery,
  Noise XX encryption, 6-digit pairing, access levels gamepad / stream / full /
  owner (level changes reach a connected device at once); remote pads, what
  runs, actions, settings, controllers per port, DEVICES, game folders browsed
  and games loaded from the client. Remote play: piccolo streams the frame it
  shows (lossless, frame_delta) and the audio it plays; GO HEADLESS hands the
  game to the streaming device (tray icon). Across the internet: the beacon
  (repo multitap-beacon, beacon.get-piccolo.app: UDP rendezvous + STUN +
  WebSocket signaling rooms); WebRTC data channels with libdatachannel (desktop;
  not on Android yet). Left: the browser's WebRTC backend, QR / link pairing
  through the beacon, TURN for symmetric NATs, netplay. Known host bugs (from
  piccoletto): a network pad's START can't leave the pause; a stream started
  while paused gets no frame.
- SHARE (TOOLS > SHARE, OWNER access): steps 1-9 done (games with their save and
  states, REPLACE?, net_probe as a headless client / host). Untested between two
  real piccolos.
- Browser (web/, docs/wasm.md): WebGL 2, one page per core (SameBoy, Snes9x) with
  the core linked statically, piccolo's folder in memory kept on OPFS by the page, pthreads (COOP/COEP: the zip ships .htaccess / nginx / _headers configs
  plus coi-serviceworker; web/docker for a ready server). Published at
  get-piccolo.app/play/. frontend_net is compiled in but inert. Untested:
  Firefox / Safari, real fps, gamepad.
- Packaging (docs/packaging.md): app ID site.multitap.piccolo. CI builds the
  Flatpak (librashader and libgamemode bundled), the Windows zip (MSYS2 UCRT64,
  x64 and ARM64), the Android APKs (main + legacy) and the web zip / Docker
  image. With PICCOLO_KEYSTORE a release build, without it a debug variant;
  CI signs with the ANDROID_KEYSTORE_* secrets when they're set (not set yet).
  release.yml turns a v* tag into a GitHub Release (PICCOLO_VERSION_BASE must
  be raised to 0.4.0 first). Releases site and self-update: designed, not built.
  Branding: assets/branding. Site: get-piccolo.app (repo multitap-org/piccolo-site).
- In progress on branches (not on main): `rewind` (piccolo-rewind: SCRUB /
  SNAPSHOT / AUTO, DVR-style PLAY, input replay, export to video; plan in
  docs/rewind.md). Planned, not started: console / TV UI and phone UI
  (docs/mockups), Android SAF.
- Release plan (early December): piccolo (Linux Flatpak, Windows, Android) +
  piccoletto (Android: gamepad, remote play, devices). The `big-picture`,
  `retroui` and old `android` branches are stale (far behind main).
- Known: Release builds fail with warnings-as-errors (GCC -O2 only:
  `instance_layer_available` unused outside Debug, a -Wformat-truncation in a
  log note). frontend_net_signal failed once under ASan with -j4 (passes alone).
  Pending: imgui docking branch.

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
