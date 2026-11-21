# Building piccolo

How to build piccolo from source, one section per platform. Every dependency
except the toolchain and the Vulkan SDK headers is a git submodule in `deps/`,
so a checkout with its submodules builds on its own.

- [Windows (MSYS2)](#windows-msys2)

## Windows (MSYS2)

piccolo is built natively on Windows with [MSYS2](https://www.msys2.org/):
the **UCRT64** environment (GCC, libstdc++ and the Universal CRT) on x86_64,
**CLANGARM64** (clang, libc++) on ARM64. The C/C++ runtimes are linked
statically, so the resulting `piccolo.exe` needs only Windows' own DLLs and
`vulkan-1.dll`, which comes with the GPU driver. The same steps are what CI
runs (`.github/workflows/windows.yml`).

### 1. Install MSYS2 and the packages

Install MSYS2 from <https://www.msys2.org/>, then open the **MSYS2 UCRT64**
shell (not "MSYS2 MSYS") and update everything first:

```sh
pacman -Syu
```

If it closes the shell to update itself, open UCRT64 again and run
`pacman -Syu` once more until nothing is left.

Then the packages piccolo needs:

```sh
pacman -S --needed git \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-vulkan-devel \
    mingw-w64-ucrt-x86_64-rust
```

| Package        | What for |
|----------------|----------|
| `git`          | the checkout and its submodules |
| `gcc`          | the compiler (C and C++) |
| `cmake`, `ninja` | the build (CMake 3.25 or newer) |
| `vulkan-devel` | the Vulkan headers and loader to link against |
| `rust`         | only for `librashader.dll` (shader presets, step 5); optional |

On ARM64 use the **MSYS2 CLANGARM64** shell and the `mingw-w64-clang-aarch64-`
prefix, with `clang` instead of `gcc`.

Never install a package with `pacman -Sy <package>` (refresh without
upgrading): MSYS2 doesn't support partial upgrades, and the new package can
need newer libraries than the ones installed (rust, for example, then fails to
start because it looks for a newer `libLLVM-NN.dll`). If that happens, run
`pacman -Syu`.

### 2. Get the source

Check out with LF line endings: themes, layouts and translations are copied
into the piccolo folder as they are.

```sh
git config --global core.autocrlf false
git clone https://github.com/multitap-org/piccolo.git
cd piccolo
```

The submodules the Windows build uses (`deps/librashader` without
`--recursive`: its own submodules are only its tests):

```sh
git submodule update --init --recursive \
    deps/SDL deps/imgui deps/libretro-common deps/libsamplerate \
    deps/gamemode deps/miniz deps/7zip deps/enet deps/monocypher deps/beacon \
    deps/piccolo-shaders deps/common/hosts-common deps/common/frontends-common \
    deps/common/retroui deps/libretro/host-libretro deps/libretro/frontend-libretro
git submodule update --init deps/librashader
```

Optionally the Test libretro core (every package ships it, and the tests use
it). Its nested submodules have SSH URLs; without a GitHub SSH key, fetch them
over HTTPS for this one command:

```sh
git -c url."https://github.com/".insteadOf="git@github.com:" \
    submodule update --init --recursive --checkout deps/test-libretro
```

### 3. Configure and build

From the UCRT64 shell, in the checkout:

```sh
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++" \
    -DCMAKE_SHARED_LINKER_FLAGS="-static -static-libgcc -static-libstdc++" \
    -DCMAKE_MODULE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
cmake --build build
```

The three linker flags give `piccolo.exe` and `test_libretro.dll` the static
C/C++ runtime; without them they need `libstdc++-6.dll`, `libgcc_s_seh-1.dll`
and `libwinpthread-1.dll` from MSYS2 next to them.

After building, piccolo is copied to its folder, `%USERPROFILE%\piccolo` by
default: `piccolo.exe`, `lang\`, `layouts\`, `themes\`, `shaders\` and
`cores\test_libretro.dll`. Files already there (saves, states, settings,
cores) are left alone. Useful options:

| Option | Meaning |
|--------|---------|
| `-DMULTITAP_PICCOLO_DIR=C:/path/piccolo` | copy piccolo somewhere else (empty: don't copy) |
| `-DMULTITAP_COPY_FROM_WORKTREE=ON` | copy also when building from a git worktree (off by default, so parallel worktrees don't overwrite the installed piccolo) |
| `-DCMAKE_BUILD_TYPE=Debug` | debug build; turn on `-DMULTITAP_WARNINGS_AS_ERRORS=ON` while developing (Release builds give two harmless GCC warnings) |
| `-DMULTITAP_BUILD_TESTS=OFF` | skip the tests |

### 4. Run the tests (optional)

```sh
ctest --test-dir build --output-on-failure
```

### 5. librashader (optional: shader presets)

piccolo loads `librashader.dll` at run time from its own folder. Without it
the picture is drawn without shader presets. Only its C API with the Vulkan
runtime is built:

```sh
RUSTFLAGS="-C target-feature=+crt-static" CARGO_TARGET_DIR=build-librashader \
    cargo build --manifest-path deps/librashader/Cargo.toml -p librashader-capi \
    --release --locked --no-default-features --features runtime-vulkan
cp build-librashader/release/librashader_capi.dll "$USERPROFILE/piccolo/librashader.dll"
cp deps/librashader/LICENSE.md "$USERPROFILE/piccolo/librashader-LICENSE.md"
```

`+crt-static` keeps the DLL from needing a C runtime DLL. CI builds it instead
with Rust from [rustup](https://rustup.rs/) (the MSVC target, which also needs
the Visual Studio Build Tools); the same command works from PowerShell there.

### 6. Check the DLLs (optional)

Neither should list an MSYS2 DLL (`libstdc++`, `libgcc`, `libwinpthread`,
`SDL3`, ...), only Windows' own and `vulkan-1.dll`:

```sh
objdump -p "$USERPROFILE/piccolo/piccolo.exe" | grep 'DLL Name'
objdump -p "$USERPROFILE/piccolo/librashader.dll" | grep 'DLL Name'
```
