#include "road.hpp"
#include "raymath.h"
#include "terrain.hpp"
#include <cmath>

namespace {

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

Mesh generate_road_mesh(const Vector3 *points, std::int32_t count) {
    Mesh mesh = {};
    if (count < 2) {
        return mesh;
    }

    const int samples_per_segment = 8;
    const int total_samples = (count - 1) * samples_per_segment + 1;
    const float road_width = 8.0f;

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

        center_points[static_cast<std::size_t>(i)] = catmull_rom(points[i0], points[i1], points[i2], points[i3], local_t);
    }

    mesh.vertexCount = total_samples * 2;
    mesh.triangleCount = (total_samples - 1) * 2;
    mesh.vertices = static_cast<float *>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.normals = static_cast<float *>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.texcoords = static_cast<float *>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount) * 2 * sizeof(float)));
    mesh.indices = static_cast<unsigned short *>(MemAlloc(static_cast<unsigned int>(mesh.triangleCount) * 3 * sizeof(unsigned short)));

    for (int i = 0; i < total_samples; i++) {
        Vector3 p = center_points[static_cast<std::size_t>(i)];
        Vector3 tangent;
        if (i < total_samples - 1) {
            tangent = Vector3Subtract(center_points[static_cast<std::size_t>(i + 1)], p);
        } else {
            tangent = Vector3Subtract(p, center_points[static_cast<std::size_t>(i - 1)]);
        }

        // horizontal right direction perpendicular to tangent
        Vector3 right_dir = {-tangent.z, 0.0f, tangent.x};
        if (Vector3Length(right_dir) < 0.0001f) {
            right_dir = {1.0f, 0.0f, 0.0f};
        } else {
            right_dir = Vector3Normalize(right_dir);
        }

        Vector3 left_pos = Vector3Subtract(p, Vector3Scale(right_dir, road_width * 0.5f));
        Vector3 right_pos = Vector3Add(p, Vector3Scale(right_dir, road_width * 0.5f));

        // project onto terrain with small offset to avoid z-fighting
        left_pos.y = get_terrain_height(left_pos.x, left_pos.z) + 0.1f;
        right_pos.y = get_terrain_height(right_pos.x, right_pos.z) + 0.1f;

        // left vertex
        mesh.vertices[i * 6 + 0] = left_pos.x;
        mesh.vertices[i * 6 + 1] = left_pos.y;
        mesh.vertices[i * 6 + 2] = left_pos.z;
        // right vertex
        mesh.vertices[i * 6 + 3] = right_pos.x;
        mesh.vertices[i * 6 + 4] = right_pos.y;
        mesh.vertices[i * 6 + 5] = right_pos.z;

        // normals (upward for simplicity)
        mesh.normals[i * 6 + 0] = 0.0f;
        mesh.normals[i * 6 + 1] = 1.0f;
        mesh.normals[i * 6 + 2] = 0.0f;
        mesh.normals[i * 6 + 3] = 0.0f;
        mesh.normals[i * 6 + 4] = 1.0f;
        mesh.normals[i * 6 + 5] = 0.0f;

        // texcoords
        mesh.texcoords[i * 4 + 0] = 0.0f;
        mesh.texcoords[i * 4 + 1] = (float)i / (float)samples_per_segment;
        mesh.texcoords[i * 4 + 2] = 1.0f;
        mesh.texcoords[i * 4 + 3] = (float)i / (float)samples_per_segment;
    }

    for (int i = 0; i < total_samples - 1; i++) {
        // segment i: vertices (i*2, i*2+1) to (i*2+2, i*2+3)
        // triangle 1
        mesh.indices[i * 6 + 0] = (unsigned short)(i * 2 + 0);
        mesh.indices[i * 6 + 1] = (unsigned short)(i * 2 + 1);
        mesh.indices[i * 6 + 2] = (unsigned short)(i * 2 + 2);
        // triangle 2
        mesh.indices[i * 6 + 3] = (unsigned short)(i * 2 + 1);
        mesh.indices[i * 6 + 4] = (unsigned short)(i * 2 + 3);
        mesh.indices[i * 6 + 5] = (unsigned short)(i * 2 + 2);
    }

    return mesh;
}
