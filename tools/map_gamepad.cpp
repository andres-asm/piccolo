// Maps a controller SDL doesn't know as a gamepad (a joystick with no
// gamepad mapping): asks for each control in turn, builds an SDL gamepad
// mapping for its GUID and keeps it in piccolo's settings/gamepad_mappings.txt,
// which piccolo loads at start. From then on SDL reports it as a gamepad and
// piccolo gives it the default layout like any other.
//
// A stopgap until CONFIG > INPUT does this inside piccolo (milestone 3).
//
// Usage: piccolo_map_gamepad [joystick number]   (default: the first unmapped one)

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "app_paths.h"

constexpr int16_t  AXIS_PUSHED = 16000;      // past this from rest, an axis counts as pushed
constexpr uint64_t REQUIRED_WAIT_MS = 30000;
constexpr uint64_t OPTIONAL_WAIT_MS = 6000;

struct Control
{
    const char* sdl_name;   // in the mapping
    const char* prompt;
    bool        optional;
    bool        stick;      // an axis wanted as a whole: push it right (x) or down (y)
};

static const Control CONTROLS[] = {
    {"dpup", "D-pad UP", false, false},
    {"dpdown", "D-pad DOWN", false, false},
    {"dpleft", "D-pad LEFT", false, false},
    {"dpright", "D-pad RIGHT", false, false},
    {"a", "B (the bottom face button)", false, false},
    {"b", "A (the right face button)", false, false},
    {"x", "Y (the left face button)", false, false},
    {"y", "X (the top face button)", false, false},
    {"leftshoulder", "L", false, false},
    {"rightshoulder", "R", false, false},
    {"back", "SELECT", false, false},
    {"start", "START", false, false},
    {"lefttrigger", "L2 (if it has one)", true, false},
    {"righttrigger", "R2 (if it has one)", true, false},
    {"leftstick", "L3, left stick click (if it has one)", true, false},
    {"rightstick", "R3, right stick click (if it has one)", true, false},
    {"guide", "HOME / GUIDE (if it has one)", true, false},
    {"leftx", "left stick: push it RIGHT (if it has one)", true, true},
    {"lefty", "left stick: push it DOWN (if it has one)", true, true},
    {"rightx", "right stick: push it RIGHT (if it has one)", true, true},
    {"righty", "right stick: push it DOWN (if it has one)", true, true},
};
constexpr size_t CONTROL_COUNT = sizeof(CONTROLS) / sizeof(CONTROLS[0]);

// Everything back at rest (buttons up, hats centered, axes near where they started).
static bool at_rest(SDL_Joystick* joystick, const std::vector<int16_t>& rest)
{
    SDL_UpdateJoysticks();
    for (int b = 0; b < SDL_GetNumJoystickButtons(joystick); b++)
    {
        if (SDL_GetJoystickButton(joystick, b))
        {
            return false;
        }
    }
    for (int h = 0; h < SDL_GetNumJoystickHats(joystick); h++)
    {
        if (SDL_GetJoystickHat(joystick, h) != SDL_HAT_CENTERED)
        {
            return false;
        }
    }
    for (int a = 0; a < SDL_GetNumJoystickAxes(joystick); a++)
    {
        int delta = SDL_GetJoystickAxis(joystick, a) - rest[static_cast<size_t>(a)];
        if (delta > AXIS_PUSHED || delta < -AXIS_PUSHED)
        {
            return false;
        }
    }
    return true;
}

// Waits for one control; writes its mapping element ("b3", "h0.1", "+a2",
// "a5", "a1~"). False on timeout.
static bool wait_for_control(SDL_Joystick* joystick, const std::vector<int16_t>& rest, const Control* control,
                             uint64_t wait_ms, std::string* element)
{
    SDL_JoystickID id = SDL_GetJoystickID(joystick);
    uint64_t until = SDL_GetTicks() + wait_ms;
    while (SDL_GetTicks() < until)
    {
        SDL_Event event;
        if (!SDL_WaitEventTimeout(&event, 100))
        {
            continue;
        }
        char text[32] = {};
        if (event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN && event.jbutton.which == id && !control->stick)
        {
            std::snprintf(text, sizeof(text), "b%u", static_cast<unsigned>(event.jbutton.button));
        }
        else if (event.type == SDL_EVENT_JOYSTICK_HAT_MOTION && event.jhat.which == id && !control->stick &&
                 (event.jhat.value == SDL_HAT_UP || event.jhat.value == SDL_HAT_RIGHT || event.jhat.value == SDL_HAT_DOWN ||
                  event.jhat.value == SDL_HAT_LEFT))
        {
            std::snprintf(text, sizeof(text), "h%u.%u", static_cast<unsigned>(event.jhat.hat), static_cast<unsigned>(event.jhat.value));
        }
        else if (event.type == SDL_EVENT_JOYSTICK_AXIS_MOTION && event.jaxis.which == id)
        {
            int16_t at_rest_value = rest[event.jaxis.axis];
            int delta = event.jaxis.value - at_rest_value;
            if (delta <= AXIS_PUSHED && delta >= -AXIS_PUSHED)
            {
                continue;
            }
            unsigned axis = event.jaxis.axis;
            if (control->stick)
            {
                std::snprintf(text, sizeof(text), delta > 0 ? "a%u" : "a%u~", axis);
            }
            else if (at_rest_value < -AXIS_PUSHED)
            {
                std::snprintf(text, sizeof(text), "a%u", axis);   // a trigger resting at one end: the whole range
            }
            else
            {
                std::snprintf(text, sizeof(text), delta > 0 ? "+a%u" : "-a%u", axis);
            }
        }
        if (text[0] != '\0')
        {
            *element = text;
            while (!at_rest(joystick, rest))
            {
                SDL_Delay(20);
            }
            SDL_FlushEvents(SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_JOYSTICK_BUTTON_UP);
            return true;
        }
    }
    return false;
}

// The mapping without the platform or a trailing comma problem: GUID,name,elements...
static void keep_mapping(const std::string& path, const std::string& guid, const std::string& line)
{
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string existing;
    while (std::getline(in, existing))
    {
        if (existing.compare(0, guid.size(), guid) != 0)
        {
            lines.push_back(existing);   // other controllers' mappings stay
        }
    }
    in.close();
    lines.push_back(line);
    std::ofstream out(path, std::ios::trunc);
    for (size_t i = 0; i < lines.size(); i++)
    {
        out << lines[i] << "\n";
    }
}

int main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_GAMEPAD))
    {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    AppPaths paths = {};
    if (!app_paths_init(&paths, "piccolo", "piccolo.cfg"))
    {
        std::printf("Can't find piccolo's folder\n");
        return 1;
    }
    std::string mappings_path = paths.settings + "/gamepad_mappings.txt";
    SDL_AddGamepadMappingsFromFile(mappings_path.c_str());

    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    std::printf("Joysticks:\n");
    int wanted = argc >= 2 ? std::atoi(argv[1]) : -1;
    SDL_JoystickID chosen = 0;
    for (int i = 0; ids != nullptr && i < count; i++)
    {
        bool gamepad = SDL_IsGamepad(ids[i]);
        std::printf("  %d: %s%s\n", i, SDL_GetJoystickNameForID(ids[i]), gamepad ? " (already a gamepad)" : "");
        if ((wanted == i) || (wanted < 0 && chosen == 0 && !gamepad))
        {
            chosen = ids[i];
        }
    }
    SDL_free(ids);
    if (chosen == 0)
    {
        std::printf("No unmapped joystick (give its number to map one anyway).\n");
        SDL_Quit();
        return 1;
    }

    SDL_Joystick* joystick = SDL_OpenJoystick(chosen);
    if (joystick == nullptr)
    {
        std::printf("Can't open it: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    char guid[64] = {};
    SDL_GUIDToString(SDL_GetJoystickGUID(joystick), guid, sizeof(guid));
    std::string name = SDL_GetJoystickName(joystick);
    for (size_t i = 0; i < name.size(); i++)
    {
        name[i] = name[i] == ',' ? ' ' : name[i];   // a comma would split the mapping
    }
    SDL_Delay(200);   // let the first axis values arrive
    std::vector<int16_t> rest;
    SDL_UpdateJoysticks();
    for (int a = 0; a < SDL_GetNumJoystickAxes(joystick); a++)
    {
        rest.push_back(SDL_GetJoystickAxis(joystick, a));
    }
    std::printf("\nMapping %s (%s). Press each control when asked; optional ones are skipped after %u s.\n\n",
                name.c_str(), guid, static_cast<unsigned>(OPTIONAL_WAIT_MS / 1000));

    std::string line = std::string(guid) + "," + name + ",";
    std::vector<std::string> used;
    for (size_t c = 0; c < CONTROL_COUNT; c++)
    {
        const Control* control = &CONTROLS[c];
        std::printf("  %-45s ", control->prompt);
        std::fflush(stdout);
        std::string element;
        bool got = false;
        while (true)
        {
            got = wait_for_control(joystick, rest, control, control->optional ? OPTIONAL_WAIT_MS : REQUIRED_WAIT_MS, &element);
            bool again = false;
            for (size_t u = 0; got && u < used.size(); u++)
            {
                again = again || used[u] == element;
            }
            if (!again)
            {
                break;
            }
            std::printf("(already used) ");
            std::fflush(stdout);
        }
        if (!got)
        {
            std::printf("%s\n", control->optional ? "skipped" : "nothing pressed, stopping");
            if (!control->optional)
            {
                SDL_CloseJoystick(joystick);
                SDL_Quit();
                return 1;
            }
            continue;
        }
        std::printf("%s\n", element.c_str());
        used.push_back(element);
        line += std::string(control->sdl_name) + ":" + element + ",";
    }
    line += std::string("platform:") + SDL_GetPlatform() + ",";

    SDL_CloseJoystick(joystick);
    if (SDL_AddGamepadMapping(line.c_str()) < 0)
    {
        std::printf("\nSDL refused the mapping: %s\n%s\n", SDL_GetError(), line.c_str());
        SDL_Quit();
        return 1;
    }
    keep_mapping(mappings_path, guid, line);
    std::printf("\nKept in %s:\n%s\n%s\n", mappings_path.c_str(), line.c_str(),
                SDL_IsGamepad(chosen) ? "SDL now sees it as a gamepad. Start piccolo again." : "");
    SDL_Quit();
    return 0;
}
