# Task 2: Spatial Optimization for Road Queries

## Goal
Enable efficient querying of road geometry during terrain generation. We need to identify which parts of the road are relevant to a specific chunk of terrain to avoid performance bottlenecks. The terrain generation loops over ~65,000 vertices; checking every road segment for every vertex would be too slow.

## Current State
*   We have (from Task 1) a `std::vector<Vector3>` representing the dense road center points.
*   Terrain is generated in `src/terrain.cpp` within a loop that covers a specific area defined by `offset_x` and `offset_z`.

## Required Changes
1.  **Implement Spatial Query Helper**:
    *   Create a helper function (e.g., in `road.hpp` or a new `road_query.hpp`) that takes the road path and a bounding box (min_x, min_z, max_x, max_z).
    *   It should return a list of "road segments" (pairs of points) that are within or near this bounding box.
    *   Signature idea: `std::vector<std::pair<Vector3, Vector3>> get_road_segments_in_bounds(const std::vector<Vector3>& path, float min_x, float min_z, float max_x, float max_z, float margin)`.
2.  **Logic**:
    *   Iterate through the dense path.
    *   For each segment (point `i` to `i+1`), check if its bounding box overlaps the query box (inflated by a margin for road width).
    *   Return the filtered list.

## Prompt
To execute this task, use the following prompt:

> `Please read 'specs/02_spatial_opt.md' and implement the spatial query helper for efficient road segment retrieval.`
