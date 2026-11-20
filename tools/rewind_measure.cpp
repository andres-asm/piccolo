// rewind_measure: what keeping a savestate every frame costs on a core and
// game (rewind). Runs the core with no window and the measuring tools'
// scripted pad, serializing every frame and encoding the XOR delta against
// the previous frame (state_delta), and prints per frame: state size,
// serialize time, encode time, delta bytes, applying a delta back, and
// minutes of history per 256 MiB. Also the delta between states
// --snapshot frames apart (SNAPSHOT mode).
//
//   rewind_measure [--frames N] [--skip N] [--snapshot N] [--scratch DIR] <core> <game or zip>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "host_log.h"
#include "libretro_host.h"
#include "measure_common.h"
#include "state_delta.h"


static double ms_since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

static bool sort_ascending(double a, double b)
{
    return a < b;
}

static double percentile(std::vector<double> values, double share)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::sort(values.begin(), values.end(), sort_ascending);
    size_t index = static_cast<size_t>(share * static_cast<double>(values.size() - 1));
    return values[index];
}

static double average(const std::vector<double>* values)
{
    double sum = 0.0;
    for (size_t i = 0; i < values->size(); i++)
    {
        sum += (*values)[i];
    }
    return values->empty() ? 0.0 : sum / static_cast<double>(values->size());
}

int main(int argc, char** argv)
{
    uint32_t frames = 3600;
    uint32_t skip = 600;
    uint32_t snapshot = 600;
    std::string scratch = ".";
    const char* core = nullptr;
    const char* game = nullptr;
    for (int i = 1; i < argc; i++)
    {
        bool has_value = i + 1 < argc;
        if (std::strcmp(argv[i], "--frames") == 0 && has_value)
        {
            frames = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--skip") == 0 && has_value)
        {
            skip = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--snapshot") == 0 && has_value)
        {
            snapshot = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--scratch") == 0 && has_value)
        {
            scratch = argv[++i];
        }
        else if (core == nullptr)
        {
            core = argv[i];
        }
        else if (game == nullptr)
        {
            game = argv[i];
        }
    }
    if (core == nullptr || game == nullptr || frames == 0 || snapshot == 0)
    {
        std::printf("usage: rewind_measure [--frames N] [--skip N] [--snapshot N] [--scratch DIR] <core> <game or zip>\n");
        return 2;
    }
    host_log_set_min_level(LogLevel::warning);
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
    if (!libretro_host_load_core(host, core, &config) ||
        !libretro_host_load_game(host, content.empty() ? nullptr : content.c_str()))
    {
        std::printf("the core or the game didn't load\n");
        return 1;
    }
    size_t size = libretro_host_state_size(host);
    if (size == 0)
    {
        std::printf("no savestates (serialize size 0)\n");
        return 1;
    }

    std::vector<uint8_t> previous;
    std::vector<uint8_t> current;
    std::vector<uint8_t> snapshot_state;
    std::vector<uint8_t> delta(state_delta_max_size(size));
    std::vector<uint8_t> check;
    std::vector<double> run_ms;
    std::vector<double> serialize_ms;
    std::vector<double> encode_ms;
    std::vector<double> apply_ms;
    std::vector<double> unserialize_ms;
    std::vector<double> bytes;
    std::vector<double> snapshot_bytes;
    bool mismatch = false;
    for (uint32_t frame = 0; frame < skip + frames; frame++)
    {
        LibretroInputState input;
        input.joypad_buttons[0] = measure_script(frame);
        libretro_host_set_input(host, &input);
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        libretro_host_run_frame(host);
        double run = ms_since(start);
        if (frame + 1 < skip)
        {
            continue;
        }
        start = std::chrono::steady_clock::now();
        libretro_host_serialize(host, &current);
        double serialize = ms_since(start);
        if (current.size() != size)
        {
            std::printf("state size changed: %zu -> %zu\n", size, current.size());
            return 1;
        }
        if (frame + 1 == skip)
        {
            previous = current;
            snapshot_state = current;
            continue;
        }
        start = std::chrono::steady_clock::now();
        size_t length = state_delta_encode(previous.data(), current.data(), size, delta.data());
        double encode = ms_since(start);

        // Back: the delta turns this frame's state into the previous one.
        check = current;
        start = std::chrono::steady_clock::now();
        bool applied = state_delta_apply(check.data(), size, delta.data(), length);
        double apply = ms_since(start);
        if (!applied || check != previous)
        {
            mismatch = true;
        }
        run_ms.push_back(run);
        serialize_ms.push_back(serialize);
        encode_ms.push_back(encode);
        apply_ms.push_back(apply);
        bytes.push_back(static_cast<double>(length));
        if ((frame + 1 - skip) % snapshot == 0)
        {
            size_t snap = state_delta_encode(snapshot_state.data(), current.data(), size, delta.data());
            snapshot_bytes.push_back(static_cast<double>(snap));
            snapshot_state = current;
        }
        // Rewinding one step: load the state.
        if (frame % 60 == 0)
        {
            start = std::chrono::steady_clock::now();
            libretro_host_unserialize(host, current.data(), size);
            unserialize_ms.push_back(ms_since(start));
        }
        previous.swap(current);
    }

    double per_frame = average(&bytes);
    double minutes = 256.0 * 1024.0 * 1024.0 / (per_frame * 60.0 * 60.0);
    std::printf("state %zu bytes, %zu frames\n", size, bytes.size());
    std::printf("core frame      avg %.3f ms  p95 %.3f ms\n", average(&run_ms), percentile(run_ms, 0.95));
    std::printf("serialize       avg %.3f ms  p95 %.3f ms\n", average(&serialize_ms), percentile(serialize_ms, 0.95));
    std::printf("encode          avg %.3f ms  p95 %.3f ms  max %.3f\n", average(&encode_ms), percentile(encode_ms, 0.95),
                percentile(encode_ms, 1.0));
    std::printf("apply (back)    avg %.3f ms  p95 %.3f ms\n", average(&apply_ms), percentile(apply_ms, 0.95));
    std::printf("unserialize     avg %.3f ms\n", average(&unserialize_ms));
    std::printf("delta           avg %.0f B  p95 %.0f B  max %.0f B (%.2f%% of the state)\n", per_frame,
                percentile(bytes, 0.95), percentile(bytes, 1.0), 100.0 * per_frame / static_cast<double>(size));
    std::printf("history         %.1f min per 256 MiB at 60 fps\n", minutes);
    std::printf("snapshot delta  every %u frames: avg %.0f B (%.1f%% of the state)\n", snapshot, average(&snapshot_bytes),
                100.0 * average(&snapshot_bytes) / static_cast<double>(size));
    std::printf("round trip      %s\n", mismatch ? "MISMATCH" : "ok");
    libretro_host_unload_core(host);
    delete host;
    return mismatch ? 1 : 0;
}
