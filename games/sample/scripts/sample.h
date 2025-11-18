#pragma once
#include "pond_interface.h"

// Forward declaration
class CubeWorldScene;

class SampleGame : public froggi::Game {
public:
    void onInit() override;
    void onUpdate(float deltaTime) override;
    void onShutdown() override;
    
private:
    CubeWorldScene* cubeworld = nullptr;
};
