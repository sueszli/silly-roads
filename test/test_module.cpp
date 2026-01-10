#include "../src/road.hpp"
#include <gtest/gtest.h>
#include <vector>

TEST(RoadTest, GenerateRoadPath) {
    std::vector<Vector3> control_points = {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 10.0f}, {20.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 10.0f}};

    std::vector<Vector3> path = generate_road_path(control_points);

    // Expected samples: (4 - 1) * 40 + 1 = 121
    EXPECT_EQ(path.size(), 121);

    // Check first and last points match control points approximately (spline passes through them?)
    // Note: Catmull-Rom passes through control points.
    // However, the implementation handles start/end tangents by duplicating points or special indexing.
    // Let's just check size for now to verify decoupling.
    EXPECT_FALSE(path.empty());
}
