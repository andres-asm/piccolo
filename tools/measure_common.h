#pragma once

// What the measuring tools (stream_measure, netplay_measure) share: the
// scripted pad they play with, and getting a game out of a zip / 7z.

#include <cstdint>
#include <string>

// The scripted pad (RETRO_DEVICE_ID_JOYPAD_* bits) at a frame: START and A
// every 1.5 s for the first 20 s (menus, file select), then RIGHT and Y held
// (run) with B (jump) every half second.
uint16_t measure_script(uint32_t frame);

// The file to load: `game` itself, or for a zip / 7z its first file,
// extracted into `scratch`. False (printed) if there's nothing to load.
bool measure_content(const char* game, const std::string* scratch, std::string* content);
