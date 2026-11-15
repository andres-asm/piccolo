#include "glyph_viewer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>

#include "imgui.h"

#include "app_shell.h"
#include "debug_ui.h"

constexpr uint32_t VIEWER_TEXTURE_SIZE = 1024;

static const char* const SETS[] = {"12", "16", "24", "32"};
constexpr int32_t SET_COUNT = static_cast<int32_t>(sizeof(SETS) / sizeof(SETS[0]));

static const char* const FOLDERS[] = {
    "nes", "snes", "n64", "gc", "gb", "gba", "wii", "wiiu", "switch",
    "ps1", "ps2", "ps3", "ps4", "ps5", "xbox", "xbox360", "xboxone", "xboxseries",
    "keyboard/common", "keyboard/en", "keyboard/es",
};
constexpr int32_t FOLDER_COUNT = static_cast<int32_t>(sizeof(FOLDERS) / sizeof(FOLDERS[0]));

// For qsort: file names in alphabetical order.
static int compare_names(const void* a, const void* b)
{
    return std::strcmp(*static_cast<char* const*>(a), *static_cast<char* const*>(b));
}

// Loads every PNG of the chosen folder into the pixels, packed in rows.
static void load_folder(GlyphViewer* viewer)
{
    viewer->reload = false;
    viewer->pictures.clear();
    viewer->pixels.assign(static_cast<size_t>(VIEWER_TEXTURE_SIZE) * VIEWER_TEXTURE_SIZE, 0u);
    const char* base = SDL_GetBasePath();
    std::string folder = std::string(base != nullptr ? base : "") + "assets/glyphs/" + SETS[viewer->set] + "/" +
                         FOLDERS[viewer->folder];
    int count = 0;
    char** names = SDL_GlobDirectory(folder.c_str(), "*.png", 0, &count);
    if (names != nullptr && count > 1)
    {
        std::qsort(names, static_cast<size_t>(count), sizeof(char*), compare_names);
    }
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t row_height = 0;
    for (int i = 0; names != nullptr && i < count; i++)
    {
        std::string path = folder + "/" + names[i];
        SDL_Surface* png = SDL_LoadPNG(path.c_str());
        SDL_Surface* converted = png != nullptr ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_ARGB8888) : nullptr;
        if (converted != nullptr && converted->w > 0 && converted->h > 0)
        {
            uint32_t w = static_cast<uint32_t>(converted->w);
            uint32_t h = static_cast<uint32_t>(converted->h);
            if (x + w > VIEWER_TEXTURE_SIZE)
            {
                x = 0;
                y += row_height + 1;
                row_height = 0;
            }
            if (w <= VIEWER_TEXTURE_SIZE && y + h <= VIEWER_TEXTURE_SIZE)
            {
                for (uint32_t row = 0; row < h; row++)
                {
                    const uint8_t* source = static_cast<const uint8_t*>(converted->pixels) + static_cast<size_t>(row) * static_cast<size_t>(converted->pitch);
                    std::memcpy(&viewer->pixels[static_cast<size_t>(y + row) * VIEWER_TEXTURE_SIZE + x], source,
                                static_cast<size_t>(w) * 4);
                }
                GlyphViewerPicture picture;
                picture.name = names[i];
                picture.name.resize(picture.name.size() - 4);   // without ".png"
                picture.x = x;
                picture.y = y;
                picture.width = w;
                picture.height = h;
                viewer->pictures.push_back(picture);
                x += w + 1;
                row_height = h > row_height ? h : row_height;
            }
        }
        SDL_DestroySurface(converted);
        SDL_DestroySurface(png);
    }
    SDL_free(names);
    viewer->upload_pending = true;
}

void glyph_viewer_draw(GlyphViewer* viewer)
{
    if (!viewer->open)
    {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Glyphs", &viewer->open))
    {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::Combo("Size", &viewer->set, SETS, SET_COUNT))
    {
        viewer->reload = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::Combo("Folder", &viewer->folder, FOLDERS, FOLDER_COUNT))
    {
        viewer->reload = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    ImGui::SliderInt("Zoom", &viewer->zoom, 1, 8);
    if (viewer->reload)
    {
        load_folder(viewer);
    }
    ImGui::Text("%d pictures", static_cast<int>(viewer->pictures.size()));
    ImGui::Separator();

    ImGui::BeginChild("##glyphs", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);
    float zoom = static_cast<float>(viewer->zoom);
    float right_edge = ImGui::GetWindowPos().x + ImGui::GetContentRegionAvail().x;
    for (size_t i = 0; i < viewer->pictures.size(); i++)
    {
        const GlyphViewerPicture* picture = &viewer->pictures[i];
        float image_w = static_cast<float>(picture->width) * zoom;
        float image_h = static_cast<float>(picture->height) * zoom;
        float label_w = ImGui::CalcTextSize(picture->name.c_str()).x;
        float item_w = image_w > label_w ? image_w : label_w;
        if (i > 0)
        {
            ImGui::SameLine();
            if (ImGui::GetCursorScreenPos().x + item_w > right_edge)
            {
                ImGui::NewLine();
            }
        }
        ImGui::BeginGroup();
        ImVec2 at = ImGui::GetCursorScreenPos();
        // A blue like the UI's panels behind it, so the cut-out labels show.
        draw_list->AddRectFilled(at, ImVec2(at.x + image_w, at.y + image_h), IM_COL32(0x2A, 0x40, 0x68, 0xFF));
        if (viewer->has_picture && viewer->texture_id != 0)
        {
            float size = static_cast<float>(VIEWER_TEXTURE_SIZE);
            ImVec2 uv_min(static_cast<float>(picture->x) / size, static_cast<float>(picture->y) / size);
            ImVec2 uv_max(static_cast<float>(picture->x + picture->width) / size,
                          static_cast<float>(picture->y + picture->height) / size);
            draw_list->AddImage(ImTextureRef(static_cast<ImTextureID>(viewer->texture_id)), at,
                                ImVec2(at.x + image_w, at.y + image_h), uv_min, uv_max, IM_COL32(0xE0, 0xE6, 0xF0, 0xFF));
        }
        ImGui::Dummy(ImVec2(item_w, image_h));
        ImGui::TextUnformatted(picture->name.c_str());
        ImGui::EndGroup();
    }
    draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);
    ImGui::EndChild();
    ImGui::End();
}

void glyph_viewer_upload(GlyphViewer* viewer, AppShell* shell)
{
    if (!viewer->upload_pending)
    {
        return;
    }
    if (viewer->texture.image == VK_NULL_HANDLE)
    {
        if (!streaming_texture_create(&viewer->texture, &shell->vulkan, VIEWER_TEXTURE_SIZE, VIEWER_TEXTURE_SIZE))
        {
            viewer->upload_pending = false;
            return;
        }
        viewer->texture_id = debug_ui_add_texture(viewer->texture.view);
    }
    if (!app_shell_acquire(shell))
    {
        return;
    }
    if (streaming_texture_upload(&viewer->texture, renderer_command_buffer(&shell->renderer),
                                 renderer_frame_index(&shell->renderer), viewer->pixels.data(), VIEWER_TEXTURE_SIZE,
                                 VIEWER_TEXTURE_SIZE))
    {
        viewer->has_picture = true;
    }
    viewer->upload_pending = false;
}

void glyph_viewer_destroy(GlyphViewer* viewer, AppShell* shell)
{
    if (viewer->texture.image != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(shell->vulkan.device);
        if (viewer->texture_id != 0)
        {
            debug_ui_remove_texture(viewer->texture_id);
        }
        streaming_texture_destroy(&viewer->texture, &shell->vulkan);
    }
    viewer->texture_id = 0;
    viewer->has_picture = false;
}
