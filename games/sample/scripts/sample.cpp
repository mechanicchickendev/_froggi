#include <iostream>
#include "sample.h"
#include "scenes/cubeworld.h"
  
void SampleGame::onInit() {
  
    std::cout << "\033[1;32m";
    std::cout << "       __    powered by      _ " << std::endl;
    std::cout << "      / _|_ _ ___  __ _ __ _(_)" << std::endl;
    std::cout << " ___ |  _| '_/ _ \\/ _` / _` | |" << std::endl;
    std::cout << "|___||_| |_| \\___/\\__, \\__, |_|" << std::endl;
    std::cout << "                  |___/|___/   " << std::endl;
    std::cout << "\033[0m" << std::endl;
    
    // LOAD MODEL
    std::cout << "Loading game resources..." << std::endl;
    loadModel("cube", "assets/models/cube.obj");

    // LOAD SCENE
    cubeworld = new CubeWorldScene();
    cubeworld->sampleGame = this;
    loadScene(cubeworld);

    // Set main camera
    froggi::GameObject* cameraObj = cubeworld->findGameObject("Main Camera");
    if (cameraObj) {
        froggi::CameraComponent* camera = cameraObj->getComponent<froggi::CameraComponent>();
        setMainCamera(camera);
    }
    
    std::cout << "Sample game initialized!\n" << std::endl;
}

void SampleGame::onUpdate(float deltaTime) {
    (void)deltaTime;
}

void SampleGame::onShutdown() {
    std::cout << "\nShutting down sample..." << std::endl;
    
    if (currentScene) {
        currentScene->onUnload();
        delete currentScene;
        currentScene = nullptr;
    }
    
    std::cout << "Sample shutdown complete." << std::endl;
}
