// Every OpenGL ES preset piccolo ships (shaders/gl/, presets/ included) loads
// (its shaders compile and link on this GPU's GLES) and runs a frame over a
// small picture, after pass 0. Run on a GLES 2 build too (MULTITAP_GL_ES2):
// that's what the Tegra 4 takes. Needs a display: without one (or without
// GLES) it says SKIP and exits 77.
//
// Usage: piccolo_gl_presets_check <shaders/gl folder>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "app_shell.h"
#include "gl_shader_chain.h"
#include "host_fs.h"
#include "video_texture.h"

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
        std::printf("usage: piccolo_gl_presets_check <shaders/gl folder>\n");
        return 2;
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
    std::vector<uint16_t> pixels(64 * 48, 0x7BEF);
    int32_t failures = 0;
    std::string pass0 = std::string(argv[1]) + "/pass0.glpreset";
    for (size_t i = 0; i < presets.size(); i++)
    {
        GlShaderChain chain = {};
        bool loaded = gl_shader_chain_load(&chain, &shell.gl, 0, pass0.c_str()) &&
                      gl_shader_chain_load(&chain, &shell.gl, 1, presets[i].c_str());
        ViewportResult viewport = {};
        viewport.width = 192;
        viewport.height = 144;
        viewport.source_width = 64;
        viewport.source_height = 48;
        bool ran = false;
        if (loaded && (video_texture_created(&texture) ||
                       video_texture_create(&texture, &shell, 64, 48, FramePixelFormat::rgb565, true)) &&
            app_shell_begin_frame(&shell) && app_shell_acquire(&shell))
        {
            ran = video_texture_upload(&texture, &shell, pixels.data(), 64, 48) &&
                  gl_shader_chain_prepare(&chain, &shell.gl, &texture.gl, texture.has_content, &viewport);
            if (ran)
            {
                gl_shader_chain_record(&chain, &shell.gl, &texture.gl);
            }
            app_shell_present(&shell, ClearColor{ 0.0f, 0.0f, 0.0f, 1.0f });
        }
        GLenum error = shell.gl.gl.GetError();
        if (!loaded || !ran || error != GL_NO_ERROR)
        {
            std::printf("FAIL %s (%s, GL error 0x%04X)\n", presets[i].c_str(),
                        !loaded ? "didn't load" : (!ran ? "didn't run" : "ran"), static_cast<uint32_t>(error));
            failures++;
        }
        else
        {
            std::printf("ok %s\n", presets[i].c_str());
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
    std::printf("gl presets: all %zu load and run\n", presets.size());
    return 0;
}
