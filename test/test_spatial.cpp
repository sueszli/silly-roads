#include "../src/road.hpp"
#include <gtest/gtest.h>

TEST(SpatialTest, EmptyPath) {
    std::vector<Vector3> path;
    auto segments = get_road_segments_in_bounds(path, 0, 0, 10, 10, 1.0f);
    EXPECT_TRUE(segments.empty());
}

TEST(SpatialTest, SegmentInside) {
    std::vector<Vector3> path = {{5.0f, 0.0f, 5.0f}, {6.0f, 0.0f, 6.0f}};
    auto segments = get_road_segments_in_bounds(path, 0, 0, 10, 10, 1.0f);
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].first.x, 5.0f);
    EXPECT_EQ(segments[0].second.x, 6.0f);
}

TEST(SpatialTest, SegmentOutside) {
    std::vector<Vector3> path = {{20.0f, 0.0f, 20.0f}, {21.0f, 0.0f, 21.0f}};
    auto segments = get_road_segments_in_bounds(path, 0, 0, 10, 10, 1.0f);
    EXPECT_TRUE(segments.empty());
}

TEST(SpatialTest, SegmentCrossing) {
    std::vector<Vector3> path = {{5.0f, 0.0f, 5.0f}, {15.0f, 0.0f, 15.0f}};
    auto segments = get_road_segments_in_bounds(path, 0, 0, 10, 10, 0.0f);
    EXPECT_EQ(segments.size(), 1);
}

TEST(SpatialTest, SegmentNearMargin) {
    std::vector<Vector3> path = {{11.0f, 0.0f, 5.0f}, {11.0f, 0.0f, 6.0f}};

    auto segments = get_road_segments_in_bounds(path, 0, 0, 10, 10, 0.5f);
    EXPECT_TRUE(segments.empty());

    segments = get_road_segments_in_bounds(path, 0, 0, 10, 10, 2.0f);
    EXPECT_EQ(segments.size(), 1);
}
