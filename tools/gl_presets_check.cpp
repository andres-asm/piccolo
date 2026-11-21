// Every OpenGL ES preset piccolo ships (shaders/gl/, presets/ included) loads
// (its shaders compile and link on this GPU's GLES) and runs a frame over a
// small picture, after pass 0. Run on a GLES 2 build too (MULTITAP_GL_ES2):
// that's what the Tegra 4 takes. Needs a display: without one (or without
// GLES) it says SKIP and exits 77.
//
// Each preset also has to come out even at a whole factor (256x224 at x3,
// square pixels): the output repeats every 6 pixels both ways (two source
// lines, and crt-mask's two-column mask). A shader that computes where it is
// within a source pixel at too low a precision (mediump is 16-bit on Mali)
// draws uneven lines and fails this.
//
// Usage: piccolo_gl_presets_check <shaders/gl folder> [--frames N]
//   --frames N: also times N frames of each (pass 0 + the preset, 256x224 to
//   1280x720 like the SHIELD Portable's screen, vsync off, glFinish each
//   frame) and prints the milliseconds per frame.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "app_shell.h"
#include "gl_shader_chain.h"
#include "host_fs.h"
#include "video_texture.h"

// True if the chain's last output repeats every `period` pixels both ways
// (within 2/255). Prints the first place it doesn't.
static bool output_is_even(const GlShaderChain* chain, GlContext* context, uint32_t period, const char* path)
{
    const GlShaderStage* stage = &chain->stages[1];
    const GlShaderPass* last = &stage->passes[stage->pass_count - 1];
    uint32_t width = last->width;
    uint32_t height = last->height;
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    context->gl.BindFramebuffer(GL_FRAMEBUFFER, last->framebuffer);
    context->gl.ReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE,
                           pixels.data());
    context->gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    for (uint32_t y = 0; y + period < height; y++)
    {
        for (uint32_t x = 0; x + period < width; x++)
        {
            const uint8_t* here = &pixels[(static_cast<size_t>(y) * width + x) * 4];
            const uint8_t* right = &pixels[(static_cast<size_t>(y) * width + x + period) * 4];
            const uint8_t* below = &pixels[(static_cast<size_t>(y + period) * width + x) * 4];
            for (uint32_t c = 0; c < 3; c++)
            {
                if (std::abs(here[c] - right[c]) > 2 || std::abs(here[c] - below[c]) > 2)
                {
                    std::printf("uneven %s at %u,%u: %u,%u,%u vs right %u,%u,%u, below %u,%u,%u\n", path, x, y,
                                here[0], here[1], here[2], right[0], right[1], right[2], below[0], below[1], below[2]);
                    return false;
                }
            }
        }
    }
    return true;
}

static void find_presets(const std::string& folder, std::vector<std::string>* presets)
{
    std::vector<HostFsEntry> entries;
    if (!host_fs_list_directory(folder.c_str(), &entries))
    {
        return;
    }
    for (size_t i = 0; i < entries.size(); i++)
    {
        std::string path = folder + "/" + entries[i].name;
        if (entries[i].is_directory)
        {
            find_presets(path, presets);
        }
        else if (path.size() > 9 && path.compare(path.size() - 9, 9, ".glpreset") == 0)
        {
            presets->push_back(path);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::printf("usage: piccolo_gl_presets_check <shaders/gl folder> [--frames N]\n");
        return 2;
    }
    uint32_t timed_frames = 0;
    if (argc >= 4 && std::strcmp(argv[2], "--frames") == 0)
    {
        timed_frames = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
    }
    std::vector<std::string> presets;
    find_presets(argv[1], &presets);
    if (presets.empty())
    {
        std::printf("FAIL: no presets in %s\n", argv[1]);
        return 1;
    }

    SDL_setenv_unsafe("MULTITAP_GPU", "gl", 1);
    AppShell shell = {};
    AppShellConfig config = {};
    config.name = "gl_presets_check";
    config.window_width = 64;
    config.window_height = 64;
    if (!app_shell_create(&shell, &config))
    {
        std::printf("SKIP: no display or no OpenGL ES\n");
        app_shell_destroy(&shell);
        return 77;
    }

    VideoTexture texture = {};
    std::vector<uint16_t> pixels(256 * 224, 0x7BEF);
    if (timed_frames > 0)
    {
        app_shell_set_vsync(&shell, false);
    }
    int32_t failures = 0;
    std::string pass0 = std::string(argv[1]) + "/pass0.glpreset";
    for (size_t i = 0; i < presets.size(); i++)
    {
        GlShaderChain chain = {};
        bool loaded = gl_shader_chain_load(&chain, &shell.gl, 0, pass0.c_str()) &&
                      gl_shader_chain_load(&chain, &shell.gl, 1, presets[i].c_str());
        ViewportResult viewport = {};
        viewport.width = 768;   // x3, square pixels: the even-output check
        viewport.height = 672;
        viewport.source_width = 256;
        viewport.source_height = 224;
        if (timed_frames > 0)
        {
            viewport.width = 960;   // 256x224 at 4:3, x3.2: what a 720p screen shows
            viewport.height = 720;
            viewport.source_width = 256;
            viewport.source_height = 224;
        }
        bool ran = false;
        bool even = true;
        if (loaded && (video_texture_created(&texture) ||
                       video_texture_create(&texture, &shell, 256, 224, FramePixelFormat::rgb565, true)) &&
            app_shell_begin_frame(&shell) && app_shell_acquire(&shell))
        {
            ran = video_texture_upload(&texture, &shell, pixels.data(), viewport.source_width, viewport.source_height) &&
                  gl_shader_chain_prepare(&chain, &shell.gl, &texture.gl, texture.has_content, &viewport);
            if (ran)
            {
                gl_shader_chain_record(&chain, &shell.gl, &texture.gl);
                if (timed_frames == 0)
                {
                    even = output_is_even(&chain, &shell.gl, 6, presets[i].c_str());
                }
            }
            app_shell_present(&shell, ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f });
        }
        double ms_per_frame = 0.0;
        if (ran && timed_frames > 0)
        {
            // The same work as a game frame on screen: upload, the chain, ImGui
            // (nothing), present; glFinish so each frame's GPU time is counted.
            uint64_t started = SDL_GetTicksNS();
            for (uint32_t frame = 0; frame < timed_frames; frame++)
            {
                if (!app_shell_begin_frame(&shell) || !app_shell_acquire(&shell))
                {
                    continue;
                }
                video_texture_upload(&texture, &shell, pixels.data(), viewport.source_width, viewport.source_height);
                gl_shader_chain_prepare(&chain, &shell.gl, &texture.gl, texture.has_content, &viewport);
                gl_shader_chain_record(&chain, &shell.gl, &texture.gl);
                app_shell_present(&shell, ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f });
                shell.gl.gl.Finish();
            }
            ms_per_frame = static_cast<double>(SDL_GetTicksNS() - started) / 1e6 / static_cast<double>(timed_frames);
        }
        GLenum error = shell.gl.gl.GetError();
        if (!loaded || !ran || !even || error != GL_NO_ERROR)
        {
            std::printf("FAIL %s (%s, GL error 0x%04X)\n", presets[i].c_str(),
                        !loaded ? "didn't load" : (!ran ? "didn't run" : (!even ? "uneven" : "ran")),
                        static_cast<uint32_t>(error));
            failures++;
        }
        else
        {
            if (timed_frames > 0)
            {
                std::printf("ok %s: %.2f ms per frame\n", presets[i].c_str(), ms_per_frame);
            }
            else
            {
                std::printf("ok %s\n", presets[i].c_str());
            }
        }
        gl_shader_chain_destroy(&chain, &shell.gl);
    }
    video_texture_destroy(&texture, &shell);
    app_shell_destroy(&shell);
    if (failures != 0)
    {
        std::printf("FAIL: %d presets\n", failures);
        return 1;
    }
    std::printf("gl presets: all %zu load, run and come out even\n", presets.size());
    return 0;
}
