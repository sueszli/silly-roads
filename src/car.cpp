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
    Vector3 pos = {};
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
    bool initialized = false;
} internal_state;

void ensure_initialized() {
    if (internal_state.initialized) {
        return;
    }
    internal_state.initialized = true;
    internal_state.pos = Terrain::get_start_position();
    internal_state.heading = Terrain::get_start_heading();
}

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

    // Colors for a stylish pickup truck
    const Color body_main = {180, 40, 45, 255};     // deep red
    const Color body_accent = {140, 30, 35, 255};   // darker red accent
    const Color trim_chrome = {200, 200, 210, 255}; // chrome trim
    const Color window_tint = {30, 40, 50, 180};    // tinted windows
    const Color headlight = {255, 250, 220, 255};   // warm headlights
    const Color taillight = {255, 40, 40, 255};     // red taillights
    const Color wheel_rim = {160, 160, 170, 255};   // alloy rims

    rlPushMatrix();
    rlTranslatef(car.pos.x, car.pos.y, car.pos.z);
    rlRotatef(car.heading * RAD2DEG, 0.0f, 1.0f, 0.0f);
    rlRotatef(car.pitch * RAD2DEG, 1.0f, 0.0f, 0.0f);
    rlRotatef(car.roll * RAD2DEG, 0.0f, 0.0f, 1.0f);

    // === CHASSIS / UNDERCARRIAGE ===
    DrawCube({0.0f, 0.15f, 0.0f}, 1.8f, 0.25f, 4.2f, DARKGRAY);

    // === HOOD (front engine section) ===
    DrawCube({0.0f, 0.55f, 1.6f}, 1.9f, 0.5f, 1.2f, body_main);      // hood top
    DrawCube({0.0f, 0.35f, 1.6f}, 1.95f, 0.15f, 1.25f, body_accent); // hood lower

    // === CAB (cabin section) ===
    DrawCube({0.0f, 0.55f, 0.3f}, 1.9f, 0.5f, 1.4f, body_main); // cab lower body
    DrawCube({0.0f, 1.05f, 0.2f}, 1.7f, 0.5f, 1.2f, body_main); // cab upper body (roof area)

    // Roof
    DrawCube({0.0f, 1.35f, 0.2f}, 1.6f, 0.1f, 1.1f, body_accent);

    // Windows (tinted glass) - offset outward to prevent z-fighting with cab
    DrawCube({0.0f, 1.0f, 0.87f}, 1.5f, 0.4f, 0.06f, window_tint);   // windshield
    DrawCube({0.0f, 1.0f, -0.42f}, 1.5f, 0.35f, 0.06f, window_tint); // rear window
    DrawCube({-0.90f, 1.0f, 0.2f}, 0.06f, 0.35f, 0.8f, window_tint); // left window
    DrawCube({0.90f, 1.0f, 0.2f}, 0.06f, 0.35f, 0.8f, window_tint);  // right window

    // A-Pillars (windshield frame) - offset outward
    DrawCube({-0.82f, 1.0f, 0.65f}, 0.08f, 0.45f, 0.12f, body_accent);
    DrawCube({0.82f, 1.0f, 0.65f}, 0.08f, 0.45f, 0.12f, body_accent);

    // === TRUCK BED (hollow trunk) ===
    // Bed floor
    DrawCube({0.0f, 0.4f, -1.3f}, 1.76f, 0.1f, 1.56f, body_accent);

    // Left bed wall
    DrawCube({-0.9f, 0.65f, -1.3f}, 0.12f, 0.45f, 1.6f, body_main);

    // Right bed wall
    DrawCube({0.9f, 0.65f, -1.3f}, 0.12f, 0.45f, 1.6f, body_main);

    // Front bed wall (behind cab)
    DrawCube({0.0f, 0.65f, -0.48f}, 1.76f, 0.45f, 0.12f, body_main);

    // Tailgate (rear)
    DrawCube({0.0f, 0.65f, -2.12f}, 1.8f, 0.45f, 0.1f, body_main);

    // Bed rail trim (chrome strips on top of walls) - raised slightly
    DrawCube({-0.9f, 0.92f, -1.3f}, 0.14f, 0.04f, 1.58f, trim_chrome);
    DrawCube({0.9f, 0.92f, -1.3f}, 0.14f, 0.04f, 1.58f, trim_chrome);
    DrawCube({0.0f, 0.92f, -2.12f}, 1.76f, 0.04f, 0.10f, trim_chrome);

    // === WHEEL ARCHES ===
    // Front wheel arches - offset outward
    DrawCube({-1.01f, 0.35f, 1.5f}, 0.12f, 0.4f, 0.7f, body_accent);
    DrawCube({1.01f, 0.35f, 1.5f}, 0.12f, 0.4f, 0.7f, body_accent);
    // Rear wheel arches
    DrawCube({-1.01f, 0.35f, -1.5f}, 0.12f, 0.4f, 0.7f, body_accent);
    DrawCube({1.01f, 0.35f, -1.5f}, 0.12f, 0.4f, 0.7f, body_accent);

    // === FRONT BUMPER ===
    DrawCube({0.0f, 0.25f, 2.28f}, 2.0f, 0.25f, 0.15f, trim_chrome);
    DrawCube({0.0f, 0.16f, 2.34f}, 1.8f, 0.1f, 0.08f, DARKGRAY);

    // === GRILLE ===
    DrawCube({0.0f, 0.5f, 2.24f}, 1.0f, 0.3f, 0.05f, trim_chrome);
    // Grille slats - offset forward from grille
    for (int i = 0; i < 5; i++) {
        float y_off = 0.42f + i * 0.05f;
        DrawCube({0.0f, y_off, 2.28f}, 0.9f, 0.02f, 0.02f, DARKGRAY);
    }

    // === HEADLIGHTS === - offset forward
    DrawCube({-0.7f, 0.5f, 2.26f}, 0.3f, 0.2f, 0.04f, headlight);
    DrawCube({0.7f, 0.5f, 2.26f}, 0.3f, 0.2f, 0.04f, headlight);
    // Turn signals
    DrawCube({-0.95f, 0.5f, 2.20f}, 0.12f, 0.12f, 0.04f, ORANGE);
    DrawCube({0.95f, 0.5f, 2.20f}, 0.12f, 0.12f, 0.04f, ORANGE);

    // === REAR BUMPER ===
    DrawCube({0.0f, 0.25f, -2.28f}, 2.0f, 0.2f, 0.12f, trim_chrome);

    // === TAILLIGHTS === - offset backward
    DrawCube({-0.75f, 0.65f, -2.18f}, 0.25f, 0.2f, 0.04f, taillight);
    DrawCube({0.75f, 0.65f, -2.18f}, 0.25f, 0.2f, 0.04f, taillight);
    // Reverse lights
    DrawCube({-0.45f, 0.65f, -2.18f}, 0.1f, 0.12f, 0.04f, WHITE);
    DrawCube({0.45f, 0.65f, -2.18f}, 0.1f, 0.12f, 0.04f, WHITE);

    // === SIDE MIRRORS ===
    // Mirror arms - offset outward
    DrawCube({-1.08f, 0.95f, 0.7f}, 0.12f, 0.05f, 0.1f, body_accent);
    DrawCube({1.08f, 0.95f, 0.7f}, 0.12f, 0.05f, 0.1f, body_accent);
    // Mirror housings
    DrawCube({-1.20f, 0.95f, 0.7f}, 0.08f, 0.12f, 0.18f, body_accent);
    DrawCube({1.20f, 0.95f, 0.7f}, 0.08f, 0.12f, 0.18f, body_accent);
    // Mirror glass - offset outward from housing
    DrawCube({-1.26f, 0.95f, 0.7f}, 0.02f, 0.1f, 0.15f, {100, 120, 140, 200});
    DrawCube({1.26f, 0.95f, 0.7f}, 0.02f, 0.1f, 0.15f, {100, 120, 140, 200});

    // === DOOR HANDLES === - offset outward
    DrawCube({-0.98f, 0.75f, 0.35f}, 0.02f, 0.04f, 0.12f, trim_chrome);
    DrawCube({0.98f, 0.75f, 0.35f}, 0.02f, 0.04f, 0.12f, trim_chrome);

    // === WHEELS ===
    for (int i = 0; i < 4; i++) {
        rlPushMatrix();
        rlTranslatef(car.wheels[i].local_offset.x, car.wheels[i].local_offset.y, car.wheels[i].local_offset.z);
        if (i < 2) {
            rlRotatef(car.wheels[i].steering_angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
        }
        // Tire (outer)
        DrawCylinderEx({-0.20f, 0.0f, 0.0f}, {0.20f, 0.0f, 0.0f}, 0.38f, 0.38f, 20, {40, 40, 45, 255});
        // Wheel rim (inner)
        DrawCylinderEx({-0.12f, 0.0f, 0.0f}, {0.12f, 0.0f, 0.0f}, 0.25f, 0.25f, 12, wheel_rim);
        // Hub cap center
        DrawCylinderEx({-0.14f, 0.0f, 0.0f}, {0.14f, 0.0f, 0.0f}, 0.08f, 0.08f, 8, trim_chrome);
        rlPopMatrix();
    }

    rlPopMatrix();
}

} // namespace

namespace Car {

void update(float dt) {
    ensure_initialized();
    read_input();
    Terrain::update(internal_state.pos);
    update_physics(dt);
    draw_car();
}

Vector3 get_position() {
    ensure_initialized();
    return internal_state.pos;
}

float get_heading() {
    ensure_initialized();
    return internal_state.heading;
}

float get_speed() {
    ensure_initialized();
    return internal_state.speed;
}

} // namespace Car
