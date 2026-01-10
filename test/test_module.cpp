#include "../src/road.hpp"
#include <gtest/gtest.h>
#include <vector>

TEST(RoadTest, GenerateRoadPath) {
    std::vector<Vector3> control_points = {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 10.0f}, {20.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 10.0f}};

    std::vector<Vector3> path = generate_road_path(control_points);

    EXPECT_EQ(path.size(), 121);
    EXPECT_FALSE(path.empty());
}
