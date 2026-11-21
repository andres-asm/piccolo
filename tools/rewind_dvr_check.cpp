// rewind_dvr_check: frontend-libretro's core_rewind on a real core and game,
// paced like a game (60 frames a second): what AUTO picks and why, then the
// DVR: back N seconds, WATCH to the present, and whether the replay was
// exact (snapshots: the determinism check at every snapshot).
//
// Usage: rewind_dvr_check [--seconds N] [--mode auto|scrub|snapshots] [--back N] [--scratch DIR] <core> <game>
//   --seconds  how long it plays first (default 30)
//   --back     how far the DVR goes back (seconds, default 20)

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "core_rewind.h"
#include "host_log.h"
#include "libretro_host.h"
#include "measure_common.h"

static double ms_since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int main(int argc, char** argv)
{
    uint32_t    seconds = 30;
    uint32_t    back = 20;
    RewindMode  mode = RewindMode::automatic;
    std::string scratch = "/tmp/rewind_dvr_check";
    const char* core = nullptr;
    const char* game = nullptr;
    for (int i = 1; i < argc; i++)
    {
        bool has_value = i + 1 < argc;
        if (std::strcmp(argv[i], "--seconds") == 0 && has_value)
        {
            seconds = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--back") == 0 && has_value)
        {
            back = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--mode") == 0 && has_value)
        {
            const char* value = argv[++i];
            mode = std::strcmp(value, "scrub") == 0       ? RewindMode::scrub
                 : std::strcmp(value, "snapshots") == 0   ? RewindMode::snapshots
                                                          : RewindMode::automatic;
        }
        else if (std::strcmp(argv[i], "--scratch") == 0 && has_value)
        {
            scratch = argv[++i];
        }
        else if (core == nullptr)
        {
            core = argv[i];
        }
        else
        {
            game = argv[i];
        }
    }
    if (core == nullptr || game == nullptr)
    {
        std::printf("usage: rewind_dvr_check [--seconds N] [--mode auto|scrub|snapshots] [--back N] [--scratch DIR] <core> <game>\n");
        return 2;
    }
    host_log_set_min_level(LogLevel::info);
    std::string content;
    if (!measure_content(game, &scratch, &content))
    {
        return 1;
    }
    LibretroHost* host = new LibretroHost{};
    LibretroHostConfig config;
    config.system_directory = scratch.c_str();
    config.save_directory = scratch.c_str();
    config.native_pixel_formats = true;
    if (!libretro_host_load_core(host, core, &config) || !libretro_host_load_game(host, content.c_str()))
    {
        std::printf("the core or the game didn't load\n");
        return 1;
    }
    double fps = host->av_info.timing.fps > 1.0 ? host->av_info.timing.fps : 60.0;

    CoreRewind* rewind = new CoreRewind{};
    rewind->settings.mode = mode;
    if (!core_rewind_start(rewind, host, fps))
    {
        std::printf("rewind didn't start\n");
        return 1;
    }

    // Play, paced like a game.
    uint32_t frames = static_cast<uint32_t>(seconds * fps);
    std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
    double period_ms = 1000.0 / fps;
    std::vector<double> record_ms;
    for (uint32_t frame = 0; frame < frames; frame++)
    {
        LibretroInputState input;
        input.joypad_buttons[0] = measure_script(frame);
        libretro_host_set_input(host, &input);
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        core_rewind_before_frame(rewind, &input);
        double recorded = ms_since(start);
        std::chrono::steady_clock::time_point run = std::chrono::steady_clock::now();
        libretro_host_run_frame(host);
        core_rewind_after_frame(rewind, static_cast<uint64_t>(ms_since(run) * 1e6));
        record_ms.push_back(recorded);
        double due = period_ms * (frame + 1);
        double now = ms_since(began);
        if (due > now)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int64_t>((due - now) * 1000.0)));
        }
    }
    const char* names[] = {"off", "unsupported", "measuring", "SCRUB", "SNAPSHOTS"};
    std::printf("mode: %s", names[static_cast<int>(rewind->state)]);
    if (rewind->measure.frames > 0)
    {
        std::printf(" (AUTO: serialize %.1f%%, load %.1f%%, encode %.1f%%, skipped %.1f%%)",
                    rewind->measure.record * 100.0, rewind->measure.load * 100.0, rewind->measure.encode * 100.0,
                    rewind->measure.skip * 100.0);
    }
    std::printf("\n");
    double worst = 0.0;
    double sum = 0.0;
    for (double value : record_ms)
    {
        worst = value > worst ? value : worst;
        sum += value;
    }
    std::printf("recording on the game's thread: %.3f ms a frame on average, %.3f ms at worst\n",
                sum / static_cast<double>(record_ms.size()), worst);

    RewindRecorderStats stats = {};
    rewind_recorder_drain(&rewind->recorder);
    rewind_recorder_stats(&rewind->recorder, &stats);
    std::printf("history: %llu states (%llu skipped, %llu restarts), %.1f MB in use, encode %.3f ms avg / %.3f max\n",
                static_cast<unsigned long long>(stats.states), static_cast<unsigned long long>(stats.skipped),
                static_cast<unsigned long long>(stats.restarts),
                static_cast<double>(rewind_buffer_bytes_used(&rewind->recorder.buffer)) / (1024.0 * 1024.0),
                stats.states ? static_cast<double>(stats.encode_ns_sum) / static_cast<double>(stats.states) / 1e6 : 0.0,
                static_cast<double>(stats.encode_ns_max) / 1e6);

    // The DVR: back, then WATCH to the end.
    if (!core_rewind_dvr_open(rewind))
    {
        std::printf("the DVR didn't open\n");
        return 1;
    }
    CoreRewindPosition position = {};
    core_rewind_dvr_position(rewind, &position);
    std::printf("DVR: %u states, %.1f s kept\n", position.states,
                static_cast<double>(position.newest - position.oldest) / fps);
    uint64_t back_frames = static_cast<uint64_t>(back * fps);
    uint64_t target = position.newest > back_frames ? position.newest - back_frames : 0;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    core_rewind_dvr_seek(rewind, target);
    double seek_ms = ms_since(start);
    core_rewind_dvr_position(rewind, &position);
    std::printf("seek back %u s: at -%.1f s in %.1f ms\n", back,
                static_cast<double>(position.newest - position.cursor) / fps, seek_ms);
    start = std::chrono::steady_clock::now();
    bool step_ok = core_rewind_dvr_step(rewind, -1) && core_rewind_dvr_step(rewind, 1);
    std::printf("a step back and forward: %s, %.2f ms\n", step_ok ? "ok" : "FAILED", ms_since(start) / 2.0);

    core_rewind_dvr_set_playing(rewind, true);
    uint32_t ticks = 0;
    double tick_worst = 0.0;
    start = std::chrono::steady_clock::now();
    for (;;)
    {
        std::chrono::steady_clock::time_point tick = std::chrono::steady_clock::now();
        if (!core_rewind_dvr_tick(rewind))
        {
            break;
        }
        double took = ms_since(tick);
        tick_worst = took > tick_worst ? took : tick_worst;
        ticks++;
    }
    std::printf("WATCH to the present: %u frames, %.2f ms avg / %.2f ms worst a frame, %llu mismatches\n", ticks,
                ms_since(start) / (ticks ? ticks : 1), tick_worst, static_cast<unsigned long long>(rewind->mismatches));

    // PLAY FROM HERE in the middle, then a few seconds more.
    core_rewind_dvr_seek(rewind, position.cursor + static_cast<uint64_t>(5 * fps));
    core_rewind_dvr_resume_here(rewind);
    for (uint32_t frame = 0; frame < static_cast<uint32_t>(3 * fps); frame++)
    {
        LibretroInputState input;
        input.joypad_buttons[0] = measure_script(frame);
        libretro_host_set_input(host, &input);
        core_rewind_before_frame(rewind, &input);
        libretro_host_run_frame(host);
        core_rewind_after_frame(rewind, 0);
    }
    bool reopened = core_rewind_dvr_open(rewind);
    core_rewind_dvr_position(rewind, &position);
    std::printf("PLAY FROM HERE then 3 s: %s, %.1f s kept\n", reopened ? "the DVR opens" : "FAILED",
                static_cast<double>(position.newest - position.oldest) / fps);
    core_rewind_dvr_close(rewind);
    core_rewind_stop(rewind);
    delete rewind;
    libretro_host_unload_core(host);
    delete host;
    return 0;
}
