#include <iostream>
#include "cubeworld.h"
#include "../components/cube_controller.h"
#include "../sample.h"

void CubeWorldScene::onLoad() {
    name = "cubeworld";
    std::cout << "Loading cubeworld..." << std::endl;
    
    // CREATE CUBE
    froggi::GameObject* cube = createGameObject("Cube");
    cube->position = glm::vec3(0.0f, 0.0f, 0.0f);
    
    // Visual mesh
    froggi::MeshComponent* cubeMesh = addComponent<froggi::MeshComponent>(cube);
    cubeMesh->meshName = "cube";
    cubeMesh->color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Cube controller
    CubeController* cubecontroller = addComponent<CubeController>(cube);
    (void)cubecontroller;
    
    // CREATE CAMERA
    froggi::GameObject* cameraObj = createGameObject("Main Camera");
    
    froggi::CameraComponent* camera = addComponent<froggi::CameraComponent>(cameraObj);
    camera->projectionType = froggi::CameraComponent::ProjectionType::Orthographic;
    camera->zoomSize = 1.2f;
    camera->orthoLeft = -13.333f * camera->zoomSize;
    camera->orthoRight = 13.333f * camera->zoomSize;
    camera->orthoTop = -7.5f * camera->zoomSize;
    camera->orthoBottom = 7.5f * camera->zoomSize;
    camera->nearClip = -150.0f;
    camera->farClip = 100.0f;
    
    std::cout << "Cubeworld loaded with " << gameObjects.size() << " objects!" << std::endl;
}
