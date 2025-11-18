#include "cube_controller.h"
#include <cmath>

void CubeController::onUpdate(float deltaTime) {
    if (!owner) return;
    
    // Example: Rotate the cube
    owner->rotation.y += deltaTime;
    owner->rotation.x += deltaTime * 2.5f;
}
