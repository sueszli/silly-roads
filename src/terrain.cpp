#include "terrain.hpp"
#include "raymath.h"
#include "road.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace {

constexpr float TILE_SIZE = 1.0f;            // world size of a single square tile
constexpr float NOISE_SCALE = 0.05f;         // frequency of the perlin noise (lower is smoother)
constexpr float TERRAIN_HEIGHT_SCALE = 7.0f; // maximum amplitude of the terrain height

/** returns a randomized lookup table for noise generation */
const std::vector<std::int32_t> &get_permutation() {
    static const std::vector<std::int32_t> p = []() {
        std::vector<std::int32_t> vec(256);
        std::iota(vec.begin(), vec.end(), 0);
        constexpr std::int32_t seed = 42;
        std::default_random_engine engine(static_cast<std::uint32_t>(seed));
        std::shuffle(vec.begin(), vec.end(), engine);
        vec.insert(vec.end(), vec.begin(), vec.end());
        assert(vec.size() == 512);
        return vec;
    }();
    return p;
}

/** implements 3d perlin noise at given coordinates */
float sample_noise(float x, float y, float z) {
    assert(!std::isnan(x) && !std::isnan(y) && !std::isnan(z));

    const auto &p = get_permutation();
    assert(p.size() == 512);
    const auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    const auto lerp = [](float t, float a, float b) { return a + t * (b - a); };
    const auto grad = [](std::int32_t hash, float x, float y, float z) {
        const std::int32_t h = hash & 15;
        const float u = h < 8 ? x : y;
        const float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    };

    const std::int32_t X = static_cast<std::int32_t>(std::floor(x)) & 255;
    const std::int32_t Y = static_cast<std::int32_t>(std::floor(y)) & 255;
    const std::int32_t Z = static_cast<std::int32_t>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    const float u = fade(x);
    const float v = fade(y);
    const float w = fade(z);

    const std::int32_t A = p[static_cast<std::size_t>(X)] + Y;
    const std::int32_t AA = p[static_cast<std::size_t>(A)] + Z;
    const std::int32_t AB = p[static_cast<std::size_t>(A) + 1] + Z;
    const std::int32_t B = p[static_cast<std::size_t>(X) + 1] + Y;
    const std::int32_t BA = p[static_cast<std::size_t>(B)] + Z;
    const std::int32_t BB = p[static_cast<std::size_t>(B) + 1] + Z;

    return lerp(w, lerp(v, lerp(u, grad(p[static_cast<std::size_t>(AA)], x, y, z), grad(p[static_cast<std::size_t>(BA)], x - 1, y, z)), lerp(u, grad(p[static_cast<std::size_t>(AB)], x, y - 1, z), grad(p[static_cast<std::size_t>(BB)], x - 1, y - 1, z))), lerp(v, lerp(u, grad(p[static_cast<std::size_t>(AA) + 1], x, y, z - 1), grad(p[static_cast<std::size_t>(BA) + 1], x - 1, y, z - 1)), lerp(u, grad(p[static_cast<std::size_t>(AB) + 1], x, y - 1, z - 1), grad(p[static_cast<std::size_t>(BB) + 1], x - 1, y - 1, z - 1))));
}

} // namespace

/** calculates height using 2d world coordinates and noise */
float get_terrain_height(float x, float z) {
    assert(!std::isnan(x) && !std::isnan(z));
    return sample_noise(x * NOISE_SCALE, 0.0f, z * NOISE_SCALE) * TERRAIN_HEIGHT_SCALE;
}

/** calculates surface normal using central difference */
Vector3 get_terrain_normal(float x, float z) {
    assert(!std::isnan(x) && !std::isnan(z));

    const float h = get_terrain_height(x, z);
    constexpr float step = 0.1f;
    const float h_x = get_terrain_height(x + step, z);
    const float h_z = get_terrain_height(x, z + step);

    const Vector3 v1 = {step, h_x - h, 0.0f};
    const Vector3 v2 = {0.0f, h_z - h, step};

    // cross product v2 x v1 to get upward pointing normal (since raylib is Y-up / right-handed)
    const Vector3 normal = {v2.y * v1.z - v2.z * v1.y, v2.z * v1.x - v2.x * v1.z, v2.x * v1.y - v2.y * v1.x};
    const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    assert(len > 0.0f);
    return {normal.x / len, normal.y / len, normal.z / len};
}

/** builds the mesh buffers for the grid */
Mesh generate_terrain_mesh_data(float offset_x, float offset_z, const std::vector<Vector3> &road_path) {
    Mesh mesh{};
    // each grid cell has two triangles
    mesh.triangleCount = (GRID_SIZE - 1) * (GRID_SIZE - 1) * 2;
    mesh.vertexCount = mesh.triangleCount * 3;

    // allocate memory for mesh attributes
    mesh.vertices = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.normals = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.texcoords = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 2 * sizeof(float)));
    mesh.colors = static_cast<unsigned char *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 4 * sizeof(unsigned char)));

    assert(mesh.vertices != nullptr);
    assert(mesh.normals != nullptr);
    assert(mesh.texcoords != nullptr);
    assert(mesh.colors != nullptr);

    // Filter road segments relevant to this terrain chunk
    // Margin covers road width (8) + fade (2) + some safety
    constexpr float margin = 12.0f;
    const float min_x_world = offset_x;
    const float min_z_world = offset_z;
    const float max_x_world = offset_x + (GRID_SIZE - 1) * TILE_SIZE;
    const float max_z_world = offset_z + (GRID_SIZE - 1) * TILE_SIZE;

    // Pre-calculate distance map (SDF) for the grid
    // Initialize with large distance
    std::vector<float> dist_map(static_cast<size_t>(GRID_SIZE * GRID_SIZE), 1e9f);

    // Retrieve relevant segments
    auto segments = get_road_segments_in_bounds(road_path, min_x_world, min_z_world, max_x_world, max_z_world, margin);

    constexpr float ROAD_WIDTH = 8.0f;
    constexpr float HALF_ROAD_WIDTH = ROAD_WIDTH * 0.5f;
    constexpr float FADE_WIDTH = 2.0f;

    // Fill SDF grid by iterating segments (scatter approach)
    for (const auto &seg : segments) {
        Vector3 p1 = seg.first;
        Vector3 p2 = seg.second;

        // Bounding box of segment in world space (inflated)
        float s_min_x = std::min(p1.x, p2.x) - (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);
        float s_max_x = std::max(p1.x, p2.x) + (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);
        float s_min_z = std::min(p1.z, p2.z) - (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);
        float s_max_z = std::max(p1.z, p2.z) + (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);

        // Convert to local grid indices (clamped)
        int x_start = std::max(0, (int)std::floor((s_min_x - offset_x) / TILE_SIZE));
        int x_end = std::min(GRID_SIZE - 1, (int)std::ceil((s_max_x - offset_x) / TILE_SIZE));
        int z_start = std::max(0, (int)std::floor((s_min_z - offset_z) / TILE_SIZE));
        int z_end = std::min(GRID_SIZE - 1, (int)std::ceil((s_max_z - offset_z) / TILE_SIZE));

        Vector3 ab = Vector3Subtract(p2, p1);
        Vector3 ab2d = {ab.x, 0, ab.z}; // Flatten for 2D distance
        float ab2d_lensq = ab2d.x * ab2d.x + ab2d.z * ab2d.z;

        for (int z = z_start; z <= z_end; z++) {
            for (int x = x_start; x <= x_end; x++) {
                float wx = offset_x + (float)x * TILE_SIZE;
                float wz = offset_z + (float)z * TILE_SIZE;

                Vector3 ap = {wx - p1.x, 0, wz - p1.z};

                float t = 0.0f;
                if (ab2d_lensq > 0.0001f) {
                    t = (ap.x * ab2d.x + ap.z * ab2d.z) / ab2d_lensq;
                    t = std::max(0.0f, std::min(1.0f, t));
                }

                Vector3 closest = {p1.x + ab2d.x * t, 0, p1.z + ab2d.z * t};

                float dx = wx - closest.x;
                float dz = wz - closest.z;
                float dist_sq = dx * dx + dz * dz;

                // Update min distance
                float &current = dist_map[(size_t)(z * GRID_SIZE + x)];
                if (dist_sq < current) {
                    current = dist_sq;
                }
            }
        }
    }

    std::int32_t v_counter = 0;

    // helper to add a vertex to the mesh buffers
    const auto push_vert = [&](std::int32_t grid_x, std::int32_t grid_z, float px, float py, float pz, float nx, float ny, float nz, float u, float v) {
        assert(v_counter < mesh.vertexCount);
        // store position components
        mesh.vertices[v_counter * 3] = px;
        mesh.vertices[v_counter * 3 + 1] = py;
        mesh.vertices[v_counter * 3 + 2] = pz;
        // store normal components
        mesh.normals[v_counter * 3] = nx;
        mesh.normals[v_counter * 3 + 1] = ny;
        mesh.normals[v_counter * 3 + 2] = nz;
        // store texture coordinates
        mesh.texcoords[v_counter * 2] = u;
        mesh.texcoords[v_counter * 2 + 1] = v;

        // Determine color
        constexpr Color ROAD_COLOR = BROWN;
        constexpr Color TERRAIN_COLOR_1 = DARKGREEN;
        constexpr Color TERRAIN_COLOR_2 = GREEN;

        bool is_dark = ((grid_x + grid_z) % 2) == 0;
        Color terrain_col = is_dark ? TERRAIN_COLOR_1 : TERRAIN_COLOR_2;
        Color final_color = terrain_col;

        float dist_sq = dist_map[(size_t)(grid_z * GRID_SIZE + grid_x)];
        float dist = std::sqrt(dist_sq);

        if (dist < HALF_ROAD_WIDTH) {
            final_color = ROAD_COLOR;
        } else if (dist < HALF_ROAD_WIDTH + FADE_WIDTH) {
            float t = (dist - HALF_ROAD_WIDTH) / FADE_WIDTH;
            // Lerp
            unsigned char r = (unsigned char)(ROAD_COLOR.r + t * (terrain_col.r - ROAD_COLOR.r));
            unsigned char g = (unsigned char)(ROAD_COLOR.g + t * (terrain_col.g - ROAD_COLOR.g));
            unsigned char b = (unsigned char)(ROAD_COLOR.b + t * (terrain_col.b - ROAD_COLOR.b));
            final_color = {r, g, b, 255};
        }

        mesh.colors[v_counter * 4] = final_color.r;
        mesh.colors[v_counter * 4 + 1] = final_color.g;
        mesh.colors[v_counter * 4 + 2] = final_color.b;
        mesh.colors[v_counter * 4 + 3] = final_color.a;

        v_counter++;
    };

    const auto process_tile = [&](std::int32_t x, std::int32_t z) {
        // world coordinates for the four corners of a tile
        const float x1 = static_cast<float>(x) * TILE_SIZE;
        const float z1 = static_cast<float>(z) * TILE_SIZE;
        const float x2 = static_cast<float>(x + 1) * TILE_SIZE;
        const float z2 = static_cast<float>(z) * TILE_SIZE;
        const float x3 = static_cast<float>(x) * TILE_SIZE;
        const float z3 = static_cast<float>(z + 1) * TILE_SIZE;
        const float x4 = static_cast<float>(x + 1) * TILE_SIZE;
        const float z4 = static_cast<float>(z + 1) * TILE_SIZE;

        const float y1 = get_terrain_height(x1 + offset_x, z1 + offset_z);
        const float y2 = get_terrain_height(x2 + offset_x, z2 + offset_z);
        const float y3 = get_terrain_height(x3 + offset_x, z3 + offset_z);
        const float y4 = get_terrain_height(x4 + offset_x, z4 + offset_z);

        // get surface normal for shading
        const Vector3 n1 = get_terrain_normal(x1 + offset_x, z1 + offset_z);

        // first triangle of the quad
        push_vert(x, z, x1, y1, z1, n1.x, n1.y, n1.z, 0.0f, 0.0f);
        push_vert(x, z + 1, x3, y3, z3, n1.x, n1.y, n1.z, 0.0f, 1.0f);
        push_vert(x + 1, z, x2, y2, z2, n1.x, n1.y, n1.z, 1.0f, 0.0f);

        // second triangle of the quad (flat normal for now)
        const Vector3 n2 = {0.0f, 1.0f, 0.0f};
        push_vert(x + 1, z, x2, y2, z2, n2.x, n2.y, n2.z, 1.0f, 0.0f);
        push_vert(x, z + 1, x3, y3, z3, n2.x, n2.y, n2.z, 0.0f, 1.0f);
        push_vert(x + 1, z + 1, x4, y4, z4, n2.x, n2.y, n2.z, 1.0f, 1.0f);
    };

    // iterate through the grid to generate vertices and normals
    for (std::int32_t z = 0; z < GRID_SIZE - 1; z++) {
        for (std::int32_t x = 0; x < GRID_SIZE - 1; x++) {
            process_tile(x, z);
        }
    }
    assert(v_counter == mesh.vertexCount);
    return mesh;
}
