# Netcode: state and pending work (handoff, 2026-11-19)

Branch-only file: written by the session that built milestones 1-4, for the
session (with a coordinator) that continues. **Delete it, and
`tools/netcode/rebase_resolve.py`, before merging this branch to main.**

The decisions and their reasons are in `docs/design-notes.md` (untracked, in
the main checkout), section "Netcode, VFS and new dependencies (decided
2026-11-19)" and the bullets after it: read that first. This file is the state
of the branches and what's left.

## Branches (all pushed, NOT merged to main: Andrés merges when he says)

| Repo | Branch | Tip when written |
|---|---|---|
| piccolo | `claude/piccolo-netcode-3a7e8d` | this commit |
| frontends-common | `netcode` | `832dbb2` |
| hosts-common | `netcode` | `26dab90` (only `host_fs_write_private_file`) |

- Rebased onto main on 2026-11-19 (after `archives` and 347bbc4). main keeps
  moving: rebase again right before merging. Libraries first: frontends-common
  conflicts in `CMakeLists.txt` (the archives block and the ENet block both end
  inside their own `if`; keep both blocks whole, one after the other). Then
  piccolo: each of its commits conflicts on `.gitmodules`, the workflows,
  `CLAUDE.md` and the frontends-common pointer. `tools/netcode/rebase_resolve.py`
  resolves one step (union of submodules and workflow lines, main's CLAUDE.md,
  the pointer moved to the rebased library commit with the same subject); update
  its subject table for commits added since. Afterwards re-add the network lines
  to CLAUDE.md and fix its test count.
- After a rebase, submodules can be left on a detached HEAD: check out the
  branch before committing there (it happened once: a commit wasn't pushed).
- Merge order when Andrés says: hosts-common and frontends-common `netcode` into
  their `main` (ff-only after a rebase), push; then piccolo's branch with the
  pointers on those `main`s, push.
- New submodules: `deps/enet` (lsalzman, v1.3.18), `deps/monocypher` (4.0.3).
  The CI workflows (flatpak, windows) already init them.

## Checks

- `build/` and `build-asan/`: warnings as errors clean, **41 tests** (60 with
  test-libretro), all pass in both. Network tests: `frontend_net` (discovery,
  sessions, pairing, access levels, memory), `frontend_net_noise` (Noise and
  CPace vectors, links), `frontend_input_net` (remote pads, game-only rules).
- Builds in a worktree don't install to `~/.local/share/piccolo` (347bbc4): run
  `./build/piccolo` from the worktree.
- Tried in the app by Andrés: NETWORK window (layout approved 2026-11-19),
  pairing with `net_probe --pair`, reconnecting, the identity kept across
  launches.
- Not tried: the Windows build (Winsock, BCryptGenRandom), two real machines
  with a real phone, a remote pad joining a port by hand in a running game
  (checked with the scripted `net_probe --pad`: "NET PROBE 1: PORT 1").

## What exists (frontends-common `frontend_net`, optional with deps/enet)

- `net_host`: ENet on one UDP port; LAN discovery through ENet's intercept;
  channels control (reliable) and input (unreliable sequenced).
- `net_memory`: ENet's allocations from reused size classes (0 system blocks
  per frame once warmed up; measured in the test).
- `net_noise` (Noise_XX_25519_ChaChaPoly_BLAKE2b), `net_link` / `net_session`
  (encrypted channels per peer, handshake deadline), `net_random`.
- `net_cpace` (CPACE-X25519-SHA512, draft 18), `net_trust` (identity file 0600,
  paired devices with access levels), `net_pairing` (messages), `net_access`
  (host: welcome, 6-digit pairing window, access enforced by message type),
  `net_client` (client).
- `net_input` + `input_net`: remote pads as router gamepads (game-only: never
  the interface; on port 1 they get SAVE STATE, LOAD STATE, FAST FORWARD).
- `tools/net_probe`: `--listen NAME`, `--pair ADDRESS CODE`, `--connect ADDRESS`
  (`--pad`), `--at`, broadcast discovery, `--dir`.
- piccolo: `src/net_window.*` (CONFIG > NETWORK), the `net_*` functions in
  `main.cpp`, settings `net_enabled` / `net_port` / `net_name`, files
  `settings/net_identity.cfg` and `settings/net_devices.cfg`;
  `retro_ui_choice_buttons` (new widget: a row of buttons, the chosen pressed).

## Message types (control channel, first byte)

- 1-7: the access layer (welcome, pairing_needed, pair_request / response /
  confirm / result, device_name). Don't reuse.
- 16-63 need gamepad, 64-127 stream, 128-255 full: `net_access` drops what a
  device's level doesn't cover. Give new application messages a type in the
  range of the level they need. Input channel: type 1 = pad states.

## Pending, in order

1. **Milestone 5: load games from the client (access full).**
   - Host: a base folders setting (a list of paths); the client browses only
     inside them. Canonicalize in `host_fs` (resolve `..` and symlinks, check
     the prefix) and refuse anything outside. Browse inside zip / 7z with the
     archives code (`archive.h`, "a.7z#file" paths).
   - Messages (types 128+): list a folder (paged: a control message is capped
     at NET_LINK_MAX_PAYLOAD, a big folder needs pages), its result, load a game
     (core + path), its result. Cores: the host's catalog filtered by the file's
     extension, like LOAD GAME.
   - Tests over loopback, including paths that try to escape.
2. **Configuring piccolo from the app (access full)**, later (Andrés): its own
   control messages (settings, load, quick menu actions), never pad input.
3. **Milestone 6: streaming (access stream) and headless (access full)**, with
   piccoletto. The core's frame at native size, near-lossless; shaders on the
   client (design-notes, piccoletto section).
4. **Milestone 7: internet.** STUN on the ENet socket (through `intercept`);
   rendezvous over HTTPS (curl) with one code for finding and pairing, Magic
   Wormhole style (e.g. 8 digits: 2 for the rendezvous slot, 6 for CPace);
   TURN; IPv6 (upstream ENet is IPv4 only: revisit a fork then); rollback
   netplay in host-libretro (savestate ring, resimulation, per-port RetroPad
   frames as their own input message type, state hash).
5. Discord lobbies, later.

## Open points / known limits

- Only the side that refuses a connection closes it (both closing at once left
  ENet peers stuck in DISCONNECTING). When pairing is closed the host sends
  `pairing_needed` and closes; a client with a code waits for that.
- A remote pad's guid is `net:<8 key bytes hex>:<pad>`; up to 4 pads a device.
- `frontend_net` links `frontend_common` for `settings_file`: it calls no SDL
  itself but links it. If a host process ever needs networking, move
  `settings_file` down or give net_trust its own format.
- The NETWORK window's strings are placeholders: Andrés reviews them with
  localization.
- piccolo's default `net_name` is "PICCOLO".
