#include "car.hpp"
#include "raylib.h"
#include "rlgl.h"
#include "terrain.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float PHYS_ACCEL = 200.0f;     // acceleration per second
constexpr float PHYS_BRAKE = 400.0f;     // braking/reverse force
constexpr float PHYS_MAX_SPEED = 120.0f; // max speed
constexpr float PHYS_DRAG = 0.98f;       // drag coefficient
constexpr float PHYS_TURN_RATE = 2.0f;   // turn rate in rad/s

struct CarControls {
    float throttle; // -1.0 (brake/reverse) to 1.0 (accel)
    float steer;    // -1.0 (left) to 1.0 (right)
};

struct WheelState {
    Vector3 local_offset; // position relative to car body
    float steering_angle; // only non-zero for front wheels
};

struct CarState {
    Vector3 pos = {60.0f, 20.0f, 60.0f};
    Vector3 vel = {0.0f, 0.0f, 0.0f};
    float heading = 0.0f;
    float speed = 0.0f;
    float pitch = 0.0f; // front/back tilt
    float roll = 0.0f;  // left/right tilt
    WheelState wheels[4] = {
        {{-1.0f, -0.3f, 1.5f}, 0.0f},  // FR
        {{1.0f, -0.3f, 1.5f}, 0.0f},   // FL
        {{-1.0f, -0.3f, -1.5f}, 0.0f}, // BR
        {{1.0f, -0.3f, -1.5f}, 0.0f},  // BL
    };
    CarControls controls = {};
} internal_state;

void read_input() {
    internal_state.controls = {};
    if (IsKeyDown(KEY_D)) {
        internal_state.controls.steer = 1.0f;
    } else if (IsKeyDown(KEY_A)) {
        internal_state.controls.steer = -1.0f;
    }
    if (IsKeyDown(KEY_W)) {
        internal_state.controls.throttle = 1.0f;
    } else if (IsKeyDown(KEY_S)) {
        internal_state.controls.throttle = -1.0f;
    }
}

void update_physics(float dt) {
    auto &car = internal_state;
    const auto &inputs = car.controls;

    // steering
    if (std::abs(car.speed) > 0.5f) {
        float turn_factor = (car.speed > 0.0f) ? 1.0f : -1.0f;
        car.heading -= inputs.steer * PHYS_TURN_RATE * dt * turn_factor;
    }

    // acceleration / braking
    bool has_input = false;
    if (inputs.throttle > 0.0f) {
        car.speed += PHYS_ACCEL * inputs.throttle * dt;
        has_input = true;
    } else if (inputs.throttle < 0.0f) {
        car.speed += PHYS_BRAKE * inputs.throttle * dt;
        has_input = true;
    }

    // speed limits & drag
    car.speed = std::clamp(car.speed, -PHYS_MAX_SPEED, PHYS_MAX_SPEED);
    car.speed *= PHYS_DRAG;
    if (!has_input && std::abs(car.speed) < 0.1f)
        car.speed = 0.0f;

    // velocity & horizontal position
    car.vel.x = std::sin(car.heading) * car.speed;
    car.vel.z = std::cos(car.heading) * car.speed;
    car.pos.x += car.vel.x * dt;
    car.pos.z += car.vel.z * dt;

    // wheel steering animation
    constexpr float MAX_STEER_ANGLE = 0.52f;
    constexpr float STEER_LERP_RATE = 8.0f;
    float target_steer = -inputs.steer * MAX_STEER_ANGLE;
    car.wheels[0].steering_angle += (target_steer - car.wheels[0].steering_angle) * STEER_LERP_RATE * dt;
    car.wheels[1].steering_angle += (target_steer - car.wheels[1].steering_angle) * STEER_LERP_RATE * dt;
    car.wheels[2].steering_angle = 0.0f;
    car.wheels[3].steering_angle = 0.0f;

    // wheel terrain sampling
    const float s = std::sin(car.heading);
    const float c = std::cos(car.heading);
    float h[4];
    float avg_h = 0.0f;
    for (int i = 0; i < 4; i++) {
        const Vector3 off = car.wheels[i].local_offset;
        const float wx = car.pos.x + (off.x * c + off.z * s);
        const float wz = car.pos.z + (-off.x * s + off.z * c);
        h[i] = Terrain::get_height(wx, wz);
        avg_h += h[i];
    }
    avg_h *= 0.25f;

    // vertical position
    float target_y = avg_h + 0.5f;
    if (car.pos.y < target_y)
        car.pos.y = target_y;
    car.pos.y += (target_y - car.pos.y) * 20.0f * dt;

    // pitch and roll
    const float front_h = (h[0] + h[1]) * 0.5f;
    const float back_h = (h[2] + h[3]) * 0.5f;
    const float left_h = (h[0] + h[2]) * 0.5f;
    const float right_h = (h[1] + h[3]) * 0.5f;
    car.pitch += (std::atan2(back_h - front_h, 3.0f) - car.pitch) * 15.0f * dt;
    car.roll += (std::atan2(right_h - left_h, 2.0f) - car.roll) * 15.0f * dt;
}

void draw_car() {
    const auto &car = internal_state;

    rlPushMatrix();
    rlTranslatef(car.pos.x, car.pos.y, car.pos.z);
    rlRotatef(car.heading * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(car.pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(car.roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

    DrawCube({0.0f, 0.5f, 0.0f}, 2.0f, 1.0f, 4.0f, RED);
    DrawCube({0.0f, 0.5f, 1.0f}, 1.8f, 0.8f, 2.0f, MAROON);

    for (int i = 0; i < 4; i++) {
        rlPushMatrix();
        rlTranslatef(car.wheels[i].local_offset.x, car.wheels[i].local_offset.y, car.wheels[i].local_offset.z);
        if (i < 2)
            rlRotatef(car.wheels[i].steering_angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
        DrawCylinderEx({-0.1f, 0.0f, 0.0f}, {0.1f, 0.0f, 0.0f}, 0.3f, 0.3f, 16, DARKGRAY);
        rlPopMatrix();
    }
    rlPopMatrix();
}

} // namespace

namespace Car {

void init(const Vector3 &pos, float heading) {
    internal_state.pos = pos;
    internal_state.heading = heading;
    internal_state.vel = {0.0f, 0.0f, 0.0f};
    internal_state.speed = 0.0f;
    internal_state.pitch = 0.0f;
    internal_state.roll = 0.0f;
}

void update(float dt) {
    read_input();
    Terrain::update(internal_state.pos);
    update_physics(dt);
    draw_car();
}

Vector3 get_position() { return internal_state.pos; }

float get_heading() { return internal_state.heading; }

float get_speed() { return internal_state.speed; }

} // namespace Car
