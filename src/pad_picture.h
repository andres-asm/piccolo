#pragma once

// A controller drawn as blocks (MAP CONTROLLER's STEP 2, the quick menu's
// remap): only the parts its layout has, one of them lit; a stick lights for
// its click and its axes, with a mark on the side to push it.

#include "input_layouts.h"

// At the cursor, centered, 100 x 46 units big. `layout` nullptr = every part;
// `lit` count = none; `minus`: a lit stick is pushed left / up (else right / down).
void pad_picture_draw(const InputLayout* layout, InputMapControl lit, bool minus, float unit);
