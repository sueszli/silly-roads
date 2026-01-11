#pragma once

#include "game_state.hpp"
#include <vector>

namespace Terrain {

/** returns the calculated terrain elevation (y) at world coordinates (x, z) */
float get_height(float x, float z);

/** initializes the terrain system, loads textures, sets initial car state */
void init(GameState &state);

/** updates the terrain system (chunk generation/unloading) */
void update(const GameState &state);

/** draws the terrain chunks */
void draw();

/** cleans up terrain resources */
void cleanup();

} // namespace terrain
