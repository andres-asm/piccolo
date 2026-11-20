#include "text_input_window.h"

#include <cstdio>
#include <cstring>

#include "imgui.h"

#include "piccolo_text.h"
#include "retro_ui.h"

// UTF-8 characters in `text` (continuation bytes 10xxxxxx don't count).
static uint32_t character_count(const char* text, int32_t length)
{
    uint32_t count = 0;
    for (int32_t i = 0; i < length; i++)
    {
        if ((static_cast<unsigned char>(text[i]) & 0xC0u) != 0x80u)
        {
            count++;
        }
    }
    return count;
}

// Byte offset where character number `characters` starts (or the end).
static int32_t offset_of_character(const char* text, int32_t length, uint32_t characters)
{
    uint32_t count = 0;
    for (int32_t i = 0; i < length; i++)
    {
        if ((static_cast<unsigned char>(text[i]) & 0xC0u) != 0x80u)
        {
            if (count == characters)
            {
                return i;
            }
            count++;
        }
    }
    return length;
}

// Dear ImGui limits a field in bytes; the core counts characters. Numbers
// take digits only. UserData: the request.
static int field_callback(ImGuiInputTextCallbackData* data)
{
    const LibretroTextInput* request = static_cast<const LibretroTextInput*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
    {
        bool digit = data->EventChar >= '0' && data->EventChar <= '9';
        return request->type == RETRO_EXT_TEXT_INPUT_TYPE_NUMBER && !digit ? 1 : 0;   // 1 = drop the character
    }
    if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit && request->max_length > 0 &&
        character_count(data->Buf, data->BufTextLen) > request->max_length)
    {
        int32_t cut = offset_of_character(data->Buf, data->BufTextLen, request->max_length);
        data->DeleteChars(cut, data->BufTextLen - cut);
    }
    return 0;
}

TextInputChoice text_input_window_draw(TextInputWindow* window, const LibretroTextInput* request, ImFont* pixel_font,
                                       int32_t scale, const Localization* strings)
{
    if (window->serial != request->serial)
    {
        window->serial = request->serial;
        std::snprintf(window->buffer, sizeof(window->buffer), "%s", request->initial);
        window->focus_field = true;
    }

    TextInputChoice choice = TextInputChoice::none;
    float block = static_cast<float>(scale);
    const ImGuiIO& io = ImGui::GetIO();

    retro_ui_push_theme(pixel_font, scale);
    // 200 retro pixels wide (never wider than the window), as tall as its text.
    float width = 200.0f * block;
    width = width < io.DisplaySize.x - 8.0f * block ? width : io.DisplaySize.x - 8.0f * block;
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
    char title[200] = {};
    std::snprintf(title, sizeof(title), "%s###text_input", piccolo_text(strings, Str::text_input_title));
    if (ImGui::Begin(title, nullptr, flags))
    {
        retro_ui_begin_window(scale);
        if (retro_ui_close_button())
        {
            choice = TextInputChoice::cancel;
        }
        if (request->prompt[0] != '\0')
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(request->prompt);
            ImGui::PopTextWrapPos();
        }
        if (window->focus_field)
        {
            ImGui::SetKeyboardFocusHere();
            window->focus_field = false;
        }
        ImGui::SetNextItemWidth(-1.0f);
        ImGuiInputTextFlags field_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit |
                                          ImGuiInputTextFlags_CallbackCharFilter;
        // The request isn't changed through UserData; Dear ImGui only takes a void*.
        void* user_data = const_cast<LibretroTextInput*>(request);
        retro_ui_push_field_text();
        if (ImGui::InputText("##text", window->buffer, sizeof(window->buffer), field_flags, field_callback, user_data))
        {
            choice = TextInputChoice::ok;   // Enter
        }
        retro_ui_pop_field_text();
        ImGui::Spacing();
        if (retro_ui_button(piccolo_text(strings, Str::text_input_ok)))
        {
            choice = TextInputChoice::ok;
        }
        ImGui::SameLine();
        if (retro_ui_button(piccolo_text(strings, Str::text_input_cancel)))
        {
            choice = TextInputChoice::cancel;
        }
        retro_ui_end_window();
    }
    ImGui::End();
    retro_ui_pop_theme();
    return choice;
}
