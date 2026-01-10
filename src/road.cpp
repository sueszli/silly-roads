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

Mesh generate_road_mesh(const std::vector<Vector3> &points) {
    Mesh mesh = {};
    if (points.size() < 2) {
        return mesh;
    }

    const float road_width = 8.0f;
    std::size_t total_samples = points.size();

    // 3 vertices per sample (left, center, right)
    mesh.vertexCount = (int)total_samples * 3;

    // 4 triangles per segment (2 per strip)
    mesh.triangleCount = ((int)total_samples - 1) * 4;
    mesh.vertices = static_cast<float *>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.normals = static_cast<float *>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount) * 3 * sizeof(float)));
    mesh.texcoords = static_cast<float *>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount) * 2 * sizeof(float)));
    mesh.indices = static_cast<unsigned short *>(MemAlloc(static_cast<unsigned int>(mesh.triangleCount) * 3 * sizeof(unsigned short)));

    const int samples_per_segment = 40; // Kept for texture coord calculation, though passed-in points size matters more now.
                                        // Ideally texture calc should depend on arc length or similar, but keeping logic consistent.

    for (int i = 0; i < (int)total_samples; i++) {
        Vector3 p = points[static_cast<std::size_t>(i)];
        Vector3 tangent;
        if (i < (int)total_samples - 1) {
            tangent = Vector3Subtract(points[static_cast<std::size_t>(i + 1)], p);
        } else {
            tangent = Vector3Subtract(p, points[static_cast<std::size_t>(i - 1)]);
        }

        // horizontal right direction perpendicular to tangent
        Vector3 right_dir = {-tangent.z, 0.0f, tangent.x};
        if (Vector3Length(right_dir) < 0.0001f) {
            right_dir = {1.0f, 0.0f, 0.0f};
        } else {
            right_dir = Vector3Normalize(right_dir);
        }

        Vector3 left_pos = Vector3Subtract(p, Vector3Scale(right_dir, road_width * 0.5f));
        Vector3 mid_pos = p;
        Vector3 right_pos = Vector3Add(p, Vector3Scale(right_dir, road_width * 0.5f));

        // project onto terrain with small offset to avoid z-fighting
        left_pos.y = get_terrain_height(left_pos.x, left_pos.z) + 0.3f;
        mid_pos.y = get_terrain_height(mid_pos.x, mid_pos.z) + 0.3f;
        right_pos.y = get_terrain_height(right_pos.x, right_pos.z) + 0.3f;

        // vertices: left (0), mid (1), right (2)
        int v_idx = i * 3;
        mesh.vertices[v_idx * 3 + 0] = left_pos.x;
        mesh.vertices[v_idx * 3 + 1] = left_pos.y;
        mesh.vertices[v_idx * 3 + 2] = left_pos.z;

        mesh.vertices[(v_idx + 1) * 3 + 0] = mid_pos.x;
        mesh.vertices[(v_idx + 1) * 3 + 1] = mid_pos.y;
        mesh.vertices[(v_idx + 1) * 3 + 2] = mid_pos.z;

        mesh.vertices[(v_idx + 2) * 3 + 0] = right_pos.x;
        mesh.vertices[(v_idx + 2) * 3 + 1] = right_pos.y;
        mesh.vertices[(v_idx + 2) * 3 + 2] = right_pos.z;

        // normals (upward for simplicity)
        for (int j = 0; j < 3; j++) {
            mesh.normals[(v_idx + j) * 3 + 0] = 0.0f;
            mesh.normals[(v_idx + j) * 3 + 1] = 1.0f;
            mesh.normals[(v_idx + j) * 3 + 2] = 0.0f;
        }

        // texcoords
        float v = (float)i / (float)samples_per_segment;
        mesh.texcoords[v_idx * 2 + 0] = 0.0f;
        mesh.texcoords[v_idx * 2 + 1] = v;
        mesh.texcoords[(v_idx + 1) * 2 + 0] = 0.5f;
        mesh.texcoords[(v_idx + 1) * 2 + 1] = v;
        mesh.texcoords[(v_idx + 2) * 2 + 0] = 1.0f;
        mesh.texcoords[(v_idx + 2) * 2 + 1] = v;
    }

    for (int i = 0; i < (int)total_samples - 1; i++) {
        // vertices for row i: (i*3, i*3+1, i*3+2)
        // vertices for row i+1: ((i+1)*3, (i+1)*3+1, (i+1)*3+2)
        int cur = i * 3;
        int next = (i + 1) * 3;

        // strip 1: left to mid
        // triangle 1
        mesh.indices[i * 12 + 0] = (unsigned short)(cur + 0);
        mesh.indices[i * 12 + 1] = (unsigned short)(cur + 1);
        mesh.indices[i * 12 + 2] = (unsigned short)(next + 0);
        // triangle 2
        mesh.indices[i * 12 + 3] = (unsigned short)(cur + 1);
        mesh.indices[i * 12 + 4] = (unsigned short)(next + 1);
        mesh.indices[i * 12 + 5] = (unsigned short)(next + 0);

        // strip 2: mid to right
        // triangle 3
        mesh.indices[i * 12 + 6] = (unsigned short)(cur + 1);
        mesh.indices[i * 12 + 7] = (unsigned short)(cur + 2);
        mesh.indices[i * 12 + 8] = (unsigned short)(next + 1);
        // triangle 4
        mesh.indices[i * 12 + 9] = (unsigned short)(cur + 2);
        mesh.indices[i * 12 + 10] = (unsigned short)(next + 2);
        mesh.indices[i * 12 + 11] = (unsigned short)(next + 1);
    }

    return mesh;
}

std::vector<Vector3> generate_road_path(const std::vector<Vector3> &control_points) {
    if (control_points.size() < 2) {
        return {};
    }

    const int samples_per_segment = 40;
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

        center_points[static_cast<std::size_t>(i)] = catmull_rom(control_points[i0], control_points[i1], control_points[i2], control_points[i3], local_t);
    }

    return center_points;
}
