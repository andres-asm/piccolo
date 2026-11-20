// netplay_measure: which netplay mode a core (and game) is good for, measured
// (Andrés: instrumented tests of the cores). Runs the core with no window and
// the measuring tools' scripted pad, three times over the same frames:
//
//   A  straight: a hash of the system RAM and of the picture, every frame.
//   B  the same after loading the core again: if it differs from A the
//      emulation isn't deterministic, and only stream can work.
//   C  rollback every frame: before each frame its state is kept; then the
//      state from D frames back is loaded, those D frames run again hidden
//      (no picture, no sound), and the frame is shown. It must match A (a
//      rollback that changes the game is no good), and it times saving,
//      loading and a hidden frame. If it differs, C runs again with the
//      replayed frames drawn: some cores skip work when told no video is
//      wanted, and some games depend on it (snes9x + Donkey Kong Country), so
//      their rollback has to replay with video on (and costs more).
//
// With the quirks the core declares (SET_SERIALIZATION_QUIRKS), the verdict:
//   rollback (up to N frames)  deterministic, states exact, and N frames of
//                              rollback fit in a frame on this machine
//   delay                      deterministic, but states unusable (quirks,
//                              or C differs) or too slow for 2 frames
//   stream only                not deterministic
//
//   netplay_measure [--frames N] [--skip N] [--depth D] [--scratch DIR] <core> <game or zip>
//   (a game of "" starts a core that runs without one)
//
//   --frames N   frames measured after the skipped ones (default 1200: 20 s)
//   --skip N     frames run first in every pass (boot, logos; default 300)
//   --depth D    pass C's rollback depth (default 4)
//   --scratch D  where a game from a zip is extracted (default ".")
//   --show       pass C runs its frames again with video and audio on (to tell
//                a core that changes when hidden from one whose states aren't exact)
//   --normal     pass C tells the core its states are NORMAL ones (no
//                ROLLBACK_NETPLAY context, no "fast savestates" bit)

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

constexpr double FRAME_MS = 1000.0 / 60.0;
constexpr uint32_t MAX_ROLLBACK = 8;

struct Pass
{
    std::vector<uint64_t> ram;     // per measured frame
    std::vector<uint64_t> video;
    bool has_ram = false;
};

struct Costs
{
    std::vector<double> save_ms;
    std::vector<double> load_ms;
    std::vector<double> hidden_ms;   // one hidden frame
    std::vector<double> shown_ms;    // one shown frame
    size_t state_bytes = 0;
};

static uint64_t fnv(const uint8_t* data, size_t size)
{
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < size; i++)
    {
        hash = (hash ^ data[i]) * 1099511628211ull;
    }
    return hash;
}

static double ms_since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

static void set_input(LibretroHost* host, uint32_t frame)
{
    LibretroInputState input;
    input.joypad_buttons[0] = measure_script(frame);
    libretro_host_set_input(host, &input);
}

static void record(LibretroHost* host, Pass* pass)
{
    const uint8_t* ram = nullptr;
    size_t ram_size = 0;
    pass->has_ram = libretro_host_memory(host, RETRO_MEMORY_SYSTEM_RAM, &ram, &ram_size);
    pass->ram.push_back(pass->has_ram ? fnv(ram, ram_size) : 0);
    const LibretroVideoFrame* video = &host->video;
    size_t bytes = static_cast<size_t>(video->width) * video->height * video->bytes_per_pixel;
    pass->video.push_back(video->pixels != nullptr ? fnv(video->pixels, bytes) : 0);
}

static LibretroHost* load(const char* core, const std::string* content, const std::string* scratch)
{
    LibretroHost* host = new LibretroHost{};
    LibretroHostConfig config;
    config.system_directory = scratch->c_str();
    config.save_directory = scratch->c_str();
    config.native_pixel_formats = true;
    if (!libretro_host_load_core(host, core, &config) || !libretro_host_load_game(host, content->empty() ? nullptr : content->c_str()))
    {
        libretro_host_unload_core(host);
        delete host;
        return nullptr;
    }
    return host;
}

static void unload(LibretroHost* host)
{
    libretro_host_unload_core(host);
    delete host;
}

// A or B: straight through.
static bool run_straight(const char* core, const std::string* content, const std::string* scratch, uint32_t skip,
                         uint32_t frames, Pass* pass)
{
    LibretroHost* host = load(core, content, scratch);
    if (host == nullptr)
    {
        return false;
    }
    for (uint32_t frame = 0; frame < skip + frames; frame++)
    {
        set_input(host, frame);
        libretro_host_run_frame(host);
        if (frame >= skip)
        {
            record(host, pass);
        }
    }
    unload(host);
    return true;
}

// C: a rollback of `depth` frames before every frame shown.
static bool g_show = false;     // --show
static bool g_normal = false;   // --normal

static bool run_rollback(const char* core, const std::string* content, const std::string* scratch, uint32_t skip,
                         uint32_t frames, uint32_t depth, Pass* pass, Costs* costs, uint64_t* quirks)
{
    LibretroHost* host = load(core, content, scratch);
    if (host == nullptr)
    {
        return false;
    }
    *quirks = host->serialization_quirks;
    if (!g_normal)
    {
        libretro_host_set_savestate_context(host, RETRO_SAVESTATE_CONTEXT_ROLLBACK_NETPLAY);
    }
    std::vector<std::vector<uint8_t>> ring(depth + 1);   // the state before frame f is ring[f % (depth + 1)]
    bool states = true;
    for (uint32_t frame = 0; frame < skip + frames && states; frame++)
    {
        bool measuring = frame >= skip;
        if (measuring && depth > 0 && frame >= skip + depth)
        {
            // Back to before frame - depth, then those frames again, hidden.
            uint32_t from = frame - depth;
            const std::vector<uint8_t>* state = &ring[from % (depth + 1)];
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            states = libretro_host_unserialize(host, state->data(), costs->state_bytes);
            costs->load_ms.push_back(ms_since(start));
            libretro_host_set_hidden(host, !g_show);
            for (uint32_t again = from; again < frame && states; again++)
            {
                // Each frame's state is saved again as it's run again, as a
                // rollback does (the inputs could have changed).
                libretro_host_serialize(host, &ring[again % (depth + 1)]);
                set_input(host, again);
                start = std::chrono::steady_clock::now();
                libretro_host_run_frame(host);
                costs->hidden_ms.push_back(ms_since(start));
            }
            libretro_host_set_hidden(host, false);
        }
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        if (depth > 0)
        {
            states = libretro_host_serialize(host, &ring[frame % (depth + 1)]);
            costs->state_bytes = libretro_host_state_size(host);
        }
        if (measuring)
        {
            costs->save_ms.push_back(ms_since(start));
        }
        set_input(host, frame);
        start = std::chrono::steady_clock::now();
        libretro_host_run_frame(host);
        if (measuring)
        {
            costs->shown_ms.push_back(ms_since(start));
            record(host, pass);
        }
    }
    unload(host);
    return states;
}

static uint32_t first_difference(const Pass* a, const Pass* b)
{
    size_t count = a->ram.size() < b->ram.size() ? a->ram.size() : b->ram.size();
    for (size_t i = 0; i < count; i++)
    {
        if (a->ram[i] != b->ram[i] || a->video[i] != b->video[i])
        {
            return static_cast<uint32_t>(i);
        }
    }
    return a->ram.size() == b->ram.size() ? UINT32_MAX : static_cast<uint32_t>(count);
}

static double average(const std::vector<double>* values)
{
    double total = 0.0;
    for (size_t i = 0; i < values->size(); i++)
    {
        total += (*values)[i];
    }
    return values->empty() ? 0.0 : total / static_cast<double>(values->size());
}

static double percentile(std::vector<double> values, double share)
{
    if (values.empty())
    {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    return values[static_cast<size_t>(share * static_cast<double>(values.size() - 1))];
}

int main(int argc, char** argv)
{
    uint32_t frames = 1200;
    uint32_t skip = 300;
    uint32_t depth = 4;
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
        else if (std::strcmp(argv[i], "--depth") == 0 && has_value)
        {
            depth = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--show") == 0)
        {
            g_show = true;
        }
        else if (std::strcmp(argv[i], "--normal") == 0)
        {
            g_normal = true;
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
        else
        {
            core = nullptr;
            break;
        }
    }
    if (core == nullptr || game == nullptr || frames == 0 || depth > MAX_ROLLBACK)
    {
        std::printf("usage: netplay_measure [--frames N] [--skip N] [--depth D (0..8)] [--scratch DIR] <core> <game or zip>\n");
        return 2;
    }
    host_log_set_min_level(LogLevel::warning);
    std::string content;
    if (!measure_content(game, &scratch, &content))
    {
        return 1;
    }

    Pass straight;
    Pass again;
    Pass rolled;
    Costs costs;
    uint64_t quirks = 0;
    if (!run_straight(core, &content, &scratch, skip, frames, &straight) ||
        !run_straight(core, &content, &scratch, skip, frames, &again))
    {
        std::printf("the core or the game didn't load\n");
        return 1;
    }
    bool states_work = run_rollback(core, &content, &scratch, skip, frames, depth, &rolled, &costs, &quirks);
    uint32_t not_deterministic = first_difference(&straight, &again);
    uint32_t rollback_differs = first_difference(&straight, &rolled);
    uint32_t hidden_differs = UINT32_MAX;   // set when hidden replays failed and drawn ones were tried
    if (states_work && rollback_differs != UINT32_MAX && !g_show && not_deterministic == UINT32_MAX)
    {
        hidden_differs = rollback_differs;
        g_show = true;
        Pass drawn;
        Costs drawn_costs;
        states_work = run_rollback(core, &content, &scratch, skip, frames, depth, &drawn, &drawn_costs, &quirks);
        rollback_differs = first_difference(&straight, &drawn);
        costs = drawn_costs;
    }
    uint64_t blocking = RETRO_SERIALIZATION_QUIRK_INCOMPLETE | RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION;
    double save = percentile(costs.save_ms, 0.95);
    double load = percentile(costs.load_ms, 0.95);
    double hidden = percentile(costs.hidden_ms, 0.95);
    double shown = percentile(costs.shown_ms, 0.95);
    // Frames of rollback that fit in a frame here: load, then N hidden frames
    // (each saved again), then the shown one with its save.
    uint32_t fits = 0;
    while (fits < MAX_ROLLBACK && load + (fits + 1) * (hidden + save) + shown + save <= FRAME_MS)
    {
        fits++;
    }

    std::printf("%s\n  %s\n", game, core);
    std::printf("  quirks           0x%llx%s%s%s%s\n", static_cast<unsigned long long>(quirks),
                (quirks & RETRO_SERIALIZATION_QUIRK_INCOMPLETE) != 0 ? " INCOMPLETE" : "",
                (quirks & RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION) != 0 ? " SINGLE_SESSION" : "",
                (quirks & RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT) != 0 ? " PLATFORM_DEPENDENT" : "",
                (quirks & RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT) != 0 ? " ENDIAN_DEPENDENT" : "");
    std::printf("  checked by       %s\n", straight.has_ram ? "system RAM + picture" : "picture only (no system RAM exposed)");
    if (not_deterministic == UINT32_MAX)
    {
        std::printf("  deterministic    yes (%u frames, twice, identical)\n", frames);
    }
    else
    {
        std::printf("  deterministic    NO: the second run differs from frame %u\n", not_deterministic);
    }
    if (!states_work)
    {
        std::printf("  rollback (%u)     states failed (save or load refused)\n", depth);
    }
    else if (rollback_differs == UINT32_MAX)
    {
        std::printf("  rollback (%u)     exact (every frame rolled back %u, same game)\n", depth, depth);
    }
    else
    {
        std::printf("  rollback (%u)     DIFFERS from frame %u\n", depth, rollback_differs);
    }
    if (hidden_differs != UINT32_MAX)
    {
        std::printf("  hidden replays   DIFFER from frame %u: replayed with video on%s\n", hidden_differs,
                    rollback_differs == UINT32_MAX ? ", and then exact" : "");
    }
    std::printf("  state            %zu bytes, save %.3f ms, load %.3f ms (95%%)\n", costs.state_bytes, save, load);
    std::printf("  frames           shown %.3f ms, hidden %.3f ms (95%%; average %.3f / %.3f)\n", shown, hidden,
                average(&costs.shown_ms), average(&costs.hidden_ms));
    std::printf("  rollback fits    %u frames in %.1f ms on this machine\n", fits, FRAME_MS);
    // Times mean little on a busy machine: its load, next to them.
    FILE* load_file = std::fopen("/proc/loadavg", "r");
    if (load_file != nullptr)
    {
        double load_average = 0.0;
        if (std::fscanf(load_file, "%lf", &load_average) == 1)
        {
            std::printf("  machine load     %.2f (times are only good on an idle machine)\n", load_average);
        }
        std::fclose(load_file);
    }

    const char* verdict = "stream only";
    const char* why = "the emulation isn't deterministic";
    char rollback[64];
    if (not_deterministic == UINT32_MAX)
    {
        verdict = "delay";
        if ((quirks & blocking) != 0)
        {
            why = "the core says its states can't be loaded freely (quirks)";
        }
        else if (!states_work || rollback_differs != UINT32_MAX)
        {
            why = "loading a state changes the game";
        }
        else if (fits < 2)
        {
            why = "rollback is too slow on this machine (fewer than 2 frames fit)";
        }
        else
        {
            std::snprintf(rollback, sizeof(rollback), "rollback (up to %u frames here%s)", fits,
                          hidden_differs != UINT32_MAX ? ", replays with video on" : "");
            verdict = rollback;
            why = "deterministic, exact states, fast enough";
        }
    }
    std::printf("  verdict          %s: %s\n", verdict, why);
    return 0;
}
