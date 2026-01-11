#include "landscape.hpp"
#include "raymath.h"
#include "terrain.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace {

constexpr float SPAWN_RADIUS = 200.0f;
constexpr float DESPAWN_RADIUS = 250.0f;
constexpr float MIN_SPACING = 8.0f;
constexpr int32_t ELEMENTS_PER_UPDATE = 5;

enum class ElementType { TREE, BUSH };

struct Element {
    Vector3 position;
    ElementType type;
    float size;
    Color color;
};

struct LandscapeState {
    std::vector<Element> elements;
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

void draw_tree(const Element &e) {
    constexpr Color TRUNK_COLOR = {101, 67, 33, 255};
    float trunk_height = e.size * 0.4f;
    float crown_height = e.size * 0.6f;

    Vector3 trunk_top = e.position;
    trunk_top.y += trunk_height;
    DrawCylinderEx(e.position, trunk_top, e.size * 0.08f, e.size * 0.06f, 6, TRUNK_COLOR);

    // layered crown for fuller look
    for (int layer = 0; layer < 3; ++layer) {
        float layer_offset = static_cast<float>(layer) * crown_height * 0.25f;
        float layer_radius = e.size * (0.5f - static_cast<float>(layer) * 0.12f);
        Vector3 base = e.position;
        base.y += trunk_height * 0.7f + layer_offset;
        Vector3 top = base;
        top.y += crown_height * 0.5f;
        Color layer_color = (layer == 1) ? GREEN : e.color;
        DrawCylinderEx(base, top, layer_radius, 0.0f, 8, layer_color);
    }
}

void draw_bush(const Element &e) {
    // round bush made of overlapping spheres
    DrawSphere(e.position, e.size * 0.5f, e.color);
    Vector3 top = e.position;
    top.y += e.size * 0.3f;
    DrawSphere(top, e.size * 0.4f, GREEN);
}

void draw_element(const Element &e) {
    switch (e.type) {
    case ElementType::TREE:
        draw_tree(e);
        break;
    case ElementType::BUSH:
        draw_bush(e);
        break;
    }
}

} // namespace

namespace Landscape {

void update(const Vector3 &car_pos) {
    ensure_initialized();

    std::erase_if(internal_state.elements, [&](const Element &e) {
        float dx = e.position.x - car_pos.x;
        float dz = e.position.z - car_pos.z;
        return std::sqrt(dx * dx + dz * dz) > DESPAWN_RADIUS;
    });

    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> radius_dist(15.0f, SPAWN_RADIUS);
    std::uniform_real_distribution<float> type_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> size_var(0.8f, 1.2f);

    const Color tree_colors[] = {DARKGREEN, {0, 100, 0, 255}, {34, 139, 34, 255}};
    const Color bush_colors[] = {GREEN, DARKGREEN, {107, 142, 35, 255}};

    for (int i = 0; i < ELEMENTS_PER_UPDATE; ++i) {
        float angle = angle_dist(internal_state.rng);
        float radius = radius_dist(internal_state.rng);
        float x = car_pos.x + std::cos(angle) * radius;
        float z = car_pos.z + std::sin(angle) * radius;

        if (is_on_road(x, z)) {
            continue;
        }

        float min_dist = MIN_SPACING;
        bool too_close = std::any_of(internal_state.elements.begin(), internal_state.elements.end(), [x, z, min_dist](const Element &e) {
            float dx = e.position.x - x;
            float dz = e.position.z - z;
            return std::sqrt(dx * dx + dz * dz) < min_dist;
        });

        if (too_close) {
            continue;
        }

        float y = Terrain::get_height(x, z);
        float type_roll = type_dist(internal_state.rng);

        Element elem;
        elem.position = {x, y, z};

        if (type_roll < 0.4f) {
            elem.type = ElementType::TREE;
            elem.size = (5.0f + type_dist(internal_state.rng) * 4.0f) * size_var(internal_state.rng);
            elem.color = tree_colors[internal_state.rng() % 3];
        } else {
            elem.type = ElementType::BUSH;
            elem.size = (1.0f + type_dist(internal_state.rng) * 1.5f) * size_var(internal_state.rng);
            elem.color = bush_colors[internal_state.rng() % 3];
        }

        internal_state.elements.push_back(elem);
    }
}

void draw() {
    ensure_initialized();
    for (const auto &e : internal_state.elements) {
        draw_element(e);
    }
}

void cleanup() { internal_state.elements.clear(); }

} // namespace Landscape
