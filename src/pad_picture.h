#pragma once

// A controller drawn as blocks (MAP CONTROLLER, the quick menu's remap):
// only the parts its layout has, one of them lit; a stick lights for its
// click and its axes, with a mark on the side to push it. A layout with a
// six-button face (Genesis, Saturn) gets two rows of three.

#include "input_layouts.h"

enum class PadView
{
    front,
    back,              // the back, see-through (left stays left): two paddles under each hand
    extra_shoulders,   // the front with L4 / R4 next to L / R
};

// At the cursor, centered, 100 x 46 units big. `layout` nullptr = every part;
// `lit` count = none; `minus`: a lit stick is pushed left / up (else right / down).
// A lit paddle shows the back; a lit L4 / R4, the extra shoulders.
void pad_picture_draw(const InputLayout* layout, InputMapControl lit, bool minus, float unit);

void pad_picture_draw_view(const InputLayout* layout, PadView view, InputMapControl lit, bool minus, float unit);
