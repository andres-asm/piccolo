// stream_measure: how much remote play's lossless delta codec (frame_delta.h)
// would send for a real game, and what it costs to code and decode. Runs a
// core with no window for a number of frames, with scripted input (START now
// and then to get past the menus, then right + jump + run), codes every new
// frame against the one before, decodes it again and checks it's exact.
//
//   stream_measure [--frames N] [--skip N] [--scratch DIR] <core> <game or zip>
//
//   --frames N   frames to run after the skipped ones (default 3600: a minute)
//   --skip N     frames to run first without measuring (boot, logos; default 300)
//   --scratch D  where a game from a zip is extracted (default ".")
//
// Prints the picture's size and format, the raw rate, the coded rate (average,
// the worst second, frame sizes), a keyframe's size, and the time to code and
// to decode a frame. Rates are at 60 frames a second.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "frame_delta.h"
#include "host_log.h"
#include "libretro_host.h"
#include "measure_common.h"

static void usage()
{
    std::printf("usage: stream_measure [--frames N] [--skip N] [--scratch DIR] <core> <game or zip>\n");
}

static bool frame_format(retro_pixel_format format, FramePixelFormat* out)
{
    // The three formats retro cores use; the HDR ones aren't coded.
    if (format == RETRO_PIXEL_FORMAT_RGB565)
    {
        *out = FramePixelFormat::rgb565;
        return true;
    }
    if (format == RETRO_PIXEL_FORMAT_0RGB1555)
    {
        *out = FramePixelFormat::xrgb1555;
        return true;
    }
    if (format == RETRO_PIXEL_FORMAT_XRGB8888)
    {
        *out = FramePixelFormat::xrgb8888;
        return true;
    }
    return false;
}

static const char* format_name(FramePixelFormat format)
{
    return format == FramePixelFormat::rgb565 ? "RGB565" : format == FramePixelFormat::xrgb1555 ? "0RGB1555" : "XRGB8888";
}

static double milliseconds_since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int main(int argc, char** argv)
{
    uint32_t frames = 3600;
    uint32_t skip = 300;
    std::string scratch = ".";
    const char* core_path = nullptr;
    const char* game_path = nullptr;
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
        else if (std::strcmp(argv[i], "--scratch") == 0 && has_value)
        {
            scratch = argv[++i];
        }
        else if (core_path == nullptr)
        {
            core_path = argv[i];
        }
        else if (game_path == nullptr)
        {
            game_path = argv[i];
        }
        else
        {
            usage();
            return 2;
        }
    }
    if (core_path == nullptr || game_path == nullptr || frames == 0)
    {
        usage();
        return 2;
    }
    host_log_set_min_level(LogLevel::warning);

    std::string content;
    if (!measure_content(game_path, &scratch, &content))
    {
        return 1;
    }

    LibretroHost* host = new LibretroHost{};
    LibretroHostConfig config;
    config.system_directory = scratch.c_str();
    config.save_directory = scratch.c_str();
    config.native_pixel_formats = true;   // the core's own format: what would be sent
    if (!libretro_host_load_core(host, core_path, &config) || !libretro_host_load_game(host, content.c_str()))
    {
        libretro_host_unload_core(host);
        delete host;
        return 1;
    }

    FrameDeltaEncoder encoder;
    FrameDeltaDecoder decoder;
    FrameDeltaEncoder key_encoder;   // a keyframe now and then, for its size only
    std::vector<uint8_t> coded;
    std::vector<uint8_t> key_coded;
    std::vector<size_t> sizes;   // per measured frame (0 = the core repeated the last one)
    std::vector<size_t> key_sizes;
    double encode_total = 0.0;
    double encode_max = 0.0;
    double decode_total = 0.0;
    double decode_max = 0.0;
    uint32_t coded_frames = 0;
    uint32_t mismatches = 0;
    uint64_t last_serial = 0;
    FramePixelFormat format = FramePixelFormat::rgb565;
    uint32_t width = 0;
    uint32_t height = 0;
    sizes.reserve(frames);
    for (uint32_t frame = 0; frame < skip + frames; frame++)
    {
        LibretroInputState input;
        input.joypad_buttons[0] = measure_script(frame);
        libretro_host_set_input(host, &input);
        if (!libretro_host_run_frame(host))
        {
            break;
        }
        const LibretroVideoFrame* video = &host->video;
        bool fresh = video->serial != last_serial && video->pixels != nullptr && video->width > 0;
        last_serial = video->serial;
        if (frame < skip)
        {
            continue;
        }
        if (!fresh || !frame_format(video->format, &format))
        {
            sizes.push_back(0);   // nothing new to send
            continue;
        }
        width = video->width;
        height = video->height;
        coded.resize(frame_delta_max_size(width, height, format));
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        size_t size = frame_delta_encode(&encoder, video->pixels, width, height, format, false, coded.data());
        double encode_ms = milliseconds_since(start);
        start = std::chrono::steady_clock::now();
        bool decoded = frame_delta_decode(&decoder, coded.data(), size);
        double decode_ms = milliseconds_since(start);
        size_t frame_bytes = static_cast<size_t>(width) * height * frame_pixel_bytes(format);
        if (!decoded || decoder.frame.size() != frame_bytes ||
            std::memcmp(decoder.frame.data(), video->pixels, frame_bytes) != 0)
        {
            mismatches++;
        }
        encode_total += encode_ms;
        decode_total += decode_ms;
        encode_max = encode_ms > encode_max ? encode_ms : encode_max;
        decode_max = decode_ms > decode_max ? decode_ms : decode_max;
        coded_frames++;
        sizes.push_back(size);
        if (coded_frames % 60 == 1)
        {
            key_coded.resize(coded.size());
            key_sizes.push_back(
                frame_delta_encode(&key_encoder, video->pixels, width, height, format, true, key_coded.data()));
        }
    }
    libretro_host_unload_core(host);
    delete host;
    if (coded_frames == 0)
    {
        std::printf("no frames came out of the core\n");
        return 1;
    }

    // The first coded frame is a keyframe: left out of the rates (sent once).
    size_t first = 0;
    while (first < sizes.size() && sizes[first] == 0)
    {
        first++;
    }
    std::vector<size_t> deltas(sizes.begin() + static_cast<long>(first) + 1, sizes.end());
    uint64_t total = 0;
    for (size_t i = 0; i < deltas.size(); i++)
    {
        total += deltas[i];
    }
    uint64_t worst_second = 0;
    for (size_t i = 0; i + 60 <= deltas.size(); i++)
    {
        uint64_t second = 0;
        for (size_t j = i; j < i + 60; j++)
        {
            second += deltas[j];
        }
        worst_second = second > worst_second ? second : worst_second;
    }
    std::vector<size_t> sorted = deltas;
    std::sort(sorted.begin(), sorted.end());
    size_t p50 = sorted.empty() ? 0 : sorted[sorted.size() / 2];
    size_t p95 = sorted.empty() ? 0 : sorted[sorted.size() * 95 / 100];
    size_t largest = sorted.empty() ? 0 : sorted.back();
    uint64_t key_total = 0;
    size_t key_largest = 0;
    for (size_t i = 0; i < key_sizes.size(); i++)
    {
        key_total += key_sizes[i];
        key_largest = key_sizes[i] > key_largest ? key_sizes[i] : key_largest;
    }
    double seconds = static_cast<double>(deltas.size()) / 60.0;
    double raw_mbps = static_cast<double>(width) * height * frame_pixel_bytes(format) * 8.0 * 60.0 / 1000000.0;
    std::printf("%s: %ux%u %s, %u frames measured (%u new, %zu repeats)\n", game_path, width, height,
                format_name(format), frames, coded_frames, sizes.size() - coded_frames);
    std::printf("  raw            %7.1f Mbit/s\n", raw_mbps);
    std::printf("  coded average  %7.2f Mbit/s   worst second %.2f Mbit/s\n",
                seconds > 0.0 ? static_cast<double>(total) * 8.0 / seconds / 1000000.0 : 0.0,
                static_cast<double>(worst_second) * 8.0 / 1000000.0);
    std::printf("  frame bytes    median %zu, 95%% %zu, largest %zu\n", p50, p95, largest);
    std::printf("  keyframe       average %llu bytes, largest %zu\n",
                static_cast<unsigned long long>(key_sizes.empty() ? 0 : key_total / key_sizes.size()), key_largest);
    std::printf("  encode         %.3f ms average, %.3f ms largest\n", encode_total / coded_frames, encode_max);
    std::printf("  decode         %.3f ms average, %.3f ms largest\n", decode_total / coded_frames, decode_max);
    std::printf("  lossless       %s\n", mismatches == 0 ? "every frame exact" : "MISMATCHES");
    return mismatches == 0 ? 0 : 1;
}
