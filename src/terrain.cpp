#include "terrain.hpp"
#include "game_state.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace Terrain {

namespace {

constexpr float NOISE_SCALE = 0.05f;         // frequency of the perlin noise (lower is smoother)
constexpr float TERRAIN_HEIGHT_SCALE = 7.0f; // maximum amplitude of the terrain height
constexpr float ROAD_NOISE_SCALE = 0.003f;   // Lower frequency for road winding

/** total vertices along one axis; defines both resolution and world size (width = grid_size - 1 units) */
constexpr std::int32_t GRID_SIZE = 128;

/** size of each tile in world units */
constexpr float TILE_SIZE = 1.0f;

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

float get_road_offset(float z) {
    // Determine the X position of the road for a given Z
    // Using simple Perlin noise to create a winding path
    // Multiply by a large amplitude to act as "road bounds"
    return sample_noise(0.0f, 42.0f, z * ROAD_NOISE_SCALE) * 200.0f;
}

Texture2D load_texture() {
    // plain white texture, so vertex colors are unfiltered
    Image white = GenImageColor(2, 2, WHITE);
    Texture2D texture = LoadTextureFromImage(white);
    assert(texture.id != 0);
    UnloadImage(white);
    return texture;
}

} // namespace

float get_height(float x, float z) { return sample_noise(x * NOISE_SCALE, 0.0f, z * NOISE_SCALE) * TERRAIN_HEIGHT_SCALE; }

namespace {

/** calculates surface normal using central difference */
Vector3 get_normal(float x, float z) {
    const float h = get_height(x, z);
    constexpr float step = 0.1f;
    const float h_x = get_height(x + step, z);
    const float h_z = get_height(x, z + step);

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
Mesh generate_mesh_data(float offset_x, float offset_z) {
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

    constexpr Color ROAD_COLOR = BROWN;
    constexpr Color TERRAIN_COLOR_1 = DARKGREEN;
    constexpr Color TERRAIN_COLOR_2 = GREEN;
    constexpr float ROAD_WIDTH = 8.0f;
    constexpr float HALF_ROAD_WIDTH = ROAD_WIDTH * 0.5f;
    constexpr float FADE_WIDTH = 2.0f;

    // Single pass generation!
    for (int z = 0; z < GRID_SIZE; z++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int idx = z * GRID_SIZE + x;

            // Coordinates
            float lx = (float)x * TILE_SIZE;
            float lz = (float)z * TILE_SIZE;
            float wx = offset_x + lx;
            float wz = offset_z + lz;

            // Height and Normal
            float wy = get_height(wx, wz);
            Vector3 n = get_normal(wx, wz);

            // Vertices
            mesh.vertices[idx * 3] = lx;
            mesh.vertices[idx * 3 + 1] = wy;
            mesh.vertices[idx * 3 + 2] = lz;

            // Normals
            mesh.normals[idx * 3] = n.x;
            mesh.normals[idx * 3 + 1] = n.y;
            mesh.normals[idx * 3 + 2] = n.z;

            // Texcoords (dummy)
            mesh.texcoords[idx * 2] = 0.0f;
            mesh.texcoords[idx * 2 + 1] = 0.0f;

            // Road Color Logic
            // Calculate distance to road center at this Z
            float road_center_x = get_road_offset(wz);
            float dist_to_road = std::abs(wx - road_center_x);

            bool is_dark = ((x + z) % 2) == 0;
            Color col = is_dark ? TERRAIN_COLOR_1 : TERRAIN_COLOR_2;

            if (dist_to_road < HALF_ROAD_WIDTH) {
                col = ROAD_COLOR;
            } else if (dist_to_road < HALF_ROAD_WIDTH + FADE_WIDTH) {
                float t = (dist_to_road - HALF_ROAD_WIDTH) / FADE_WIDTH;
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

    // Generate Indices
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

} // namespace

void update(GameState &state) {
    float chunk_world_size = (GRID_SIZE - 1) * TILE_SIZE;
    state.chunk_size = chunk_world_size;

    int center_cx = (int)std::floor(state.car.pos.x / chunk_world_size);
    int center_cz = (int)std::floor(state.car.pos.z / chunk_world_size);

    // 5x5 Grid (radius 2)
    int render_radius = 2; // -2 to +2

    // 1. Identify valid chunks
    // Keep existing chunks that are in range, unload others
    std::erase_if(state.terrain_chunks, [&](const TerrainChunk &chunk) {
        bool in_range = std::abs(chunk.cx - center_cx) <= render_radius && std::abs(chunk.cz - center_cz) <= render_radius;
        if (!in_range) {
            UnloadModel(chunk.model);
        }
        return !in_range;
    });

    // 2. Identify missing chunks and load them
    for (int z = -render_radius; z <= render_radius; z++) {
        for (int x = -render_radius; x <= render_radius; x++) {
            int target_cx = center_cx + x;
            int target_cz = center_cz + z;

            bool exists = std::any_of(state.terrain_chunks.begin(), state.terrain_chunks.end(), [target_cx, target_cz](const TerrainChunk &chunk) { return chunk.cx == target_cx && chunk.cz == target_cz; });

            if (!exists) {
                // Generate chunk
                float ox = (float)target_cx * chunk_world_size;
                float oz = (float)target_cz * chunk_world_size;

                Mesh mesh = generate_mesh_data(ox, oz);
                UploadMesh(&mesh, false);

                Model model = LoadModelFromMesh(mesh);
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = state.texture;

                state.terrain_chunks.push_back({target_cx, target_cz, model});
            }
        }
    }
}

void init(GameState &state) {
    state.texture = load_texture();

    // Initial car placement on road
    float start_z = 0.0f;
    float start_x = get_road_offset(start_z);

    state.car.pos = {start_x, 0.0f, start_z};
    state.car.pos.y = get_height(start_x, start_z) + 2.0f;

    // Face the road direction (approximate derivative)
    float look_ahead_z = start_z + 1.0f;
    float look_ahead_x = get_road_offset(look_ahead_z);

    float dx = look_ahead_x - start_x;
    float dz = look_ahead_z - start_z;
    state.car.heading = std::atan2(dx, dz);

    update(state);
}

void draw(const GameState &state) {
    for (const auto &chunk : state.terrain_chunks) {
        float chunk_world_size = (GRID_SIZE - 1) * TILE_SIZE;
        float ox = (float)chunk.cx * chunk_world_size;
        float oz = (float)chunk.cz * chunk_world_size;
        Vector3 pos = {ox, 0.0f, oz};

        DrawModel(chunk.model, pos, 1.0f, WHITE);
    }
}

} // namespace Terrain
