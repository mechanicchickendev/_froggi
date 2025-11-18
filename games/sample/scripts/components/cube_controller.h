#pragma once
#include "pond_interface.h"

class CubeController : public froggi::Component {
public:
    void onUpdate(float deltaTime) override;
};
