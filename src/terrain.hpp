#pragma once

#include "raylib.h"
#include <vector>

namespace Terrain {

/** updates the terrain system (chunk generation/unloading) based on car position */
void update(const Vector3 &car_pos);

/** draws the terrain chunks */
void draw();

/** cleans up terrain resources */
void cleanup();

//
// getters
//

/** returns the calculated terrain elevation (y) at world coordinates (x, z) */
float get_height(float x, float z);

/** returns the starting position on the road */
Vector3 get_start_position();

/** returns the starting heading aligned with the road */
float get_start_heading();

} // namespace Terrain
