#include "physics.hpp"
#include "terrain.hpp"

#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace Physics {

// Internal constants moved from main.cpp
constexpr float PHYS_ACCEL = 200.0f;     // acceleration per second
constexpr float PHYS_BRAKE = 400.0f;     // braking/reverse force
constexpr float PHYS_MAX_SPEED = 120.0f; // max speed
constexpr float PHYS_DRAG = 0.98f;       // drag coefficient
constexpr float PHYS_TURN_RATE = 2.0f;   // turn rate in rad/s

void update_car_physics(Components::Car &car, const Components::CarControls &inputs, float dt) {
    //
    // body - horizontal movement
    //

    // Steering
    if (std::abs(car.speed) > 0.5f) {
        float turn_factor = (car.speed > 0.0f) ? 1.0f : -1.0f; // reverse steering when going backward
        car.heading -= inputs.steer * PHYS_TURN_RATE * dt * turn_factor;
    }

    // Acceleration / Braking
    bool has_input = false;
    if (inputs.throttle > 0.0f) {
        car.speed += PHYS_ACCEL * inputs.throttle * dt;
        has_input = true;
    } else if (inputs.throttle < 0.0f) {
        car.speed += PHYS_BRAKE * inputs.throttle * dt; // throttle is negative here
        has_input = true;
    }

    // speed limits & drag
    if (car.speed > PHYS_MAX_SPEED)
        car.speed = PHYS_MAX_SPEED;
    if (car.speed < -PHYS_MAX_SPEED)
        car.speed = -PHYS_MAX_SPEED;
    car.speed *= PHYS_DRAG;
    if (!has_input && std::abs(car.speed) < 0.1f)
        car.speed = 0.0f;

    // calculate velocity & update horizontal position
    // moving body before sampling wheels prevents clipping at high speed
    car.vel.x = std::sin(car.heading) * car.speed;
    car.vel.z = std::cos(car.heading) * car.speed;
    car.pos.x += car.vel.x * dt;
    car.pos.z += car.vel.z * dt;

    //
    // wheel update
    //

    // wheel steering animation lerp
    constexpr float MAX_STEER_ANGLE = 0.52f; // 30 degrees
    constexpr float STEER_LERP_RATE = 8.0f;
    float target_steer = -inputs.steer * MAX_STEER_ANGLE;

    car.wheels[0].steering_angle += (target_steer - car.wheels[0].steering_angle) * STEER_LERP_RATE * dt;
    car.wheels[1].steering_angle += (target_steer - car.wheels[1].steering_angle) * STEER_LERP_RATE * dt;
    car.wheels[2].steering_angle = 0.0f;
    car.wheels[3].steering_angle = 0.0f;

    // wheel terrain sampling at new positions
    const float s = std::sin(car.heading);
    const float c = std::cos(car.heading);
    float h[4];
    float avg_h = 0.0f;
    for (int i = 0; i < 4; i++) {
        const Vector3 off = car.wheels[i].local_offset;
        const float wx = car.pos.x + (off.x * c + off.z * s);
        const float wz = car.pos.z + (-off.x * s + off.z * c);

        // Using global get_terrain_height from terrain.hpp as per plan/instructions
        h[i] = Terrain::get_height(wx, wz);

        car.wheel_heights[i] = h[i];
        avg_h += h[i];
    }
    avg_h *= 0.25f;

    //
    // body - vertical movement
    //

    float target_y = avg_h + 0.5f;
    if (car.pos.y < target_y)
        car.pos.y = target_y;
    car.pos.y += (target_y - car.pos.y) * 20.0f * dt;

    // body pitch and roll
    const float front_h = (h[0] + h[1]) * 0.5f;
    const float back_h = (h[2] + h[3]) * 0.5f;
    const float left_h = (h[0] + h[2]) * 0.5f;
    const float right_h = (h[1] + h[3]) * 0.5f;

    const float target_pitch = std::atan2(back_h - front_h, 3.0f);
    const float target_roll = std::atan2(right_h - left_h, 2.0f);

    car.pitch += (target_pitch - car.pitch) * 15.0f * dt;
    car.roll += (target_roll - car.roll) * 15.0f * dt;
}

} // namespace Physics
