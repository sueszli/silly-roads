#include "terrain.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace {

constexpr float TILE_SIZE = 1.0f;             // world size of a single square tile
constexpr float NOISE_SCALE = 0.1f;           // frequency of the perlin noise (lower is smoother)
constexpr float TERRAIN_HEIGHT_SCALE = 10.0f; // maximum amplitude of the terrain height

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

    const Vector3 normal = {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
    const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    assert(len > 0.0f);
    return {normal.x / len, normal.y / len, normal.z / len};
}

// builds the mesh buffers for the grid
Mesh generate_terrain_mesh_data() {
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

    std::int32_t vCounter = 0;

    // iterate through the grid to generate vertices and normals
    for (std::int32_t z = 0; z < GRID_SIZE - 1; z++) {
        for (std::int32_t x = 0; x < GRID_SIZE - 1; x++) {
            // world coordinates for the four corners of a tile
            const float x1 = static_cast<float>(x) * TILE_SIZE;
            const float z1 = static_cast<float>(z) * TILE_SIZE;
            const float x2 = static_cast<float>(x + 1) * TILE_SIZE;
            const float z2 = static_cast<float>(z) * TILE_SIZE;
            const float x3 = static_cast<float>(x) * TILE_SIZE;
            const float z3 = static_cast<float>(z + 1) * TILE_SIZE;
            const float x4 = static_cast<float>(x + 1) * TILE_SIZE;
            const float z4 = static_cast<float>(z + 1) * TILE_SIZE;

            const float y1 = get_terrain_height(x1, z1);
            const float y2 = get_terrain_height(x2, z2);
            const float y3 = get_terrain_height(x3, z3);
            const float y4 = get_terrain_height(x4, z4);

            // get surface normal for shading
            const Vector3 n1 = get_terrain_normal(x1, z1);

            // helper to add a vertex to the mesh buffers
            auto PushVert = [&](float px, float py, float pz, float nx, float ny, float nz, float u, float v) {
                assert(vCounter < mesh.vertexCount);
                // store position components
                mesh.vertices[vCounter * 3] = px;
                mesh.vertices[vCounter * 3 + 1] = py;
                mesh.vertices[vCounter * 3 + 2] = pz;
                // store normal components
                mesh.normals[vCounter * 3] = nx;
                mesh.normals[vCounter * 3 + 1] = ny;
                mesh.normals[vCounter * 3 + 2] = nz;
                // store texture coordinates
                mesh.texcoords[vCounter * 2] = u;
                mesh.texcoords[vCounter * 2 + 1] = v;
                // store vertex colors (default gray)
                mesh.colors[vCounter * 4] = 100;
                mesh.colors[vCounter * 4 + 1] = 100;
                mesh.colors[vCounter * 4 + 2] = 100;
                mesh.colors[vCounter * 4 + 3] = 255;
                vCounter++;
            };

            // first triangle of the quad
            PushVert(x1, y1, z1, n1.x, n1.y, n1.z, 0.0f, 0.0f);
            PushVert(x3, y3, z3, n1.x, n1.y, n1.z, 0.0f, 1.0f);
            PushVert(x2, y2, z2, n1.x, n1.y, n1.z, 1.0f, 0.0f);

            // second triangle of the quad (flat normal for now)
            const Vector3 n2 = {0.0f, 1.0f, 0.0f};
            PushVert(x2, y2, z2, n2.x, n2.y, n2.z, 1.0f, 0.0f);
            PushVert(x3, y3, z3, n2.x, n2.y, n2.z, 0.0f, 1.0f);
            PushVert(x4, y4, z4, n2.x, n2.y, n2.z, 1.0f, 1.0f);
        }
    }
    assert(vCounter == mesh.vertexCount);
    return mesh;
}
