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

void update(GameState *state);

Texture2D load_texture();

/** returns the calculated road x-offset at world z coordinate */
float get_road_offset(float z);

} // namespace Terrain
