#include "landscape.hpp"
#include "raymath.h"
#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

constexpr float TREE_SPAWN_RADIUS = 150.0f;
constexpr float TREE_DESPAWN_RADIUS = 200.0f;
constexpr float MIN_TREE_SPACING = 8.0f;
constexpr int32_t TREES_PER_UPDATE = 5;

struct Tree {
    Vector3 position;
    float height;
    float trunk_radius;
    float crown_radius;
};

struct LandscapeState {
    std::vector<Tree> trees;
    std::mt19937 rng{42};
    bool initialized = false;
} internal_state;

void ensure_initialized() {
    if (internal_state.initialized) {
        return;
    }
    internal_state.initialized = true;
}

bool is_on_road(float x, float z) {
    float road_center = Terrain::get_road_center_x(z);
    return std::abs(x - road_center) < 8.0f;
}

void draw_tree(const Tree &tree) {
    // trunk
    constexpr Color TRUNK_COLOR = {101, 67, 33, 255};
    Vector3 trunk_top = tree.position;
    trunk_top.y += tree.height * 0.4f;
    DrawCylinderEx(tree.position, trunk_top, tree.trunk_radius, tree.trunk_radius, 6, TRUNK_COLOR);
    // crown
    Vector3 crown_base = tree.position;
    crown_base.y += tree.height * 0.35f;
    Vector3 crown_top = crown_base;
    crown_top.y += tree.height * 0.65f;
    DrawCylinderEx(crown_base, crown_top, tree.crown_radius, 0.0f, 8, DARKGREEN);
}

} // namespace

namespace Landscape {

void update(const Vector3 &car_pos) {
    ensure_initialized();

    // remove distant trees
    std::erase_if(internal_state.trees, [&](const Tree &t) {
        float dx = t.position.x - car_pos.x;
        float dz = t.position.z - car_pos.z;
        return std::sqrt(dx * dx + dz * dz) > TREE_DESPAWN_RADIUS;
    });

    // spawn new trees around car
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> radius_dist(20.0f, TREE_SPAWN_RADIUS);
    std::uniform_real_distribution<float> height_dist(4.0f, 8.0f);
    std::uniform_real_distribution<float> trunk_dist(0.3f, 0.5f);
    std::uniform_real_distribution<float> crown_dist(2.0f, 3.5f);

    for (int i = 0; i < TREES_PER_UPDATE; ++i) {
        float angle = angle_dist(internal_state.rng);
        float radius = radius_dist(internal_state.rng);

        float x = car_pos.x + std::cos(angle) * radius;
        float z = car_pos.z + std::sin(angle) * radius;

        // skip if on road
        if (is_on_road(x, z)) {
            continue;
        }

        // check spacing with existing trees
        bool too_close = std::any_of(internal_state.trees.begin(), internal_state.trees.end(), [x, z](const Tree &t) {
            float dx = t.position.x - x;
            float dz = t.position.z - z;
            return std::sqrt(dx * dx + dz * dz) < MIN_TREE_SPACING;
        });

        if (too_close) {
            continue;
        }

        float y = Terrain::get_height(x, z);
        Tree tree{.position = {x, y, z}, .height = height_dist(internal_state.rng), .trunk_radius = trunk_dist(internal_state.rng), .crown_radius = crown_dist(internal_state.rng)};
        internal_state.trees.push_back(tree);
    }
}

void draw() {
    ensure_initialized();
    for (const auto &tree : internal_state.trees) {
        draw_tree(tree);
    }
}

void cleanup() { internal_state.trees.clear(); }

} // namespace Landscape
