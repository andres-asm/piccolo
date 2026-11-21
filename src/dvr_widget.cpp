#include "dvr_widget.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <SDL3/SDL.h>

#include "core_session.h"
#include "input_bindings.h"
#include "piccolo_settings.h"
#include "piccolo_text.h"
#include "settings.h"

// Previews every 5 s in SCRUB (Andrés: "se ven frames cada 5 segundos").
constexpr double PREVIEW_SECONDS = 5.0;
// LEFT / RIGHT held: the first repeat after this long, then one step every
// STEP_MS; after FAST_MS held, FAST_STEPS at a time.
constexpr uint64_t REPEAT_MS  = 400;
constexpr uint64_t STEP_MS    = 50;
constexpr uint64_t FAST_MS    = 1500;
constexpr int32_t  FAST_STEPS = 4;
constexpr uint64_t NO_FRAME   = ~static_cast<uint64_t>(0);

static double fps_of(const DvrWidget* widget)
{
    return widget->rewind.fps > 1.0 ? widget->rewind.fps : 60.0;
}

// "-1:05" (or "-1:05.3" with tenths) for `frames` before the present.
static void format_back(char* out, size_t size, uint64_t frames, double fps, bool tenths)
{
    double seconds = static_cast<double>(frames) / fps;
    uint32_t whole = static_cast<uint32_t>(seconds);
    if (tenths)
    {
        uint32_t tenth = static_cast<uint32_t>((seconds - static_cast<double>(whole)) * 10.0);
        std::snprintf(out, size, "-%u:%02u.%u", whole / 60, whole % 60, tenth);
    }
    else
    {
        std::snprintf(out, size, "-%u:%02u", whole / 60, whole % 60);
    }
}

static RewindMode mode_from(const char* value)
{
    if (value == nullptr)
    {
        return RewindMode::automatic;
    }
    if (std::strcmp(value, "off") == 0)
    {
        return RewindMode::off;
    }
    if (std::strcmp(value, "scrub") == 0)
    {
        return RewindMode::scrub;
    }
    if (std::strcmp(value, "snapshots") == 0)
    {
        return RewindMode::snapshots;
    }
    return RewindMode::automatic;
}

static uint32_t number_of(const Settings* settings, const char* key)
{
    const char* value = settings_get(settings, key, nullptr);
    return value != nullptr ? static_cast<uint32_t>(std::strtoul(value, nullptr, 10)) : 0;
}

void dvr_widget_game_started(DvrWidget* widget, CoreSession* session, const Settings* settings)
{
    CoreRewindSettings* config = &widget->rewind.settings;
    config->mode             = mode_from(settings_get(settings, PICCOLO_REWIND, nullptr));
    config->known            = mode_from(settings_get_layer(settings, SettingLayer::game, PICCOLO_REWIND_KNOWN));
    config->scrub_seconds    = number_of(settings, PICCOLO_REWIND_SCRUB_LENGTH) * 60;
    config->snapshot_seconds = number_of(settings, PICCOLO_REWIND_SNAPSHOT_EVERY);
    config->snapshots_kept   = number_of(settings, PICCOLO_REWIND_SNAPSHOTS_KEPT);
    if (config->known == RewindMode::off)
    {
        config->known = RewindMode::automatic;
    }

    uint64_t megabytes = number_of(settings, PICCOLO_REWIND_MEMORY);
    if (megabytes == 0)   // AUTO
    {
#if defined(SDL_PLATFORM_ANDROID)
        megabytes = 128;
#else
        megabytes = 256;
#endif
    }
    // Never more than a quarter of the device's memory (Andrés: "el techo
    // que el dispositivo permita").
    int ram = SDL_GetSystemRAM();
    if (ram > 0 && megabytes > static_cast<uint64_t>(ram) / 4)
    {
        megabytes = static_cast<uint64_t>(ram) / 4;
    }
    config->memory_bytes = static_cast<size_t>(megabytes) << 20;

    session->rewind = &widget->rewind;
    if (core_session_running(session))
    {
        core_rewind_start(&widget->rewind, session->host, session->host->av_info.timing.fps);
    }
}

bool dvr_widget_take_verdict(DvrWidget* widget, RewindMode* known)
{
    if (!widget->rewind.verdict)
    {
        return false;
    }
    widget->rewind.verdict = false;
    *known = widget->rewind.state == CoreRewindState::scrub ? RewindMode::scrub : RewindMode::snapshots;
    return true;
}

static void set_hud(DvrWidget* widget, const char* text, uint64_t now_ms)
{
    std::snprintf(widget->hud, sizeof(widget->hud), "%s", text);
    widget->hud_since_ms = now_ms;
}

// The previews: every snapshot, or (SCRUB) the first state of each 5 s.
static void build_previews(DvrWidget* widget)
{
    const RewindBuffer* buffer = &widget->rewind.recorder.buffer;
    bool scrub = widget->rewind.state != CoreRewindState::snapshots;
    widget->preview_frames = scrub ? static_cast<uint64_t>(PREVIEW_SECONDS * fps_of(widget))
                                   : widget->rewind.state_every;
    if (widget->preview_frames == 0)
    {
        widget->preview_frames = 1;
    }
    widget->previews.clear();
    uint64_t next = 0;
    for (uint32_t i = 0; i < buffer->count; i++)
    {
        const RewindRecord* record = rewind_buffer_record(buffer, i);
        if (record->kind != RewindRecordKind::state || (scrub && record->frame < next))
        {
            continue;
        }
        widget->previews.push_back(record->frame);
        next = (record->frame / widget->preview_frames + 1) * widget->preview_frames;
    }
}

bool dvr_widget_open(DvrWidget* widget, const Localization* strings, uint64_t now_ms)
{
    if (widget->open)
    {
        return true;
    }
    CoreRewindState state = widget->rewind.state;
    if (state == CoreRewindState::off)
    {
        set_hud(widget, piccolo_text(strings, Str::dvr_hud_off), now_ms);
        return false;
    }
    if (state == CoreRewindState::unsupported)
    {
        set_hud(widget, piccolo_text(strings, Str::dvr_hud_unsupported), now_ms);
        return false;
    }
    if (!core_rewind_dvr_open(&widget->rewind))
    {
        set_hud(widget, piccolo_text(strings, Str::dvr_hud_nothing), now_ms);
        return false;
    }
    widget->open = true;
    widget->hud_since_ms = 0;
    widget->held_direction = 0;
    for (uint32_t i = 0; i < DVR_VIEW_MAX_THUMBS; i++)
    {
        widget->thumb_used[i] = false;   // the history moved on since: made again
    }
    build_previews(widget);
    return true;
}

void dvr_widget_close(DvrWidget* widget)
{
    if (widget->open)
    {
        core_rewind_dvr_close(&widget->rewind);
        widget->open = false;
    }
}

static void play_from_here(DvrWidget* widget, const Localization* strings, uint64_t now_ms)
{
    CoreRewindPosition position = {};
    core_rewind_dvr_position(&widget->rewind, &position);
    char when[16] = {};
    format_back(when, sizeof(when), position.newest - position.cursor, fps_of(widget), true);
    core_rewind_dvr_resume_here(&widget->rewind);
    widget->open = false;
    char text[96] = {};
    std::snprintf(text, sizeof(text), "%s %s", piccolo_text(strings, Str::dvr_hud_from), when);
    set_hud(widget, text, now_ms);
}

static bool pressed(const InputFrame* frame, InputAction action)
{
    return frame->pressed[static_cast<uint32_t>(action)];
}

static bool held(const InputFrame* frame, InputAction action)
{
    return frame->held[static_cast<uint32_t>(action)];
}

static void jump(DvrWidget* widget, int32_t direction)
{
    CoreRewindPosition position = {};
    core_rewind_dvr_position(&widget->rewind, &position);
    uint64_t target = position.cursor;
    if (direction < 0)
    {
        target = target > widget->preview_frames ? target - widget->preview_frames : 0;
    }
    else
    {
        target += widget->preview_frames;
    }
    core_rewind_dvr_seek(&widget->rewind, target);
}

// The game stopping closes the history's DVR (core_session_stop): the GUI follows.
static void follow_history(DvrWidget* widget)
{
    widget->open = widget->open && widget->rewind.dvr_open;
}

void dvr_widget_update(DvrWidget* widget, const InputFrame* frame, const Localization* strings, uint64_t now_ms)
{
    follow_history(widget);
    if (!widget->open)
    {
        return;
    }
    if (pressed(frame, InputAction::ui_back))
    {
        dvr_widget_close(widget);   // BACK TO NOW
        return;
    }
    if (pressed(frame, InputAction::ui_accept))
    {
        play_from_here(widget, strings, now_ms);
        return;
    }
    if (pressed(frame, InputAction::ui_open_menu))   // RetroPad Y
    {
        core_rewind_dvr_set_playing(&widget->rewind, !widget->rewind.playing);
    }
    if (widget->rewind.state != CoreRewindState::snapshots)
    {
        if (pressed(frame, InputAction::ui_page_left))
        {
            jump(widget, -1);
        }
        if (pressed(frame, InputAction::ui_page_right))
        {
            jump(widget, 1);
        }
    }

    // LEFT / RIGHT: a step when pressed; held, repeated, then faster.
    int32_t direction = held(frame, InputAction::ui_left) ? -1 : held(frame, InputAction::ui_right) ? 1 : 0;
    if (direction == 0)
    {
        widget->held_direction = 0;
        return;
    }
    if (direction != widget->held_direction)
    {
        widget->held_direction = direction;
        widget->held_since_ms = now_ms;
        widget->next_step_ms = now_ms + REPEAT_MS;
        core_rewind_dvr_step(&widget->rewind, direction);
        return;
    }
    if (now_ms >= widget->next_step_ms)
    {
        int32_t steps = now_ms - widget->held_since_ms >= FAST_MS ? FAST_STEPS : 1;
        core_rewind_dvr_step(&widget->rewind, direction * steps);
        widget->next_step_ms = now_ms + STEP_MS;
    }
}

// The previews shown around the cursor: [first, first + count).
static void visible_previews(const DvrWidget* widget, uint64_t cursor, uint32_t* first, uint32_t* count,
                             uint32_t* selected)
{
    uint32_t total = static_cast<uint32_t>(widget->previews.size());
    uint32_t sel = 0;
    for (uint32_t i = 0; i < total; i++)
    {
        if (widget->previews[i] <= cursor)
        {
            sel = i;
        }
    }
    uint32_t half = DVR_VIEW_MAX_THUMBS / 2;
    uint32_t start = sel > half ? sel - half : 0;
    uint32_t end = start + DVR_VIEW_MAX_THUMBS < total ? start + DVR_VIEW_MAX_THUMBS : total;
    *first = start;
    *count = end - start;
    *selected = sel - start;
}

static int32_t thumb_for(const DvrWidget* widget, uint64_t frame)
{
    for (uint32_t i = 0; i < DVR_VIEW_MAX_THUMBS; i++)
    {
        if (widget->thumb_used[i] && widget->thumb_frame[i] == frame)
        {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

void dvr_widget_upload(DvrWidget* widget, CoreSession* session, AppShell* shell)
{
    follow_history(widget);
    // Only when the core may be moved and put back: not while WATCHING, and
    // not after it (the core is then the game at the cursor, kept for B).
    if (!widget->open || widget->rewind.playing || widget->rewind.synced)
    {
        return;
    }
    CoreRewindPosition position = {};
    core_rewind_dvr_position(&widget->rewind, &position);
    uint32_t first = 0;
    uint32_t count = 0;
    uint32_t selected = 0;
    visible_previews(widget, position.cursor, &first, &count, &selected);

    // The missing one nearest the cursor.
    int32_t want = -1;
    for (uint32_t distance = 0; distance < DVR_VIEW_MAX_THUMBS && want < 0; distance++)
    {
        for (int32_t sign = -1; sign <= 1 && want < 0; sign += 2)
        {
            int64_t i = static_cast<int64_t>(selected) + sign * static_cast<int64_t>(distance);
            if (i >= 0 && i < static_cast<int64_t>(count) && thumb_for(widget, widget->previews[first + static_cast<uint32_t>(i)]) < 0)
            {
                want = static_cast<int32_t>(i);
            }
        }
    }
    if (want < 0)
    {
        return;
    }
    // A free texture, or one holding a preview not shown now.
    int32_t slot = -1;
    for (uint32_t t = 0; t < DVR_VIEW_MAX_THUMBS && slot < 0; t++)
    {
        bool shown = false;
        for (uint32_t i = 0; i < count && widget->thumb_used[t]; i++)
        {
            shown = shown || widget->thumb_frame[t] == widget->previews[first + i];
        }
        if (!shown)
        {
            slot = static_cast<int32_t>(t);
        }
    }
    if (slot < 0)
    {
        return;
    }
    // Its picture: the core goes there and comes back to the cursor.
    uint64_t frame = widget->previews[first + static_cast<uint32_t>(want)];
    if (core_rewind_dvr_seek(&widget->rewind, frame) &&
        core_session_copy_picture(session, shell, &widget->thumbs[slot]))
    {
        widget->thumb_used[slot] = true;
        widget->thumb_frame[slot] = frame;
    }
    core_rewind_dvr_seek(&widget->rewind, position.cursor);
}

float dvr_widget_strip_height(const DvrWidget* widget, int32_t scale)
{
    return widget->open && widget->rewind.dvr_open ? dvr_view_height(&widget->view, scale) : 0.0f;
}

// The view from where the cursor is now.
static void build_view(DvrWidget* widget, const Localization* strings, bool touch)
{
    DvrView* view = &widget->view;
    CoreRewindPosition position = {};
    core_rewind_dvr_position(&widget->rewind, &position);
    double fps = fps_of(widget);
    uint32_t first = 0;
    uint32_t count = 0;
    uint32_t selected = 0;
    visible_previews(widget, position.cursor, &first, &count, &selected);

    view->touch = touch;
    view->count = count;
    view->selected = selected;
    view->playing = widget->rewind.playing;
    view->between = 0.0f;
    if (selected + 1 < count)
    {
        uint64_t here = widget->previews[first + selected];
        uint64_t next = widget->previews[first + selected + 1];
        view->between = static_cast<float>(position.cursor - here) / static_cast<float>(next - here);
    }
    for (uint32_t i = 0; i < count; i++)
    {
        DvrViewThumb* thumb = &view->thumbs[i];
        uint64_t frame = widget->previews[first + i];
        format_back(thumb->label, sizeof(thumb->label), position.newest - frame, fps, false);
        thumb->after = frame > position.cursor;
        int32_t t = thumb_for(widget, frame);
        thumb->texture = t >= 0 ? widget->thumbs[t].imgui_id : 0;
        if (t >= 0)
        {
            video_texture_uv_max(&widget->thumbs[t], &thumb->u_max, &thumb->v_max);
            if (widget->thumbs[t].content_height > 0)
            {
                view->aspect = static_cast<float>(widget->thumbs[t].content_width) /
                               static_cast<float>(widget->thumbs[t].content_height);
            }
        }
    }

    format_back(widget->readout, sizeof(widget->readout), position.newest - position.cursor, fps, true);
    char span[16] = {};
    format_back(span, sizeof(span), position.newest - position.oldest, fps, false);
    std::snprintf(widget->kept, sizeof(widget->kept), "%s %s", piccolo_text(strings, Str::dvr_kept), span + 1);   // no "-"
    bool scrub = widget->rewind.state != CoreRewindState::snapshots;
    if (scrub)
    {
        std::snprintf(widget->mode, sizeof(widget->mode), "%s", piccolo_text(strings, Str::dvr_mode_scrub));
    }
    else
    {
        std::snprintf(widget->mode, sizeof(widget->mode), "%s %u S", piccolo_text(strings, Str::dvr_mode_snapshots),
                      widget->rewind.settings.snapshot_seconds);
    }

    // The hints (touch: the same as buttons, with arrows for the steps).
    uint32_t n = 0;
    if (touch)
    {
        view->hints[n++] = {"", "\xE2\x97\x80", false, DvrViewAction::back};   // ◀
    }
    else
    {
        view->hints[n++] = {"\xE2\x97\x80\xE2\x96\xB6", piccolo_text(strings, scrub ? Str::dvr_hint_frame : Str::dvr_hint_snapshot),
                            false, DvrViewAction::none};
        if (scrub)
        {
            view->hints[n++] = {"L R", piccolo_text(strings, Str::dvr_hint_jump), false, DvrViewAction::none};
        }
    }
    view->hints[n++] = {"B", piccolo_text(strings, Str::dvr_play_here), touch, DvrViewAction::play_here};
    view->hints[n++] = {"Y", piccolo_text(strings, widget->rewind.playing ? Str::dvr_pause : Str::dvr_watch),
                        widget->rewind.playing, DvrViewAction::watch};
    view->hints[n++] = {"A", piccolo_text(strings, Str::dvr_back_to_now), false, DvrViewAction::back_to_now};
    if (touch)
    {
        view->hints[n++] = {"", "\xE2\x96\xB6", false, DvrViewAction::forward};   // ▶
    }
    view->hint_count = n;
}

void dvr_widget_draw(DvrWidget* widget, ImFont* pixel_font, int32_t scale, const Localization* strings, bool touch,
                     uint64_t now_ms)
{
    follow_history(widget);
    if (!widget->open)
    {
        return;
    }
    build_view(widget, strings, touch);
    DvrViewText text = {};
    text.title = piccolo_text(strings, widget->rewind.playing ? Str::dvr_watching : Str::dvr_title);
    text.mode = widget->mode;
    text.kept = widget->kept;
    text.readout = widget->readout;
    DvrViewInput input = {};
    dvr_view_draw(&widget->view, pixel_font, scale, &text, &input);

    // A click / tap: as the buttons do.
    if (input.clicked >= 0)
    {
        CoreRewindPosition position = {};
        core_rewind_dvr_position(&widget->rewind, &position);
        uint32_t first = 0;
        uint32_t count = 0;
        uint32_t selected = 0;
        visible_previews(widget, position.cursor, &first, &count, &selected);
        if (static_cast<uint32_t>(input.clicked) < count)
        {
            core_rewind_dvr_seek(&widget->rewind, widget->previews[first + static_cast<uint32_t>(input.clicked)]);
        }
    }
    switch (input.action)
    {
    case DvrViewAction::back:
        core_rewind_dvr_step(&widget->rewind, -1);
        break;
    case DvrViewAction::forward:
        core_rewind_dvr_step(&widget->rewind, 1);
        break;
    case DvrViewAction::play_here:
        play_from_here(widget, strings, now_ms);
        break;
    case DvrViewAction::watch:
        core_rewind_dvr_set_playing(&widget->rewind, !widget->rewind.playing);
        break;
    case DvrViewAction::back_to_now:
        dvr_widget_close(widget);
        break;
    case DvrViewAction::none:
        break;
    }
}

void dvr_widget_draw_hud(DvrWidget* widget, ImFont* pixel_font, int32_t scale, uint64_t now_ms)
{
    dvr_view_draw_hud(pixel_font, scale, widget->hud, widget->hud_since_ms, now_ms);
}

void dvr_widget_note(const DvrWidget* widget, const Localization* strings, char* out, size_t out_size)
{
    out[0] = '\0';
    switch (widget->rewind.state)
    {
    case CoreRewindState::measuring:
        std::snprintf(out, out_size, "%s", piccolo_text(strings, Str::rewind_note_measuring));
        break;
    case CoreRewindState::unsupported:
        std::snprintf(out, out_size, "%s", piccolo_text(strings, Str::rewind_note_unsupported));
        break;
    case CoreRewindState::scrub:
    case CoreRewindState::snapshots:
        if (widget->rewind.settings.mode == RewindMode::automatic)
        {
            std::snprintf(out, out_size, "%s %s.", piccolo_text(strings, Str::rewind_note_chose),
                          piccolo_text(strings, widget->rewind.state == CoreRewindState::scrub ? Str::value_rewind_scrub
                                                                                               : Str::value_rewind_snapshots));
        }
        break;
    case CoreRewindState::off:
        break;
    }
}

void dvr_widget_destroy(DvrWidget* widget, AppShell* shell)
{
    for (uint32_t i = 0; i < DVR_VIEW_MAX_THUMBS; i++)
    {
        video_texture_destroy(&widget->thumbs[i], shell);
        widget->thumb_used[i] = false;
    }
}
