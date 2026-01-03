#pragma once

#include "raylib.h"
#include <cstdint>
#include <vector>

/** total vertices along one axis; defines both resolution and world size (width = grid_size - 1 units) */
constexpr std::int32_t GRID_SIZE = 120;

/** returns the calculated terrain elevation (y) at world coordinates (x, z) */
float get_terrain_height(float x, float z);

/** returns the normalized surface normal vector at world coordinates (x, z) for physics or lighting */
Vector3 get_terrain_normal(float x, float z);

/** generates a raylib mesh consisting of (grid_size-1)x(grid_size-1) tiles, each 1.0x1.0 units, using world offset (offset_x, offset_z) */
Mesh generate_terrain_mesh_data(float offset_x, float offset_z);
