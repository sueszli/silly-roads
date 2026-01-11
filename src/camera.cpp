#include "camera.hpp"
#include "raymath.h"
#include "terrain.hpp"
#include <cmath>

namespace CameraSystem {

void update(Components::CameraState &cam, const Components::Car &car, float dt) {
    // update camera to follow car (chase cam)
    Vector3 target_cam_pos;
    target_cam_pos.x = car.pos.x - std::sin(car.heading) * 15.0f;
    target_cam_pos.z = car.pos.z - std::cos(car.heading) * 15.0f;
    target_cam_pos.y = car.pos.y + 8.0f;

    // smooth follow
    cam.camera.position = Vector3Lerp(cam.camera.position, target_cam_pos, dt * 3.0f);

    // check terrain collision for camera
    float cam_terrain_h = Terrain::get_height(cam.camera.position.x, cam.camera.position.z);
    if (cam.camera.position.y < cam_terrain_h + 2.0f) {
        cam.camera.position.y = cam_terrain_h + 2.0f;
    }
    cam.camera.target = car.pos;
}

} // namespace CameraSystem
