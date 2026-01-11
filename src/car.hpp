#pragma once

#include "raylib.h"

namespace Car {

/** reads keyboard input and updates car state accordingly */
void read_input();

/** updates the car physics simulation (position, velocity, heading, pitch/roll) */
void update(float dt);

/** draws the car model at its current position */
void draw();

/** initializes the car at the given position and heading */
void init(const Vector3 &pos, float heading);

/** returns the current car position */
Vector3 get_position();

/** returns the current car heading (radians) */
float get_heading();

/** returns the current car speed */
float get_speed();

} // namespace Car
