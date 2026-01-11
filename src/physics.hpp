#pragma once

#include "game_state.hpp"
#include "terrain.hpp"

namespace Physics {

void update_car_physics(Components::Car &car, const Components::CarControls &inputs, float dt);

} // namespace Physics
