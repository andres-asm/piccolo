// Checks the slow shader check (shader_check.h): a fast preset goes back to
// idle, a slow one asks, a game that was already slow isn't blamed on the
// preset, and gaps (paused, menus) aren't samples.
//
// Usage: piccolo_shader_check_test
// Exit code 0 = all checks passed. Each failure is printed.

#include <cstdint>
#include <cstdio>

#include "shader_check.h"

static int g_failures = 0;

static void check(bool condition, const char* what)
{
    if (!condition)
    {
        std::printf("FAIL %s\n", what);
        g_failures++;
    }
}

constexpr double   TARGET_NS = 16666667.0;   // 60 fps
constexpr uint64_t FAST_NS   = 16666667;
constexpr uint64_t SLOW_NS   = 25000000;      // 40 fps

// Runs `loops` loops of `length_ns` each; true if any of them asked.
static bool run(ShaderCheck* check_state, uint64_t* now_ns, uint32_t loops, uint64_t length_ns)
{
    bool asked = false;
    for (uint32_t i = 0; i < loops; i++)
    {
        *now_ns += length_ns;
        if (shader_check_loop(check_state, true, *now_ns, TARGET_NS))
        {
            asked = true;
        }
    }
    return asked;
}

constexpr uint32_t WHOLE_CHECK = SHADER_CHECK_SETTLE_LOOPS + SHADER_CHECK_MEASURE_LOOPS + 1;

int main()
{
    // A fast preset: no question, back to idle.
    {
        ShaderCheck state = {};
        uint64_t now = 1000000000;
        run(&state, &now, 200, FAST_NS);
        check(state.baseline_ns > 16000000.0 && state.baseline_ns < 17500000.0, "baseline follows the loops");
        shader_check_start(&state);
        check(!run(&state, &now, WHOLE_CHECK, FAST_NS), "fast preset: no question");
        check(state.state == ShaderCheckState::idle, "fast preset: idle again");
    }

    // A slow preset after a game that ran at full speed: asked once.
    {
        ShaderCheck state = {};
        uint64_t now = 1000000000;
        run(&state, &now, 200, FAST_NS);
        shader_check_start(&state);
        check(!run(&state, &now, SHADER_CHECK_SETTLE_LOOPS + 10, SLOW_NS), "not while settling");
        check(run(&state, &now, SHADER_CHECK_MEASURE_LOOPS, SLOW_NS), "slow preset: asked");
        check(state.state == ShaderCheckState::asking, "slow preset: asking");
        check(state.measured_ns > 24000000.0 && state.measured_ns < 26000000.0, "measured average");
        check(!run(&state, &now, 300, SLOW_NS), "asked only once");
        shader_check_stop(&state);
        check(state.state == ShaderCheckState::idle, "answered: idle");
    }

    // Slow before the preset too: not the preset's fault.
    {
        ShaderCheck state = {};
        uint64_t now = 1000000000;
        run(&state, &now, 400, SLOW_NS);
        shader_check_start(&state);
        check(!run(&state, &now, WHOLE_CHECK, SLOW_NS), "already slow: no question");
    }

    // The settling loops absorb a slow start (the chain's first frames).
    {
        ShaderCheck state = {};
        uint64_t now = 1000000000;
        run(&state, &now, 200, FAST_NS);
        shader_check_start(&state);
        run(&state, &now, SHADER_CHECK_SETTLE_LOOPS, SLOW_NS * 4);
        check(!run(&state, &now, SHADER_CHECK_MEASURE_LOOPS + 1, FAST_NS), "slow start, then fast: no question");
    }

    // Paused loops and long gaps aren't samples: a pause in the middle
    // doesn't make a fast preset look slow.
    {
        ShaderCheck state = {};
        uint64_t now = 1000000000;
        run(&state, &now, 200, FAST_NS);
        shader_check_start(&state);
        run(&state, &now, SHADER_CHECK_SETTLE_LOOPS + 50, FAST_NS);
        for (uint32_t i = 0; i < 100; i++)
        {
            now += FAST_NS;
            shader_check_loop(&state, false, now, TARGET_NS);   // paused
        }
        now += SHADER_CHECK_GAP_NS * 4;   // a stall
        check(!shader_check_loop(&state, true, now, TARGET_NS), "a gap isn't a sample");
        check(!run(&state, &now, SHADER_CHECK_MEASURE_LOOPS, FAST_NS), "pause and gap: no question");
        check(state.state == ShaderCheckState::idle, "pause and gap: idle again");
    }

    // A check started with nothing measured before still asks when slow.
    {
        ShaderCheck state = {};
        uint64_t now = 1000000000;
        shader_check_start(&state);
        check(run(&state, &now, WHOLE_CHECK, SLOW_NS), "no baseline: slow preset asked");
    }

    if (g_failures == 0)
    {
        std::printf("shader_check: all checks passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
