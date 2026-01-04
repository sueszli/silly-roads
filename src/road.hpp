#pragma once
#include "raylib.h"
#include <cstdint>

/** generates a raylib mesh for the road based on control points */
Mesh generate_road_mesh(const Vector3 *points, std::int32_t count);
