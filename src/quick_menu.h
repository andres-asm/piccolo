#pragma once

// piccolo's quick menu (F1 while a game runs; decided by Andrés, plan-milestones
// milestone 4, key changed from TAB to F1 on 2026-11-14):
//
//   +- QUICK MENU ----------------------------------------------+
//   | [1] [2] [3] [4] [5] [6] [7] [8] [9]                        |   slots, used ones lit
//   | +--------------+  SLOT 3                                    |
//   | |  thumbnail   |  2026-11-14 15:32                          |
//   | +--------------+  [SAVE] [LOAD]                             |
//   | [RESET] [CLOSE GAME]                                        |
//   | > CORE OPTIONS                                              |   the running core's, live
//   | > CONTROLS                                                  |
//   | > SHADER                                                    |   the preset (CHOOSE) and FILTER
//   +-----------------------------------------------------------+
//
// Slots 1..9: arrows or 1-9 pick one (also the slot F2 / F4 use), S saves, L
// loads; Enter does nothing by itself, so a state is never loaded or
// overwritten by accident. Each slot shows its thumbnail (the PNG saved next
// to the state, at the aspect in use: core_session.h) and date. The game
// pauses while it's open (PAUSE WHILE THE MENU IS OPEN). F1 or Esc closes it.

#include <cstdint>
#include <vector>

#include "core_options_ui.h"
#include "core_session.h"
#include "input_bindings.h"
#include "input_layouts.h"
#include "input_sdl.h"
#include "settings.h"
#include "localization.h"
#include "video_texture.h"

struct AppShell;
struct ImFont;

enum class QuickMenuAction
{
    none,
    save,         // save to `slot`
    load,         // load `slot`
    reset,
    close_game,
    pause,        // PAUSE / RESUME: the player's own pause (main.cpp toggles it)
};

struct QuickMenu
{
    bool          open = false;
    int           tab = 0;              // 0 SAVESTATES, 1 CORE OPTIONS, 2 CONTROLS, 3 SHADER (for PAGE LEFT / RIGHT)
    int           open_on_tab = 0;      // the tab it opens on

    // CONTROLS can show the remap of one port's controller for the running
    // core and that port's device: its controls by the game's names, SET,
    // BIND ALL, DEFAULT ALL, and the core's combos bound to inputs. It is the
    // core's remap, or THIS GAME's when the game has its own (SCOPE). Changes
    // mark input_dirty (piccolo saves input.cfg).
    bool          remap = false;
    uint32_t      remap_port = 0;
    uint32_t      ports_selected = 0;   // CONTROLS: the port open (UINT32_MAX: none)
    int32_t       remap_section = 0;    // the remap's open section (one at a time; -1 none)
    uint32_t      remap_controller = 0;
    bool          capturing = false;
    InputAction   capture_action = InputAction::pad_b;
    uint32_t      capture_row = 0;      // the row (source) being set; its count = a new row (+)
    InputCoreProfile capture_backup;    // the remap as the SET / BIND ALL began (CANCEL)
    uint64_t      capture_started_ms = 0;
    bool          bind_all = false;
    bool          capture_seen = false; // the remap view looked at the capture this frame (else it's dropped)
    std::string   capture_combo;        // SET on a combo's row: its id ("" = on capture_action)
    InputAction   pad_action = InputAction::count;   // the row the drawn pad shows when not capturing (focused or hovered)
    std::string   pad_combo;            // ...or this combo's row (its id)
    uint32_t      pad_row = 0;          // which of its sources
    // COMBOS: making or changing one, in a modal (the core's, any game of it can bind it).
    bool          combo_editing = false;
    std::string   combo_edit_id;        // "" = a new one
    char          combo_edit_name[64] = {};
    uint16_t      combo_edit_buttons = 0;
    // MACROS: the same, in their own modal, and its recorder.
    bool          macro_editing = false;
    std::string   macro_edit_id;        // "" = a new one
    char          macro_edit_name[64] = {};
    std::vector<InputMacroStep> macro_edit_steps;
    bool          macro_edit_repeat = false;
    int32_t       macro_edit_selected = -1;   // the step whose buttons are shown (-1 none)
    int32_t       macro_change_steps = 3;     // CHANGE STEPS TO
    int32_t       macro_change_pauses = 3;    // CHANGE PAUSES TO
    int32_t       macro_record = 0;           // 0 off, 1 getting ready, 2 recording (the router suspended)
    uint64_t      macro_record_ms = 0;        // when getting ready began, then the last change
    bool          macro_record_seen = false;  // the modal looked at the recording this frame (else it's dropped)
    std::vector<uint64_t> macro_record_times;   // a change of the RetroPad buttons: when...
    std::vector<uint16_t> macro_record_buttons; // ...and to what
    bool          input_dirty = false;
    // SHADER: the settings scope it edits (GLOBAL / THIS CORE / THIS GAME,
    // the most specific one in use when the menu opens), and CHOOSE, which
    // piccolo answers by opening the shader picker for that scope.
    SettingLayer  shader_scope = SettingLayer::global;
    bool          shader_choose_scope = true;
    bool          choose_shader_request = false;
    uint32_t      slot = LIBRETRO_STATE_SLOT_FIRST;   // the selected slot
    StateSlotInfo slots[LIBRETRO_STATE_SLOT_LAST + 1] = {};   // [1..9]
    bool          refresh = true;   // read the slots again on the next draw
    bool          focus_slot = false;   // move the keyboard focus to `slot` on the next draw (picked by 1-9)

    // The selected slot's thumbnail: loaded from its PNG, uploaded on the next frame.
    VideoTexture          texture;   // texture.imgui_id: what ImGui draws
    std::vector<uint32_t> pixels;
    uint32_t              width = 0;
    uint32_t              height = 0;
    bool                  upload_pending = false;
    bool                  has_picture = false;   // the texture shows shown_slot
    uint32_t              shown_slot = 0;
    int64_t               shown_modified = 0;
};

struct QuickMenuContext
{
    const Localization*  strings = nullptr;
    const CoreSession*   session = nullptr;
    CoreOptionsUiState*  core_options_state = nullptr;
    CoreOptionsUiContext core_options;   // label_width and strings filled by the menu
    InputSystem*         input = nullptr;   // CONTROLS: who plays on which port
    LibretroHost*        host = nullptr;    // CONTROLS: the core's devices per port, and the one plugged in
    Settings*            settings = nullptr;   // CONTROLS: the device picked is kept per core
    const InputSdl*      sdl = nullptr;        // CONTROLS: button names
    const InputLayouts*  layouts = nullptr;    // CONTROLS: the controllers' button names
    uint64_t             now_ms = 0;
    const char*          core_name = "";         // SHADER: the scope selector's names
    const char*          game_name = "";
    bool                 shaders_available = false;   // librashader was found
    bool                 user_paused = false;         // the player paused: PAUSE reads RESUME
};

// Opens on `slot` (the session's current slot), on the tab it was last closed on.
void quick_menu_open(QuickMenu* menu, uint32_t slot);

// Opens on the CONTROLS tab (a controller went away: rearrange the ports).
void quick_menu_open_controls(QuickMenu* menu, uint32_t slot);

// Opens on CONTROLS showing the remap of `controller` on `port` (HOLD START
// TO REMAP was held).
void quick_menu_open_remap(QuickMenu* menu, uint32_t slot, uint32_t port, uint32_t controller);

// The slots changed on disk (a state was saved): read them again.
void quick_menu_refresh(QuickMenu* menu);

// Draws the menu while open; returns what was picked this frame.
QuickMenuAction quick_menu_draw(QuickMenu* menu, const QuickMenuContext* context, ImFont* pixel_font, int32_t scale);

// Uploads a newly loaded thumbnail. Call after app_shell_acquire, outside
// rendering (where the game's video is uploaded).
void quick_menu_upload(QuickMenu* menu, AppShell* shell);

// Frees the texture (waits for the GPU).
void quick_menu_destroy(QuickMenu* menu, AppShell* shell);
