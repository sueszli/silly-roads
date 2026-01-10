#pragma once
#include "raylib.h"
#include <cstdint>
#include <vector>

/** generates a raylib mesh for the road based on control points */
/**
 * Generates the dense path of points for the road using Catmull-Rom splines.
 * @param control_points Sparse control points defining the road
 * @return Dense list of interpolated points
 */
std::vector<Vector3> generate_road_path(const std::vector<Vector3> &control_points);
