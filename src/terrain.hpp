#pragma once

#include "raylib.h"
#include <vector>

namespace Terrain {

/** returns the calculated terrain elevation (y) at world coordinates (x, z) */
float get_height(float x, float z);

/** initializes the terrain system, loads textures. Returns the road start position and heading. */
void init(Vector3 &out_pos, float &out_heading);

/** updates the terrain system (chunk generation/unloading) based on car position */
void update(const Vector3 &car_pos);

/** draws the terrain chunks */
void draw();

/** cleans up terrain resources */
void cleanup();

} // namespace Terrain
