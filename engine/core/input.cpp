namespace froggi {
    bool Input::isKeyDown(int keycode) {
        return glfwGetKey(window, keycode) == GLFW_PRESS;
    }
    
    bool Input::isKeyPressed(int keycode) {
        // Track previous frame state
        bool current = isKeyDown(keycode);
        bool previous = prevKeyState[keycode];
        prevKeyState[keycode] = current;
        return current && !previous;
    }
    
    glm::vec2 Input::getMovementInput() {
        glm::vec2 input(0.0f);
        if (isKeyDown(GLFW_KEY_W)) input.y -= 1.0f;
        if (isKeyDown(GLFW_KEY_S)) input.y += 1.0f;
        if (isKeyDown(GLFW_KEY_A)) input.x -= 1.0f;
        if (isKeyDown(GLFW_KEY_D)) input.x += 1.0f;
        return input;
    }
}
