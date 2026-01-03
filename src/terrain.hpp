#pragma once

#include "raylib.h"
#include <cstdint>
#include <vector>

// total vertices along one axis. defines both resolution and world size (width = GRID_SIZE - 1 units)
constexpr std::int32_t GRID_SIZE = 120;

// returns the calculated terrain elevation (Y) at world coordinates (x, z)
float get_terrain_height(float x, float z);

// returns the normalized surface normal vector at world coordinates (x, z) for physics or lighting
Vector3 get_terrain_normal(float x, float z);

// generates a raylib Mesh consisting of (GRID_SIZE-1)x(GRID_SIZE-1) tiles, each 1.0x1.0 units
Mesh generate_terrain_mesh_data();
