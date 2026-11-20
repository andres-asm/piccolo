#pragma once

// Is a shader preset just applied too slow for the game? (Asked by Andrés,
// 2026-11-20.) After a preset change, the loops that run core frames are
// timed; when they take longer than the frame target, piccolo asks KEEP or
// REVERT (shader_check_window.h), and REVERT by itself when the countdown ends.
//
//   idle       the loop length is averaged all the time (the baseline: how
//              long a loop took before the change)
//   settling   the first SHADER_CHECK_SETTLE_LOOPS after the change aren't
//              counted (the chain's first frames, a load that took a while)
//   measuring  SHADER_CHECK_MEASURE_LOOPS are averaged
//   asking     too slow: the dialog is up until it's answered
//
// Too slow: the measured average is SHADER_CHECK_SLOW_RATIO over the target
// (a display refresh in Vsync timing, a core frame in Exact) and over the
// baseline too, so a game that was already slow without the preset isn't
// blamed on it. A gap longer than SHADER_CHECK_GAP_NS between two loops that
// ran frames (paused, a menu, a stall) isn't a sample.
//
// No SDL and no ImGui: the caller passes the time (unit tested in
// tools/shader_check_test.cpp).

#include <cstdint>

constexpr uint32_t SHADER_CHECK_SETTLE_LOOPS  = 30;
constexpr uint32_t SHADER_CHECK_MEASURE_LOOPS = 120;   // about 2 seconds at 60 fps
constexpr double   SHADER_CHECK_SLOW_RATIO    = 1.10;
constexpr uint64_t SHADER_CHECK_GAP_NS        = 250000000;   // 250 ms
constexpr double   SHADER_CHECK_BASELINE_RATE = 1.0 / 64.0;  // how fast the baseline follows

enum class ShaderCheckState
{
    idle,
    settling,
    measuring,
    asking,
};

struct ShaderCheck
{
    ShaderCheckState state = ShaderCheckState::idle;
    uint64_t last_ns     = 0;     // the last loop that ran frames; 0 = none yet
    double   baseline_ns = 0.0;   // average loop length while idle; 0 = not known
    double   before_ns   = 0.0;   // the baseline when the check started
    uint32_t loops       = 0;     // counted in this state
    double   total_ns    = 0.0;   // measuring: sum of the loop lengths
    double   measured_ns = 0.0;   // asking: the average that was too slow
};

// A preset was applied: start measuring (again, if a check was running).
void shader_check_start(ShaderCheck* check);

// Back to idle (answered, or the preset went away).
void shader_check_stop(ShaderCheck* check);

// Once per loop. ran_frames: core frames ran this loop (not paused, not fast
// forward); target_ns: the frame target (> 0). Returns true on the loop the
// preset is found too slow (the state is `asking` from then on).
bool shader_check_loop(ShaderCheck* check, bool ran_frames, uint64_t now_ns, double target_ns);
