#include "ui_mode.h"

// Every mode change goes through here: it closes what the old mode had,
// counts the change (main.cpp lets go of held keys and cancels grabs) and
// forgets the grab (its owner is gone).
static bool change_mode(UiState* state, UiMode mode, UiWidget widget)
{
    if (state->mode == mode && state->widget == widget)
    {
        return false;
    }
    if (mode != UiMode::main_menu)
    {
        state->window_count = 0;   // MAIN MENU's windows close with it
    }
    state->mode = mode;
    state->widget = widget;
    state->grab = UiGrab::none;
    state->grab_window = UiWindow::none;
    state->changes++;
    return true;
}

void ui_game_started(UiState* state)
{
    state->game_running = true;
    state->user_paused = false;
    change_mode(state, UiMode::game, UiWidget::none);   // straight into the game
}

void ui_game_stopped(UiState* state)
{
    if (!state->game_running)
    {
        return;
    }
    state->game_running = false;
    state->user_paused = false;
    change_mode(state, UiMode::main_menu, UiWidget::none);
}

bool ui_enter_game(UiState* state)
{
    if (!state->game_running)
    {
        return false;
    }
    return change_mode(state, UiMode::game, UiWidget::none);
}

bool ui_enter_quick_menu(UiState* state)
{
    if (!state->game_running)
    {
        return false;
    }
    return change_mode(state, UiMode::quick_menu, UiWidget::none);
}

bool ui_enter_widget(UiState* state, UiWidget widget)
{
    if (!state->game_running || widget == UiWidget::none)
    {
        return false;   // widgets are momentary, during play
    }
    return change_mode(state, UiMode::widget, widget);
}

bool ui_enter_main_menu(UiState* state)
{
    return change_mode(state, UiMode::main_menu, UiWidget::none);
}

bool ui_window_open(const UiState* state, UiWindow window)
{
    for (uint32_t i = 0; i < state->window_count; i++)
    {
        if (state->windows[i] == window)
        {
            return true;
        }
    }
    return false;
}

UiWindow ui_top_window(const UiState* state)
{
    return state->window_count > 0 ? state->windows[state->window_count - 1] : UiWindow::none;
}

void ui_remove_window(UiState* state, UiWindow window)
{
    uint32_t kept = 0;
    for (uint32_t i = 0; i < state->window_count; i++)
    {
        if (state->windows[i] != window)
        {
            state->windows[kept] = state->windows[i];
            kept++;
        }
    }
    state->window_count = kept;
}

void ui_push_window(UiState* state, UiWindow window)
{
    if (window == UiWindow::none || window == UiWindow::count)
    {
        return;
    }
    ui_enter_main_menu(state);
    ui_remove_window(state, window);   // already open: to the top
    if (state->window_count < UI_WINDOW_MAX)
    {
        state->windows[state->window_count] = window;
        state->window_count++;
    }
}

void ui_grab_begin(UiState* state, UiGrab grab)
{
    state->grab = grab;
    state->grab_mode = state->mode;
    state->grab_window = state->mode == UiMode::main_menu ? ui_top_window(state) : UiWindow::none;
}

void ui_grab_end(UiState* state)
{
    state->grab = UiGrab::none;
    state->grab_window = UiWindow::none;
}

bool ui_grab_orphaned(const UiState* state)
{
    if (state->grab == UiGrab::none)
    {
        return false;
    }
    if (state->mode != state->grab_mode)
    {
        return true;
    }
    return state->grab_window != UiWindow::none && !ui_window_open(state, state->grab_window);
}

UiBack ui_back(const UiState* state, bool quick_menu_sub_view)
{
    if (state->grab != UiGrab::none)
    {
        return UiBack::cancel_grab;
    }
    switch (state->mode)
    {
        case UiMode::game:
            return UiBack::none;
        case UiMode::quick_menu:
            return quick_menu_sub_view ? UiBack::close_sub_view : UiBack::to_game;
        case UiMode::widget:
            return UiBack::close_widget;
        case UiMode::main_menu:
            if (state->window_count > 0)
            {
                return UiBack::close_window;
            }
            return state->game_running ? UiBack::to_game : UiBack::none;   // the bar stays while nothing runs
    }
    return UiBack::none;
}

uint32_t ui_pause_bit(UiPause cause)
{
    return 1u << static_cast<uint32_t>(cause);
}

// The windows that need the game running: the core waits for its text box,
// KEEP / REVERT measures the frame time, the DVR has its own logic.
static bool widget_keeps_game_running(UiWidget widget)
{
    return widget == UiWidget::text_input || widget == UiWidget::shader_check || widget == UiWidget::dvr;
}

UiDerived ui_derive(const UiState* state, const UiFacts* facts)
{
    UiDerived derived = {};
    bool in_game = state->game_running && state->mode == UiMode::game;
    derived.interface_layer = !in_game;   // any window that isn't a HUD stops the core's input
    derived.keyboard_hotkeys_free = state->mode == UiMode::main_menu;
    derived.overlay_active = in_game;

    if (state->game_running)
    {
        bool menu = !in_game && facts->pause_in_menu && !facts->headless &&
                    !(state->mode == UiMode::widget && widget_keeps_game_running(state->widget));
        if (menu)
        {
            derived.pause |= ui_pause_bit(UiPause::menu);
        }
        if (!facts->focused && facts->pause_on_focus_loss && !facts->streaming)
        {
            derived.pause |= ui_pause_bit(UiPause::focus);
        }
        if (facts->pause_on_disconnect && facts->played_with_gamepad && !facts->gamepad_on_port)
        {
            derived.pause |= ui_pause_bit(UiPause::controllers);
        }
        if (state->user_paused)
        {
            derived.pause |= ui_pause_bit(UiPause::user);
        }
        if (facts->background)
        {
            derived.pause |= ui_pause_bit(UiPause::background);
        }
    }
    derived.frame_delay_interrupt = !in_game || derived.pause != 0;
    return derived;
}
