#include "sky.hpp"
#include "raymath.h"

#include <cmath>
#include <cstdint>

namespace {

constexpr float SUN_DISTANCE = 500.0f;
constexpr float SUN_RADIUS = 30.0f;
constexpr float CLOUD_DISTANCE = 300.0f;
constexpr int32_t NUM_CLOUDS = 12;

struct Cloud {
    float angle;     // horizontal angle around camera
    float elevation; // height above horizon
    float scale;     // size multiplier
    float stretch;   // horizontal stretch factor
};

// fixed cloud positions
constexpr Cloud CLOUDS[NUM_CLOUDS] = {
    {0.3f, 0.15f, 1.0f, 1.4f}, {0.8f, 0.20f, 0.8f, 1.2f}, {1.4f, 0.12f, 1.2f, 1.6f}, {2.0f, 0.18f, 0.9f, 1.3f}, {2.5f, 0.25f, 1.1f, 1.5f}, {3.0f, 0.14f, 0.7f, 1.1f}, {3.6f, 0.22f, 1.0f, 1.4f}, {4.2f, 0.16f, 1.3f, 1.7f}, {4.8f, 0.19f, 0.85f, 1.25f}, {5.3f, 0.13f, 1.15f, 1.55f}, {5.8f, 0.21f, 0.95f, 1.35f}, {6.1f, 0.17f, 1.05f, 1.45f},
};

void draw_sun(const Vector3 &camera_pos) {
    constexpr float SUN_ANGLE = 0.6f;      // horizontal angle
    constexpr float SUN_ELEVATION = 0.35f; // how high above horizon (0-1)

    float sun_x = camera_pos.x + std::cos(SUN_ANGLE) * SUN_DISTANCE;
    float sun_z = camera_pos.z + std::sin(SUN_ANGLE) * SUN_DISTANCE;
    float sun_y = camera_pos.y + SUN_DISTANCE * SUN_ELEVATION;

    Vector3 sun_pos = {sun_x, sun_y, sun_z};

    constexpr Color SUN_COLOR = {255, 230, 100, 255};
    constexpr Color GLOW_COLOR = {255, 200, 50, 128};
    DrawSphere(sun_pos, SUN_RADIUS * 1.5f, GLOW_COLOR);
    DrawSphere(sun_pos, SUN_RADIUS * 1.2f, GLOW_COLOR);
    DrawSphere(sun_pos, SUN_RADIUS, SUN_COLOR);
}

void draw_cloud(const Vector3 &camera_pos, const Cloud &cloud) {
    float cloud_x = camera_pos.x + std::cos(cloud.angle) * CLOUD_DISTANCE;
    float cloud_z = camera_pos.z + std::sin(cloud.angle) * CLOUD_DISTANCE;
    float cloud_y = camera_pos.y + CLOUD_DISTANCE * cloud.elevation;

    Vector3 base_pos = {cloud_x, cloud_y, cloud_z};

    constexpr Color CLOUD_COLOR = {255, 255, 255, 230};
    constexpr Color CLOUD_SHADOW = {220, 220, 230, 200};

    float base_radius = 15.0f * cloud.scale;

    // fluffy cloud
    DrawSphere(base_pos, base_radius, CLOUD_COLOR);

    // side puffs
    Vector3 left = base_pos;
    left.x -= base_radius * 0.7f * cloud.stretch;
    DrawSphere(left, base_radius * 0.8f, CLOUD_COLOR);

    Vector3 right = base_pos;
    right.x += base_radius * 0.8f * cloud.stretch;
    DrawSphere(right, base_radius * 0.75f, CLOUD_COLOR);

    // top puffs
    Vector3 top = base_pos;
    top.y += base_radius * 0.5f;
    DrawSphere(top, base_radius * 0.7f, CLOUD_COLOR);

    // bottom shadow
    Vector3 bottom = base_pos;
    bottom.y -= base_radius * 0.3f;
    DrawSphere(bottom, base_radius * 0.6f, CLOUD_SHADOW);
}

} // namespace

namespace Sky {

void draw(const Camera3D &camera) {
    draw_sun(camera.position);

    for (int i = 0; i < NUM_CLOUDS; ++i) {
        draw_cloud(camera.position, CLOUDS[i]);
    }
}

} // namespace Sky
