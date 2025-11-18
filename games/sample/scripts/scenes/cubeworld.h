#pragma once
#include "pond_interface.h"

// Forward declaration
class SampleGame;

class CubeWorldScene : public froggi::Scene {
public:
    void onLoad() override;
    
    // Reference to game (to access resources)
    SampleGame* sampleGame = nullptr;  
};
