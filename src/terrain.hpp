#pragma once

#include "raylib.h"
#include <cstdint>
#include <vector>

/** total vertices along one axis; defines both resolution and world size (width = grid_size - 1 units) */
constexpr std::int32_t GRID_SIZE = 128;

/** size of each tile in world units */
constexpr float TILE_SIZE = 1.0f;

/** returns the calculated terrain elevation (y) at world coordinates (x, z) */
float get_terrain_height(float x, float z);

Mesh generate_terrain_mesh_data(float offset_x, float offset_z, const std::vector<Vector3> &road_path);

/** generates the dense path of points for the road using Catmull-Rom splines */
std::vector<Vector3> generate_road_path(const std::vector<Vector3> &control_points);
