#include "shader_check.h"

void shader_check_start(ShaderCheck* check)
{
    check->state = ShaderCheckState::settling;
    check->before_ns = check->baseline_ns;
    check->loops = 0;
    check->total_ns = 0.0;
    check->measured_ns = 0.0;
}

void shader_check_stop(ShaderCheck* check)
{
    check->state = ShaderCheckState::idle;
    check->loops = 0;
    check->total_ns = 0.0;
    // The baseline starts over: what's applied now is the new normal.
    check->baseline_ns = 0.0;
}

bool shader_check_loop(ShaderCheck* check, bool ran_frames, uint64_t now_ns, double target_ns)
{
    if (!ran_frames)
    {
        check->last_ns = 0;   // the next loop that runs frames has nothing to compare with
        return false;
    }
    uint64_t previous = check->last_ns;
    check->last_ns = now_ns;
    if (previous == 0 || now_ns <= previous || now_ns - previous > SHADER_CHECK_GAP_NS)
    {
        return false;
    }
    double length = static_cast<double>(now_ns - previous);

    switch (check->state)
    {
        case ShaderCheckState::idle:
            check->baseline_ns = check->baseline_ns == 0.0
                                     ? length
                                     : check->baseline_ns + (length - check->baseline_ns) * SHADER_CHECK_BASELINE_RATE;
            return false;
        case ShaderCheckState::settling:
            check->loops++;
            if (check->loops >= SHADER_CHECK_SETTLE_LOOPS)
            {
                check->state = ShaderCheckState::measuring;
                check->loops = 0;
                check->total_ns = 0.0;
            }
            return false;
        case ShaderCheckState::measuring:
        {
            check->loops++;
            check->total_ns += length;
            if (check->loops < SHADER_CHECK_MEASURE_LOOPS)
            {
                return false;
            }
            double average = check->total_ns / static_cast<double>(check->loops);
            bool over_target = average > target_ns * SHADER_CHECK_SLOW_RATIO;
            bool over_before = check->before_ns == 0.0 || average > check->before_ns * SHADER_CHECK_SLOW_RATIO;
            if (over_target && over_before)
            {
                check->state = ShaderCheckState::asking;
                check->measured_ns = average;
                return true;
            }
            // Fast enough: the measurement becomes the baseline.
            check->state = ShaderCheckState::idle;
            check->baseline_ns = average;
            check->loops = 0;
            return false;
        }
        case ShaderCheckState::asking:
            return false;
    }
    return false;
}
