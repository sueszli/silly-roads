#pragma once

#include "raylib.h"

namespace Car {

/** reads input, updates physics, and draws the car */
void update(float dt);

//
// getters
//

/** returns the current car position */
Vector3 get_position();

/** returns the current car heading (radians) */
float get_heading();

/** returns the current car speed */
float get_speed();

} // namespace Car
