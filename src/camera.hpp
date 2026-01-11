#pragma once

#include "game_state.hpp"
#include "raylib.h"

namespace CameraSystem {

// We need car state for heading to calculate chase position
void update(Components::CameraState &cam, const Components::Car &car, float dt);

} // namespace CameraSystem
