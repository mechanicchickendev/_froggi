#pragma once

#include <GLFW/glfw3.h>
#include "3d_renderer.h"

namespace InputHandler
{

void key_callback(GLFWwindow* window, int keycode, int scancode, int action, int mods);

void key_delay();
	static int tilePosX;
	static int tilePosY;
	
	//static float figPosX;
	//static float figPosY;
	static float figRotZ;
	glm::vec3 direction(0.0f);
	glm::vec3 figPos(0.0f);
	glm::vec3 camPos(0.0f);
	
    bool moveX = false;
    bool moveY = false;
    
    float moveDelay = 0.0f;
    float moveSpeed = 0.0f;
    bool moveUp = false;
    bool moveDown = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool jump = false;
    bool sprint = false;
    bool attack = false;
    
    
    bool isMove = false;
    void updatePlayer();
}

void joystick_callback(GLFWwindow* window, int keycode, int scancode, int action, int mods);


