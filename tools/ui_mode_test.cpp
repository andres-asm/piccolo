// Checks piccolo's UI modes (src/ui_mode.h) with no window: the transitions,
// MAIN MENU's stack, what Esc / B does, grabs and their owners, and what is
// derived from the mode (input layer, hotkeys, overlay, pause causes).
//
// Exit code 0 = all checks passed. Each failure is printed.

#include <cstdio>

#include "ui_mode.h"

static int g_failures = 0;

static void check(bool condition, const char* what)
{
    if (!condition)
    {
        std::printf("FAIL %s\n", what);
        g_failures++;
    }
}

static void test_transitions()
{
    UiState state = {};
    check(state.mode == UiMode::main_menu, "start: MAIN MENU (nothing runs)");
    check(!ui_enter_game(&state) && state.mode == UiMode::main_menu, "no game: GAME refused");
    check(!ui_enter_quick_menu(&state), "no game: QUICK MENU refused");
    check(!ui_enter_widget(&state, UiWidget::ports), "no game: widgets refused");
    ui_push_window(&state, UiWindow::load_game);

    uint32_t changes = state.changes;
    ui_game_started(&state);
    check(state.mode == UiMode::game && state.window_count == 0, "a game starts: GAME, LOAD GAME closed");
    check(state.changes == changes + 1, "a game starts: counted as a change");

    check(ui_enter_quick_menu(&state) && state.mode == UiMode::quick_menu, "F1: QUICK MENU");
    check(!ui_enter_quick_menu(&state), "QUICK MENU again: no change");
    check(ui_enter_widget(&state, UiWidget::ports) && state.mode == UiMode::widget && state.widget == UiWidget::ports,
          "a window widget from QUICK MENU replaces it");
    check(ui_enter_widget(&state, UiWidget::text_input) && state.widget == UiWidget::text_input,
          "one window widget at a time");
    check(ui_enter_main_menu(&state) && state.widget == UiWidget::none, "MAIN MENU closes the widget");
    check(ui_enter_quick_menu(&state) && state.mode == UiMode::quick_menu, "F1 in MAIN MENU: QUICK MENU");

    ui_push_window(&state, UiWindow::settings);
    check(state.mode == UiMode::main_menu && ui_top_window(&state) == UiWindow::settings, "a window enters MAIN MENU");
    ui_push_window(&state, UiWindow::choose_folder);
    ui_push_window(&state, UiWindow::settings);
    check(state.window_count == 2 && ui_top_window(&state) == UiWindow::settings, "an open window moves to the top");
    ui_remove_window(&state, UiWindow::settings);
    check(ui_top_window(&state) == UiWindow::choose_folder && state.mode == UiMode::main_menu, "closing one keeps MAIN MENU");
    check(ui_enter_widget(&state, UiWidget::ports) && state.window_count == 0, "F3 in MAIN MENU: its windows close, DEVICES opens");

    changes = state.changes;
    ui_game_stopped(&state);
    check(state.mode == UiMode::main_menu && state.widget == UiWidget::none, "the game stops: MAIN MENU");
    check(state.changes == changes + 1, "the game stops: counted");
}

static void test_back()
{
    UiState state = {};
    check(ui_back(&state, false) == UiBack::none, "back: the bar alone with nothing running stays");
    ui_game_started(&state);
    check(ui_back(&state, false) == UiBack::none, "back: in GAME it's the game's");
    ui_enter_quick_menu(&state);
    check(ui_back(&state, true) == UiBack::close_sub_view, "back: QUICK MENU's sub-view first");
    check(ui_back(&state, false) == UiBack::to_game, "back: then QUICK MENU to GAME");
    ui_enter_widget(&state, UiWidget::shader_check);
    check(ui_back(&state, false) == UiBack::close_widget, "back: a widget's own back");
    ui_push_window(&state, UiWindow::settings);
    check(ui_back(&state, false) == UiBack::close_window, "back: MAIN MENU's top window");
    ui_remove_window(&state, UiWindow::settings);
    check(ui_back(&state, false) == UiBack::to_game, "back: the bar alone to GAME");
    ui_push_window(&state, UiWindow::input);
    ui_grab_begin(&state, UiGrab::capture);
    check(ui_back(&state, false) == UiBack::cancel_grab, "back: a grab is cancelled first");
}

static void test_grabs()
{
    UiState state = {};
    ui_game_started(&state);
    ui_push_window(&state, UiWindow::settings);
    ui_push_window(&state, UiWindow::input);
    ui_grab_begin(&state, UiGrab::capture);
    check(state.grab_window == UiWindow::input && !ui_grab_orphaned(&state), "grab: owned by INPUT");
    ui_push_window(&state, UiWindow::map_controller);
    check(!ui_grab_orphaned(&state), "grab: another window on top keeps it");
    ui_remove_window(&state, UiWindow::input);
    check(ui_grab_orphaned(&state), "grab: INPUT closed (its X): orphaned");
    ui_grab_end(&state);
    check(!ui_grab_orphaned(&state) && state.grab == UiGrab::none, "grab: ended");

    ui_enter_quick_menu(&state);
    ui_grab_begin(&state, UiGrab::macro);
    check(state.grab_mode == UiMode::quick_menu && state.grab_window == UiWindow::none, "grab: the macro recorder is QUICK MENU's");
    uint32_t changes = state.changes;
    ui_enter_game(&state);
    check(state.grab == UiGrab::none && state.changes == changes + 1, "grab: a mode change ends it");
    ui_enter_quick_menu(&state);
    ui_grab_begin(&state, UiGrab::macro);
    state.mode = UiMode::game;   // (as if something changed the mode behind its back)
    check(ui_grab_orphaned(&state), "grab: its mode gone: orphaned");
}

static void test_derive()
{
    UiState state = {};
    UiFacts facts = {};
    facts.pause_in_menu = true;
    UiDerived d = ui_derive(&state, &facts);
    check(d.interface_layer && d.keyboard_hotkeys_free && !d.overlay_active && d.pause == 0,
          "derive: nothing runs: interface, free keyboard hotkeys, no overlay, no pause");

    ui_game_started(&state);
    d = ui_derive(&state, &facts);
    check(!d.interface_layer && !d.keyboard_hotkeys_free && d.overlay_active && d.pause == 0 && !d.frame_delay_interrupt,
          "derive: GAME: the game's input, overlay on, runs");

    ui_enter_quick_menu(&state);
    d = ui_derive(&state, &facts);
    check(d.interface_layer && !d.keyboard_hotkeys_free && !d.overlay_active, "derive: QUICK MENU: interface, hotkeys need Shift");
    check(d.pause == ui_pause_bit(UiPause::menu) && d.frame_delay_interrupt, "derive: QUICK MENU pauses with PAUSE IN MENU");
    facts.pause_in_menu = false;
    d = ui_derive(&state, &facts);
    check(d.pause == 0 && d.frame_delay_interrupt, "derive: without PAUSE IN MENU it runs, no frame delay measuring");
    facts.pause_in_menu = true;

    ui_enter_widget(&state, UiWidget::text_input);
    d = ui_derive(&state, &facts);
    check(d.interface_layer && d.pause == 0, "derive: the text box takes input and doesn't pause");
    ui_enter_widget(&state, UiWidget::shader_check);
    check(ui_derive(&state, &facts).pause == 0, "derive: KEEP / REVERT doesn't pause");
    ui_enter_widget(&state, UiWidget::dvr);
    check(ui_derive(&state, &facts).pause == 0, "derive: the DVR doesn't pause");
    ui_enter_widget(&state, UiWidget::ports);
    check(ui_derive(&state, &facts).pause == ui_pause_bit(UiPause::menu), "derive: DEVICES pauses with PAUSE IN MENU");
    facts.headless = true;
    check(ui_derive(&state, &facts).pause == 0, "derive: headless: no MENU cause");
    facts.headless = false;

    ui_enter_main_menu(&state);
    d = ui_derive(&state, &facts);
    check(d.keyboard_hotkeys_free && d.pause == ui_pause_bit(UiPause::menu), "derive: MAIN MENU: free keyboard hotkeys, MENU");

    ui_enter_game(&state);
    facts.focused = false;
    facts.pause_on_focus_loss = true;
    check(ui_derive(&state, &facts).pause == ui_pause_bit(UiPause::focus), "derive: FOCUS");
    facts.streaming = true;
    check(ui_derive(&state, &facts).pause == 0, "derive: no FOCUS while a device streams");
    facts.streaming = false;
    facts.focused = true;

    facts.pause_on_disconnect = true;
    facts.played_with_gamepad = true;
    check(ui_derive(&state, &facts).pause == ui_pause_bit(UiPause::controllers), "derive: CONTROLLERS");
    facts.gamepad_on_port = true;
    check(ui_derive(&state, &facts).pause == 0, "derive: CONTROLLERS lifts when a pad is back on a port");
    facts.gamepad_on_port = false;
    facts.played_with_gamepad = false;
    check(ui_derive(&state, &facts).pause == 0, "derive: CONTROLLERS lifts when the keyboard plays");

    state.user_paused = true;
    facts.background = true;
    d = ui_derive(&state, &facts);
    check(d.pause == (ui_pause_bit(UiPause::user) | ui_pause_bit(UiPause::background)) && d.frame_delay_interrupt,
          "derive: causes add up (USER + BACKGROUND)");
    ui_game_stopped(&state);
    check(!state.user_paused && ui_derive(&state, &facts).pause == 0, "derive: nothing runs: no pause, USER forgotten");
}

int main()
{
    test_transitions();
    test_back();
    test_grabs();
    test_derive();
    if (g_failures == 0)
    {
        std::printf("ui_mode: all checks passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
