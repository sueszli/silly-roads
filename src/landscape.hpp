#pragma once

#include "raylib.h"

namespace Landscape {

/** updates landscape elements based on car position (generates/unloads trees) */
void update(const Vector3 &car_pos);

/** draws landscape elements (trees) */
void draw();

/** cleans up landscape resources */
void cleanup();

} // namespace Landscape
