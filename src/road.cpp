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
