#include "terrain.hpp"
#include "raymath.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace {

// TILE_SIZE is now in terrain.hpp
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
    const auto &p = get_permutation();
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

std::vector<std::pair<Vector3, Vector3>> get_road_segments_in_bounds(const std::vector<Vector3> &path, float min_x, float min_z, float max_x, float max_z, float margin) {
    std::vector<std::pair<Vector3, Vector3>> segments;
    if (path.size() < 2) {
        return segments;
    }

    // Expand bounds by margin
    float search_min_x = min_x - margin;
    float search_min_z = min_z - margin;
    float search_max_x = max_x + margin;
    float search_max_z = max_z + margin;

    for (size_t i = 0; i < path.size() - 1; ++i) {
        Vector3 p1 = path[i];
        Vector3 p2 = path[i + 1];

        // Calculate segment bounding box
        float seg_min_x = (p1.x < p2.x) ? p1.x : p2.x;
        float seg_max_x = (p1.x > p2.x) ? p1.x : p2.x;
        float seg_min_z = (p1.z < p2.z) ? p1.z : p2.z;
        float seg_max_z = (p1.z > p2.z) ? p1.z : p2.z;

        // Check for overlap
        if (seg_max_x >= search_min_x && seg_min_x <= search_max_x && seg_max_z >= search_min_z && seg_min_z <= search_max_z) {
            segments.push_back({p1, p2});
        }
    }

    return segments;
}

Vector3 catmull_rom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;

    Vector3 res;
    res.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
    res.y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
    res.z = 0.5f * ((2.0f * p1.z) + (-p0.z + p2.z) * t + (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 + (-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3);
    return res;
}

} // namespace

/** calculates height using 2d world coordinates and noise */
float get_terrain_height(float x, float z) { return sample_noise(x * NOISE_SCALE, 0.0f, z * NOISE_SCALE) * TERRAIN_HEIGHT_SCALE; }

/** calculates surface normal using central difference */
Vector3 get_terrain_normal(float x, float z) {
    const float h = get_terrain_height(x, z);
    constexpr float step = 0.1f;
    const float h_x = get_terrain_height(x + step, z);
    const float h_z = get_terrain_height(x, z + step);

    const Vector3 v1 = {step, h_x - h, 0.0f};
    const Vector3 v2 = {0.0f, h_z - h, step};

    const Vector3 normal = {v2.y * v1.z - v2.z * v1.y, v2.z * v1.x - v2.x * v1.z, v2.x * v1.y - v2.y * v1.x};
    const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (len > 0.0f) {
        return {normal.x / len, normal.y / len, normal.z / len};
    }
    return {0.0f, 1.0f, 0.0f};
}

/** builds the mesh buffers for the grid */
Mesh generate_terrain_mesh_data(float offset_x, float offset_z, const std::vector<Vector3> &road_path) {
    Mesh mesh{};

    // Indexed mesh generation
    constexpr int num_verts = GRID_SIZE * GRID_SIZE;
    constexpr int num_tris = (GRID_SIZE - 1) * (GRID_SIZE - 1) * 2;

    mesh.vertexCount = num_verts;
    mesh.triangleCount = num_tris;

    // allocate memory
    mesh.vertices = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.normals = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.texcoords = static_cast<float *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 2 * sizeof(float)));
    mesh.colors = static_cast<unsigned char *>(MemAlloc(static_cast<std::uint32_t>(mesh.vertexCount) * 4 * sizeof(unsigned char)));
    mesh.indices = static_cast<unsigned short *>(MemAlloc(static_cast<std::uint32_t>(mesh.triangleCount) * 3 * sizeof(unsigned short)));

    assert(mesh.vertices != nullptr);
    assert(mesh.indices != nullptr);

    // 1. Pre-calculate height map (Global Coordinates for Height, Local for Indexing)
    std::vector<float> height_map(num_verts);
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            // Global coords for noise
            float wx = offset_x + (float)x * TILE_SIZE;
            float wz = offset_z + (float)z * TILE_SIZE;
            height_map[size_t(z * GRID_SIZE + x)] = get_terrain_height(wx, wz);
        }
    }

    // 2. Pre-calculate SDF map for road
    std::vector<float> dist_map(num_verts, 1e9f);

    constexpr float margin = 12.0f;
    const float min_x_world = offset_x;
    const float min_z_world = offset_z;
    const float max_x_world = offset_x + (GRID_SIZE - 1) * TILE_SIZE;
    const float max_z_world = offset_z + (GRID_SIZE - 1) * TILE_SIZE;

    auto segments = get_road_segments_in_bounds(road_path, min_x_world, min_z_world, max_x_world, max_z_world, margin);

    constexpr float ROAD_WIDTH = 8.0f;
    constexpr float HALF_ROAD_WIDTH = ROAD_WIDTH * 0.5f;
    constexpr float FADE_WIDTH = 2.0f;

    for (const auto &seg : segments) {
        Vector3 p1 = seg.first;
        Vector3 p2 = seg.second;

        float s_min_x = std::min(p1.x, p2.x) - (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);
        float s_max_x = std::max(p1.x, p2.x) + (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);
        float s_min_z = std::min(p1.z, p2.z) - (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);
        float s_max_z = std::max(p1.z, p2.z) + (HALF_ROAD_WIDTH + FADE_WIDTH + 1.0f);

        int x_start = std::max(0, (int)std::floor((s_min_x - offset_x) / TILE_SIZE));
        int x_end = std::min(GRID_SIZE - 1, (int)std::ceil((s_max_x - offset_x) / TILE_SIZE));
        int z_start = std::max(0, (int)std::floor((s_min_z - offset_z) / TILE_SIZE));
        int z_end = std::min(GRID_SIZE - 1, (int)std::ceil((s_max_z - offset_z) / TILE_SIZE));

        Vector3 ab = Vector3Subtract(p2, p1);
        Vector3 ab2d = {ab.x, 0, ab.z};
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

                float &current = dist_map[(size_t)(z * GRID_SIZE + x)];
                if (dist_sq < current)
                    current = dist_sq;
            }
        }
    }

    // 3. Generate Vertices (LOCAL COORDS) and Normals
    constexpr Color ROAD_COLOR = BROWN;
    constexpr Color TERRAIN_COLOR_1 = DARKGREEN;
    constexpr Color TERRAIN_COLOR_2 = GREEN;

    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int idx = z * GRID_SIZE + x;

            // Local Coordinates for Mesh
            float lx = (float)x * TILE_SIZE;
            float lz = (float)z * TILE_SIZE;
            float wy = height_map[static_cast<std::size_t>(idx)]; // Height is global

            mesh.vertices[idx * 3] = lx;
            mesh.vertices[idx * 3 + 1] = wy;
            mesh.vertices[idx * 3 + 2] = lz;

            // Normals (Requires global coords for noise query if looking up neighbors not in map)
            float wx = offset_x + lx;
            float wz = offset_z + lz;
            Vector3 n = get_terrain_normal(wx, wz);

            mesh.normals[idx * 3] = n.x;
            mesh.normals[idx * 3 + 1] = n.y;
            mesh.normals[idx * 3 + 2] = n.z;

            mesh.texcoords[idx * 2] = 0.0f;
            mesh.texcoords[idx * 2 + 1] = 0.0f;

            // Color
            float dist_sq = dist_map[static_cast<std::size_t>(idx)];
            float dist = std::sqrt(dist_sq);

            bool is_dark = ((x + z) % 2) == 0;
            Color col = is_dark ? TERRAIN_COLOR_1 : TERRAIN_COLOR_2;

            if (dist < HALF_ROAD_WIDTH) {
                col = ROAD_COLOR;
            } else if (dist < HALF_ROAD_WIDTH + FADE_WIDTH) {
                float t = (dist - HALF_ROAD_WIDTH) / FADE_WIDTH;
                unsigned char r = (unsigned char)(ROAD_COLOR.r + t * (col.r - ROAD_COLOR.r));
                unsigned char g = (unsigned char)(ROAD_COLOR.g + t * (col.g - ROAD_COLOR.g));
                unsigned char b = (unsigned char)(ROAD_COLOR.b + t * (col.b - ROAD_COLOR.b));
                col = {r, g, b, 255};
            }

            mesh.colors[idx * 4] = col.r;
            mesh.colors[idx * 4 + 1] = col.g;
            mesh.colors[idx * 4 + 2] = col.b;
            mesh.colors[idx * 4 + 3] = col.a;
        }
    }

    // 4. Generate Indices
    int i_counter = 0;
    for (int z = 0; z < GRID_SIZE - 1; z++) {
        for (int x = 0; x < GRID_SIZE - 1; x++) {
            unsigned short i_tl = (unsigned short)(z * GRID_SIZE + x);
            unsigned short i_bl = (unsigned short)((z + 1) * GRID_SIZE + x);
            unsigned short i_br = (unsigned short)((z + 1) * GRID_SIZE + (x + 1));
            unsigned short i_tr = (unsigned short)(z * GRID_SIZE + (x + 1));

            // T1
            mesh.indices[i_counter++] = i_tl;
            mesh.indices[i_counter++] = i_bl;
            mesh.indices[i_counter++] = i_tr;

            // T2
            mesh.indices[i_counter++] = i_tr;
            mesh.indices[i_counter++] = i_bl;
            mesh.indices[i_counter++] = i_br;
        }
    }

    return mesh;
}

std::vector<Vector3> generate_road_path(const std::vector<Vector3> &control_points) {
    if (control_points.size() < 2) {
        return {};
    }

    const int samples_per_segment = 4;
    const int count = (int)control_points.size();
    const int total_samples = (count - 1) * samples_per_segment + 1;

    std::vector<Vector3> center_points(static_cast<std::size_t>(total_samples));

    for (int i = 0; i < total_samples; i++) {
        float global_t = (float)i / (float)samples_per_segment;
        int segment = (int)global_t;
        float local_t = global_t - (float)segment;

        if (segment >= count - 1) {
            segment = count - 2;
            local_t = 1.0f;
        }

        int i0 = (segment > 0) ? segment - 1 : 0;
        int i1 = segment;
        int i2 = segment + 1;
        int i3 = (segment + 2 < count) ? segment + 2 : count - 1;

        center_points[static_cast<std::size_t>(i)] = catmull_rom(control_points[static_cast<std::size_t>(i0)], control_points[static_cast<std::size_t>(i1)], control_points[static_cast<std::size_t>(i2)], control_points[static_cast<std::size_t>(i3)], local_t);
    }

    return center_points;
}
