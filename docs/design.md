# Raycer Design Specification

## Project Overview

**Goal**: Develop a car driving simulator with a pickup truck on procedurally generated mountainous terrain with roads.  
**Current State**: A red box (car) with arcade physics, chase camera, and collectible objectives.  
**Next Milestone**: Replace box with 4-wheeled pickup truck model, add procedural road, and improve ground snapping.

## Core Constraints

1. **Tiger Style Compliance**: Follow `CONTRIBUTING.md` strictly (Data-Oriented, `_mut` suffix, aggressive asserts)
2. **LLM Observability**: Verification via structured text logs to stdout
3. **Atomic Stages**: Each stage does ONE thing. If unsure, make it smaller.

---

## Log Format (LLM's Eyes)

```
[FRAME <n>] CAR_POS:<x> <y> <z> | SPEED:<f> | HEADING:<rad> | GROUND:<0|1> | WHEEL_STEER:<rad>
```

---

## Implementation Stages

### Stage 1: Four-Wheel Ground Snapping

**What**: Calculate car height from 4 wheel contact points instead of center.  
**Why**: Prevents car body clipping into ground on slopes.  
**Size**: ~40 lines

**Technical Approach**:
- Define 4 wheel offsets in local car space: front-left, front-right, rear-left, rear-right
- Each frame, calculate world positions of wheel contact points using `car_pos` + rotated offset
- Sample `get_terrain_height()` at each wheel position
- Set `car_pos.y` = average of 4 heights + body clearance (e.g., 0.5 units)
- Calculate car pitch/roll from height differences (optional for later)

**State Changes**:
```cpp
// Add to GameState
float wheel_heights[4] = {0}; // FL, FR, RL, RR terrain heights
```

**Prompt**:
> Fix car ground clipping: instead of sampling terrain height at car center, sample at 4 wheel positions (offsets: ±1.0 X, ±1.5 Z from center). Average the 4 heights and add 0.5 clearance for `car_pos.y`. Store wheel heights in `GameState::wheel_heights[4]` for later use. Keep airtime logic: only snap when close to ground.

**Verification**: Drive over bumpy terrain. Car body should stay above ground. Log `GROUND:1` when all 4 wheels touch.

---

### Stage 2: Add Wheel Structs

**What**: Add data for 4 wheels to `GameState`.  
**Why**: Foundation for wheel rendering and steering visuals.  
**Size**: ~20 lines

**State Changes** (in `game_state.hpp`):
```cpp
struct WheelState {
    Vector3 local_offset;  // position relative to car body
    float steering_angle;  // only non-zero for front wheels
    float spin_angle;      // rotation from rolling
};

// Add to GameState
WheelState wheels[4] = {
    {{-1.0f, -0.3f, 1.5f}, 0.0f, 0.0f},   // front-left
    {{ 1.0f, -0.3f, 1.5f}, 0.0f, 0.0f},   // front-right
    {{-1.0f, -0.3f, -1.5f}, 0.0f, 0.0f},  // rear-left
    {{ 1.0f, -0.3f, -1.5f}, 0.0f, 0.0f},  // rear-right
};
```

**Prompt**:
> Add `WheelState` struct with `local_offset`, `steering_angle`, and `spin_angle`. Add `wheels[4]` array to `GameState` with offsets for a 2x4 unit car body. Front wheels at Z=+1.5, rear at Z=-1.5. X offset ±1.0. Y offset -0.3 (below body).

**Verification**: Code compiles. No visual changes yet.

---

### Stage 3: Render Wheels as Cylinders

**What**: Draw 4 cylinders at wheel positions.  
**Why**: Visual representation of wheels.  
**Size**: ~30 lines

**Requirements**:
1. For each wheel, calculate world position: `car_pos` + (wheel offset rotated by `car_heading`)
2. Draw horizontal cylinder (wheel) at each position
3. Cylinder size: radius 0.3, height 0.2
4. Use `rlPushMatrix`, `rlRotatef` to orient wheel

**Prompt**:
> Render 4 wheels as dark gray cylinders (radius 0.3, width 0.2). For each wheel: compute world position from `car_pos` + rotated offset, apply `car_heading` rotation, then draw cylinder oriented along X-axis. Use `DrawCylinder` with appropriate transforms.

**Verification**: Run game. 4 gray cylinders should appear at wheel positions, rotating with car.

---

### Stage 4: Wheel Steering Animation

**What**: Front wheels visually turn when A/D pressed.  
**Why**: Shows steering input to player.  
**Size**: ~15 lines

**Requirements**:
1. When A/D pressed, update `wheels[0].steering_angle` and `wheels[1].steering_angle`
2. Max steering angle: ±30° (0.52 rad)
3. Smooth interpolation toward target angle
4. Apply steering rotation to front wheel cylinders when drawing

**Prompt**:
> Animate front wheel steering: when A pressed, target `steering_angle` = +0.52 rad; D = -0.52 rad; neither = 0. Lerp current toward target at rate 8.0/s. Apply this rotation around Y-axis when drawing front wheels. Rear wheels stay at 0. Log `WHEEL_STEER:<rad>`.

**Verification**: Run game. Press A/D - front wheels should visibly turn. Log shows steering angle.

---

### Stage 5: Wheel Spin Animation -> SKIPPED

**What**: Wheels spin based on car speed.  
**Why**: Looks more realistic.  
**Size**: ~10 lines

**Requirements**:
1. Update `spin_angle` based on `car_speed * dt / wheel_radius`
2. Apply spin rotation around X-axis when drawing wheel

**Prompt**:
> Spin wheels based on speed: `spin_angle += car_speed * dt / 0.3f` (0.3 = wheel radius). Apply rotation around local X-axis when rendering. Keep angle bounded using modulo or wrapping.

**Verification**: Run game. Wheels should spin forward when moving forward, backward when reversing.

---

### Stage 6: Pickup Truck Body Shape

**What**: Replace single box with multi-box truck shape.  
**Why**: Looks like a vehicle instead of a brick.  
**Size**: ~25 lines

**Truck Model** (all local coordinates):
- **Cabin**: Box at (0, 0.5, 0.8), size (1.8, 1.0, 1.5) - BLUE
- **Bed**: Box at (0, 0.2, -1.2), size (1.8, 0.5, 2.0) - DARKBLUE
- **Hood**: Box at (0, 0.2, 1.8), size (1.6, 0.4, 0.8) - BLUE
- Remove old single red box

**Prompt**:
> Replace single red box with pickup truck shape. Draw 3 boxes (cabin, bed, hood) with offsets listed above. Use BLUE for cabin/hood, DARKBLUE for bed. Keep same rotation transform. Add wire outlines for definition.

**Verification**: Run game. Should see blocky pickup truck shape. Wheels should still be attached correctly.

---

### Stage 7: Road Data Structure

**What**: Add data for procedural road spline.  
**Why**: Foundation for rendering road on terrain.  
**Size**: ~30 lines

**Concept**: Road is a list of control points. Use Catmull-Rom spline for smooth curves.

**State Changes** (in `game_state.hpp`):
```cpp
constexpr std::int32_t ROAD_POINTS = 64;

// Add to GameState
Vector3 road_points[ROAD_POINTS] = {};  // control points
bool road_initialized = false;
```

**Prompt**:
> Add road data structure: `road_points[64]` array in `GameState`, `road_initialized` flag. Create `init_road_mut(GameState*)` function that generates 64 control points in a winding path. Start from car position, extend in a loop pattern.

**Verification**: Code compiles. Call `init_road_mut` at startup.

---

### Stage 8: Generate Road Points

**What**: Procedurally generate road control points.  
**Why**: Creates an interesting path to drive on.  
**Size**: ~40 lines

**Algorithm**:
1. Start at origin
2. For each point: advance by fixed step, vary direction using noise
3. Sample terrain height for Y coordinate
4. Create a rough loop by curving back toward origin at end

**Prompt**:
> Implement `init_road_mut`: generate 64 road points starting at (0, 0, 0). Use angle-based walking: `angle += noise(i) * 0.3`. Step size 20 units. Set Y from `get_terrain_height()`. Final points should curve back toward start to create a loop. Log `[ROAD] Generated N points`.

**Verification**: Log shows road generation message. Points should form a rough loop.

---

### Stage 9: Render Road Mesh

**What**: Draw the road as a gray strip projected onto terrain.  
**Why**: Visual road to follow.  
**Size**: ~60 lines

**Approach**:
1. Create a new header/source: `src/road.hpp`, `src/road.cpp`
2. Interpolate between control points using Catmull-Rom
3. At each sample, calculate road center + left/right edges (width 8 units)
4. Project edge points onto terrain
5. Build triangle strip mesh, upload to GPU
6. Draw as gray material

**Prompt**:
> Create `road.hpp/cpp`. Implement `generate_road_mesh(const Vector3* points, std::int32_t count)` that returns a `Mesh`. Interpolate points using Catmull-Rom. Road width 8 units. Sample terrain height for each vertex. Return mesh ready for GPU upload. Draw in gray (DARKGRAY).

**Verification**: Run game. Gray road should appear on terrain, following the generated curve.

---

### Stage 10: Road Follows Player

**What**: Regenerate road around player as they move.  
**Why**: Infinite procedural road.  
**Size**: ~30 lines

**Approach**:
1. Track player progress along road
2. When player passes midpoint, shift road points
3. Generate new points ahead, discard old points behind
4. Regenerate mesh

**Prompt**:
> Track which road segment player is nearest. When player passes point 32, shift array: move points 32-63 to 0-31, generate new points at 32-63. Regenerate road mesh. Log `[ROAD] Extended`.

**Verification**: Drive along road. Road should extend ahead infinitely. Log shows extension messages.

---

## Summary Table

| Stage | Name | Lines | Key Files |
|-------|------|-------|-----------|
| 1 | Four-Wheel Ground Snap | ~40 | main.cpp, game_state.hpp |
| 2 | Wheel Structs | ~20 | game_state.hpp |
| 3 | Render Wheels | ~30 | main.cpp |
| 4 | Wheel Steering Animation | ~15 | main.cpp |
| 5 | Wheel Spin Animation | ~10 | main.cpp |
| 6 | Pickup Truck Body | ~25 | main.cpp |
| 7 | Road Data Structure | ~30 | game_state.hpp |
| 8 | Generate Road Points | ~40 | main.cpp or road.cpp |
| 9 | Render Road Mesh | ~60 | road.hpp, road.cpp |
| 10 | Road Follows Player | ~30 | main.cpp, road.cpp |

**Total**: 10 stages, ~300 lines of incremental changes

---

## Suggested Implementation Order

**Phase A (Truck Visuals)**: Stages 1 → 2 → 3 → 4 → 5 → 6  
**Phase B (Ground Physics)**: Stage 7  
**Phase C (Road System)**: Stages 8 → 9 → 10 → 11

Start with Phase A for quick visual payoff, then Phase B for physics fix, then Phase C for road.

---

## Notes for LLM Developers

1. **One stage at a time**: Complete verification before proceeding
2. **Read the log**: stdout is your primary debugging tool
3. **Compile often**: `make lint && make run` after each change
4. **Remove collectibles**: After Stage 16, consider removing the yellow cylinder objective in favor of road-following
5. **Rollback on failure**: If a stage breaks, revert and retry with smaller steps
