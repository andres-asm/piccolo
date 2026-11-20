#pragma once

// The question when a shader preset just applied is too slow (shader_check.h):
//
//   +- SHADER TOO SLOW -----------------------------------+
//   | CRT-ROYALE RUNS AT 41 FPS. THE GAME NEEDS 60.       |
//   | KEEP IT?                                            |
//   |                          [KEEP] [REVERT (15)]       |
//   +-----------------------------------------------------+
//
// The game keeps running behind it, so the preset can be seen while deciding.
// REVERT is chosen by itself when the countdown ends (and by Esc / the close
// button): a preset too slow to use the menu with is undone without help.

#include <cstdint>

#include "localization.h"

struct ImFont;

constexpr uint64_t SHADER_CHECK_ANSWER_MS = 15000;

enum class ShaderCheckChoice
{
    none,     // still open
    keep,
    revert,
};

// Draws the dialog (centered). `preset`: the preset's name; measured_fps and
// target_fps for the text; asked_ms / now_ms: when it was asked, for REVERT's
// countdown.
ShaderCheckChoice shader_check_window_draw(ImFont* pixel_font, int32_t scale, const Localization* strings,
                                           const char* preset, double measured_fps, double target_fps,
                                           uint64_t asked_ms, uint64_t now_ms);
