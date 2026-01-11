#pragma once

struct GameState;
#include "game_state.hpp"
#include "raylib.h"
#include <cstdint>
#include <vector>

namespace Terrain {

/** total vertices along one axis; defines both resolution and world size (width = grid_size - 1 units) */
constexpr std::int32_t GRID_SIZE = 128;

/** size of each tile in world units */
constexpr float TILE_SIZE = 1.0f;

/** returns the calculated terrain elevation (y) at world coordinates (x, z) */
float get_height(float x, float z);

/** calculates surface normal */
Vector3 get_normal(float x, float z);

/** generates the dense path of points for the road using Catmull-Rom splines */
std::vector<Vector3> generate_road_path(const std::vector<Vector3> &control_points);

/** updates terrain chunks based on car position */
void update(GameState *state);

Texture2D load_texture();

/** initializes the road state */
void init_road(Components::RoadState &road);

/** updates the road state based on car position */
void update_road(Components::RoadState &road, const Vector3 &car_pos);

} // namespace Terrain
