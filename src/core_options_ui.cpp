#include "core_options_ui.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "imgui.h"

#include "core_session.h"
#include "piccolo_text.h"
#include "retro_ui.h"

// Marker colors (0xAARRGGBB): core and game as in settings_ui, folder its own.
constexpr uint32_t MARKER_CORE   = 0xFFF0C040u;   // amber
constexpr uint32_t MARKER_FOLDER = 0xFF60B0F0u;   // light blue
constexpr uint32_t MARKER_GAME   = 0xFF60E080u;   // green

static ImVec4 color_of(uint32_t argb)
{
    return ImVec4(static_cast<float>((argb >> 16) & 0xFFu) / 255.0f, static_cast<float>((argb >> 8) & 0xFFu) / 255.0f,
                  static_cast<float>(argb & 0xFFu) / 255.0f, static_cast<float>(argb >> 24) / 255.0f);
}

static bool running(const CoreOptionsUiContext* context)
{
    return context->running_core[0] != '\0' && context->session != nullptr;
}

static void add_unique(std::vector<std::string>* list, const std::string& name)
{
    for (size_t i = 0; i < list->size(); i++)
    {
        if ((*list)[i] == name)
        {
            return;
        }
    }
    list->push_back(name);
}

// The running core first, then every core the cache or the store knows.
static void core_list(const CoreOptionsUiContext* context, std::vector<std::string>* cores)
{
    cores->clear();
    if (context->running_core[0] != '\0')
    {
        cores->push_back(context->running_core);
    }
    std::vector<std::string> cached;
    core_options_cache_cores(context->cache, &cached);
    for (size_t i = 0; i < cached.size(); i++)
    {
        add_unique(cores, cached[i]);
    }
    const std::vector<SettingsFileSection>* sections = &context->store->file.sections;
    for (size_t i = 0; i < sections->size(); i++)
    {
        const std::string& name = (*sections)[i].name;
        if (name.empty() || (*sections)[i].entries.empty())
        {
            continue;
        }
        size_t slash = name.find('/');
        add_unique(cores, slash == std::string::npos ? name : name.substr(0, slash));
    }
}

static void draw_core_combo(CoreOptionsUiState* state, const CoreOptionsUiContext* context,
                            const std::vector<std::string>* cores)
{
    const Localization* strings = context->strings;
    char preview[300] = {};
    bool selected_running = state->selected_core == context->running_core;
    std::snprintf(preview, sizeof(preview), "%s%s%s%s", state->selected_core.c_str(), selected_running ? " (" : "",
                  selected_running ? piccolo_text(strings, Str::core_options_running) : "", selected_running ? ")" : "");
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(piccolo_text(strings, Str::core_options_core));
    ImGui::SameLine(start + context->label_width);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (retro_ui_begin_combo("##core", preview))
    {
        for (size_t i = 0; i < cores->size(); i++)
        {
            char label[300] = {};
            bool is_running = (*cores)[i] == context->running_core;
            std::snprintf(label, sizeof(label), "%s%s%s%s###core%zu", (*cores)[i].c_str(), is_running ? " (" : "",
                          is_running ? piccolo_text(strings, Str::core_options_running) : "", is_running ? ")" : "", i);
            if (ImGui::Selectable(label, (*cores)[i] == state->selected_core))
            {
                state->selected_core = (*cores)[i];
            }
        }
        ImGui::EndCombo();
    }
}

static const char* layer_name(const CoreOptionsUiContext* context, CoreOptionLayer layer, char* buffer, size_t size)
{
    const Localization* strings = context->strings;
    switch (layer)
    {
    case CoreOptionLayer::core:
        std::snprintf(buffer, size, "%s (%s)", piccolo_text(strings, Str::core_options_layer_core), context->running_core);
        break;
    case CoreOptionLayer::directory:
        std::snprintf(buffer, size, "%s (%s)", piccolo_text(strings, Str::core_options_layer_folder), context->folder);
        break;
    case CoreOptionLayer::game:
        std::snprintf(buffer, size, "%s (%s)", piccolo_text(strings, Str::core_options_layer_game), context->game);
        break;
    }
    return buffer;
}

static void draw_layer_combo(CoreOptionsUiState* state, const CoreOptionsUiContext* context)
{
    CoreOptionsStore* store = context->store;
    bool has_folder = !core_options_store_section(store, CoreOptionLayer::directory)->empty();
    bool has_game = !core_options_store_section(store, CoreOptionLayer::game)->empty();
    if (state->choose_layer)
    {
        // The most specific layer already in use, so changes keep going there.
        state->choose_layer = false;
        state->layer = CoreOptionLayer::core;
        if (has_game && core_options_store_layer_has_values(store, CoreOptionLayer::game))
        {
            state->layer = CoreOptionLayer::game;
        }
        else if (has_folder && core_options_store_layer_has_values(store, CoreOptionLayer::directory))
        {
            state->layer = CoreOptionLayer::directory;
        }
    }
    if ((state->layer == CoreOptionLayer::game && !has_game) || (state->layer == CoreOptionLayer::directory && !has_folder))
    {
        state->layer = CoreOptionLayer::core;
    }

    char preview[600] = {};
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    ImGui::TextUnformatted(piccolo_text(context->strings, Str::core_options_save_to));
    ImGui::SetItemTooltip("%s", piccolo_text(context->strings, Str::core_options_save_to_help));
    ImGui::SameLine(start + context->label_width);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (retro_ui_begin_combo("##layer", layer_name(context, state->layer, preview, sizeof(preview))))
    {
        CoreOptionLayer layers[3] = {CoreOptionLayer::core, CoreOptionLayer::directory, CoreOptionLayer::game};
        bool exists[3] = {true, has_folder, has_game};
        for (int i = 0; i < 3; i++)
        {
            if (!exists[i])
            {
                continue;
            }
            char label[600] = {};
            layer_name(context, layers[i], label, sizeof(label) - 16);
            std::snprintf(label + std::strlen(label), 16, "###layer%d", i);
            if (ImGui::Selectable(label, state->layer == layers[i]))
            {
                state->layer = layers[i];
            }
        }
        ImGui::EndCombo();
    }
}

// C / D / G in front of an option that a layer stores for what runs.
static void draw_marker(const CoreOptionsUiContext* context, const char* key)
{
    CoreOptionLayer source = CoreOptionLayer::core;
    const char* stored = core_options_store_get(context->store, key, &source);
    ImVec2 size = ImGui::CalcTextSize("C");
    if (stored == nullptr)
    {
        ImGui::Dummy(size);
        return;
    }
    const char* letter = source == CoreOptionLayer::game ? "G" : (source == CoreOptionLayer::directory ? "D" : "C");
    uint32_t color = source == CoreOptionLayer::game ? MARKER_GAME
                     : (source == CoreOptionLayer::directory ? MARKER_FOLDER : MARKER_CORE);
    Str help = source == CoreOptionLayer::game ? Str::core_options_marker_game_help
               : (source == CoreOptionLayer::directory ? Str::core_options_marker_folder_help
                                                       : Str::core_options_marker_core_help);
    ImGui::TextColored(color_of(color), "%s", letter);
    ImGui::SetItemTooltip("%s", piccolo_text(context->strings, help));
}

static const char* value_label(const LibretroOption* option, uint32_t index)
{
    const char* label = option->value_labels[index];
    return label != nullptr ? label : option->values[index];
}

static void draw_option(const CoreOptionsUiState* state, const CoreOptionsUiContext* context,
                        const LibretroOption* option, bool in_category)
{
    ImGui::PushID(option->key);
    draw_marker(context, option->key);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    float start = ImGui::GetCursorPosX();
    const char* label = in_category && option->label_in_category != nullptr ? option->label_in_category : option->label;
    ImGui::TextUnformatted(label != nullptr ? label : option->key);
    if (option->info != nullptr)
    {
        ImGui::SetItemTooltip("%s", option->info);
    }
    ImGui::SameLine(start + context->label_width);

    // The value as it resolves at the edited layer (or the core's default).
    const char* at_layer = core_options_store_get_at(context->store, state->layer, option->key, nullptr);
    uint32_t current = option->default_index;
    for (uint32_t v = 0; at_layer != nullptr && v < option->value_count; v++)
    {
        if (std::strcmp(option->values[v], at_layer) == 0)
        {
            current = v;
        }
    }
    const char* reset = piccolo_text(context->strings, Str::reset);
    bool stored_here = core_options_store_get_layer(context->store, state->layer, option->key) != nullptr;
    float reset_width = ImGui::CalcTextSize(reset).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - reset_width - ImGui::GetStyle().ItemSpacing.x);
    if (retro_ui_begin_combo("##value", value_label(option, current)))
    {
        for (uint32_t v = 0; v < option->value_count; v++)
        {
            ImGui::PushID(static_cast<int>(v));
            bool picked = ImGui::Selectable(value_label(option, v), v == current);
            ImGui::PopID();
            if (picked && v != current)
            {
                core_session_set_option(context->session, state->layer, option->key, option->values[v]);
            }
        }
        ImGui::EndCombo();
    }
    if (stored_here)
    {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - reset_width);
        if (ImGui::Button(reset))
        {
            core_session_reset_option(context->session, state->layer, option->key);
        }
        ImGui::SetItemTooltip("%s", piccolo_text(context->strings, Str::reset_help_override));
    }
    ImGui::PopID();
}

// The running core's options: uncategorized first, then a section per category.
static void draw_live(CoreOptionsUiState* state, const CoreOptionsUiContext* context)
{
    const LibretroOptions* options = &context->session->host->options;
    draw_layer_combo(state, context);
    if (options->count == 0)
    {
        ImGui::TextDisabled("%s", piccolo_text(context->strings, Str::core_options_none));
        return;
    }
    for (uint32_t i = 0; i < options->count; i++)
    {
        const LibretroOption* option = &options->options[i];
        bool has_category = false;
        for (uint32_t c = 0; option->category_key != nullptr && c < options->category_count; c++)
        {
            has_category = has_category || std::strcmp(options->categories[c].key, option->category_key) == 0;
        }
        if (option->visible && !has_category)
        {
            draw_option(state, context, option, false);
        }
    }
    for (uint32_t c = 0; c < options->category_count; c++)
    {
        const LibretroOptionCategory* category = &options->categories[c];
        char header[300] = {};
        std::snprintf(header, sizeof(header), "%s###category%u", category->label, c);
        if (!ImGui::CollapsingHeader(header))
        {
            continue;
        }
        for (uint32_t i = 0; i < options->count; i++)
        {
            const LibretroOption* option = &options->options[i];
            if (option->visible && option->category_key != nullptr && std::strcmp(option->category_key, category->key) == 0)
            {
                draw_option(state, context, option, true);
            }
        }
    }
}

static const CachedCoreOption* cached_option(const std::vector<CachedCoreOption>* cached, const std::string& key)
{
    for (size_t i = 0; i < cached->size(); i++)
    {
        if ((*cached)[i].key == key)
        {
            return &(*cached)[i];
        }
    }
    return nullptr;
}

// After the stored values changed under the running core: it gets what applies now.
static void reapply(const CoreOptionsUiContext* context)
{
    if (!running(context))
    {
        return;
    }
    LibretroHost* host = context->session->host;
    for (uint32_t i = 0; i < host->options.count; i++)
    {
        const LibretroOption* option = &host->options.options[i];
        const char* stored = core_options_store_get(context->store, option->key, nullptr);
        libretro_host_set_option(host, option->key, stored != nullptr ? stored : option->values[option->default_index]);
    }
}

// What core_options.cfg keeps for `core`, from any core (no need to load it).
static void draw_stored(const CoreOptionsUiState* state, const CoreOptionsUiContext* context)
{
    const Localization* strings = context->strings;
    const char* core = state->selected_core.c_str();
    std::vector<std::string> sections;
    core_options_store_sections_of(context->store, core, &sections);
    if (!ImGui::CollapsingHeader(piccolo_text(strings, Str::core_options_stored), ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }
    if (sections.empty())
    {
        ImGui::TextDisabled("%s", piccolo_text(strings, Str::core_options_stored_none));
        return;
    }
    std::vector<CachedCoreOption> cached;
    core_options_cache_get(context->cache, core, &cached);
    bool changed = false;
    const char* delete_text = piccolo_text(strings, Str::core_options_delete);
    float delete_width = ImGui::CalcTextSize(delete_text).x + ImGui::GetStyle().FramePadding.x * 2.0f;

    for (size_t s = 0; s < sections.size() && !changed; s++)
    {
        ImGui::PushID(static_cast<int>(s));
        const std::string& name = sections[s];
        size_t core_length = std::strlen(core);
        char title[600] = {};
        if (name.size() == core_length)
        {
            std::snprintf(title, sizeof(title), "%s", piccolo_text(strings, Str::core_options_section_core));
        }
        else if (name.compare(core_length, 5, "/dir:") == 0)
        {
            std::snprintf(title, sizeof(title), "%s: %s", piccolo_text(strings, Str::core_options_section_folder),
                          name.c_str() + core_length + 5);
        }
        else
        {
            size_t colon = name.find(':', core_length);
            std::snprintf(title, sizeof(title), "%s: %s", piccolo_text(strings, Str::core_options_section_game),
                          colon != std::string::npos ? name.c_str() + colon + 1 : name.c_str());
        }
        ImGui::SeparatorText(title);
        const char* delete_section = piccolo_text(strings, Str::core_options_delete_section);
        if (ImGui::SmallButton(delete_section))
        {
            changed = core_options_store_remove_section(context->store, name.c_str());
        }

        int32_t index = settings_file_find_section(&context->store->file, name.c_str());
        const SettingsFileSection* section =
            changed || index < 0 ? nullptr : &context->store->file.sections[static_cast<size_t>(index)];
        for (size_t e = 0; section != nullptr && e < section->entries.size() && !changed; e++)
        {
            const SettingsFileEntry* entry = &section->entries[e];
            const CachedCoreOption* option = cached_option(&cached, entry->key);
            const char* value_text = entry->value.c_str();
            for (size_t v = 0; option != nullptr && v < option->values.size(); v++)
            {
                if (option->values[v] == entry->value && !option->labels[v].empty())
                {
                    value_text = option->labels[v].c_str();
                }
            }
            ImGui::PushID(static_cast<int>(e));
            ImGui::AlignTextToFramePadding();
            if (option != nullptr)
            {
                ImGui::Text("%s = %s", option->label.c_str(), value_text);
                ImGui::SetItemTooltip("%s", entry->key.c_str());
            }
            else
            {
                // Without cached definitions (or a key the core dropped): the raw text.
                ImGui::Text("%s = %s", entry->key.c_str(), value_text);
                if (!cached.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", piccolo_text(strings, Str::core_options_gone));
                }
            }
            ImGui::SameLine(ImGui::GetContentRegionMax().x - delete_width);
            if (ImGui::Button(delete_text))
            {
                changed = core_options_store_remove(context->store, name.c_str(), entry->key.c_str());
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    if (!changed && ImGui::Button(piccolo_text(strings, Str::core_options_delete_all)))
    {
        for (size_t s = 0; s < sections.size(); s++)
        {
            core_options_store_remove_section(context->store, sections[s].c_str());
        }
        changed = true;
    }
    ImGui::SetItemTooltip("%s", piccolo_text(strings, Str::core_options_delete_all_help));
    if (changed && state->selected_core == context->running_core)
    {
        reapply(context);
    }
}

void core_options_ui_draw(CoreOptionsUiState* state, const CoreOptionsUiContext* context)
{
    std::vector<std::string> cores;
    core_list(context, &cores);
    if (cores.empty())
    {
        ImGui::TextDisabled("%s", piccolo_text(context->strings, Str::core_options_no_cores));
        return;
    }
    bool known = false;
    for (size_t i = 0; i < cores.size(); i++)
    {
        known = known || cores[i] == state->selected_core;
    }
    if (state->selected_core.empty() || !known)
    {
        state->selected_core = cores[0];   // the running core, when there is one
    }
    draw_core_combo(state, context, &cores);

    if (running(context) && state->selected_core == context->running_core)
    {
        draw_live(state, context);
    }
    else
    {
        ImGui::TextWrapped("%s", piccolo_text(context->strings, Str::core_options_not_running));
    }
    ImGui::Spacing();
    draw_stored(state, context);
}
