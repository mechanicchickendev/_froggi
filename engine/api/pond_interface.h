#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <GLFW/glfw3.h>

namespace froggi {

// Forward declarations
class Scene;
class GameObject;
class Component;
class Renderer;
class CollisionSystem;
class Collider;
class Rigidbody;

///////////////////////////////////////////////////////////////////////////////
// Component Base Class

class Component {
public:
    virtual ~Component() = default;
    
    virtual void onInit() {}
    virtual void onUpdate(float deltaTime) { (void)deltaTime; }
    virtual void onFixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
    virtual void onDestroy() {}
    
    GameObject* owner = nullptr;
    bool enabled = true;
};

///////////////////////////////////////////////////////////////////////////////
// GameObject - Entity in the scene

class GameObject {
public:
    GameObject(const std::string& n = "GameObject") : name(n) {}
    ~GameObject() = default;
    
    // Transform
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    
    // Hierarchy
    GameObject* parent = nullptr;
    std::vector<GameObject*> children;
    
    // Components
    std::vector<Component*> components;
    
    // Identity
    std::string name;
    bool active = true;
    
    // Get world transform matrix
    glm::mat4 getWorldTransform() const {
        glm::mat4 transform = getLocalTransform();
        if (parent) {
            transform = parent->getWorldTransform() * transform;
        }
        return transform;
    }
    
    // Get local transform matrix
    glm::mat4 getLocalTransform() const {
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, position);
        transform = glm::rotate(transform, rotation.z, glm::vec3(0, 0, 1));
        transform = glm::rotate(transform, rotation.y, glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, rotation.x, glm::vec3(1, 0, 0));
        transform = glm::scale(transform, scale);
        return transform;
    }
    
    // Get component by type
    template<typename T>
    T* getComponent() {
        for (auto* comp : components) {
            T* result = dynamic_cast<T*>(comp);
            if (result) return result;
        }
        return nullptr;
    }
};

///////////////////////////////////////////////////////////////////////////////
// MeshComponent - Makes GameObject visible

class MeshComponent : public Component {
public:
    std::string meshName;
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    void setMesh(const std::string& name) {
        meshName = name;
    }
};

///////////////////////////////////////////////////////////////////////////////
// CameraComponent - Defines view/projection

class CameraComponent : public Component {
public:
    enum class ProjectionType {
        Orthographic,
        Perspective
    };
    
    ProjectionType projectionType = ProjectionType::Orthographic;
    
    // Orthographic settings
    float orthoLeft = -13.333f;
    float orthoRight = 13.333f;
    float orthoTop = -7.5f;
    float orthoBottom = 7.5f;
    float zoomSize = 1.2f;
    
    // Common settings
    float nearClip = -150.0f;
    float farClip = 100.0f;
    
    glm::mat4 getProjectionMatrix(float aspect) const {
        if (projectionType == ProjectionType::Orthographic) {
            (void)aspect;
            return glm::ortho(orthoLeft, orthoRight, 
                            orthoBottom, orthoTop,
                            nearClip, farClip);
        } else {
            return glm::mat4(1.0f);
        }
    }
    
    glm::mat4 getViewMatrix() const {
        if (!owner) return glm::mat4(1.0f);
        
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, glm::vec3(0, 0, -5.0f));
        view = glm::rotate(view, owner->rotation.x, glm::vec3(1, 0, 0));
        view = glm::rotate(view, owner->rotation.z, glm::vec3(0, 0, 1));
        view = glm::translate(view, -owner->position);
        
        return view;
    }
};

///////////////////////////////////////////////////////////////////////////////
// Scene - Container for GameObjects

class Scene {
public:
    virtual ~Scene() {
        // Clean up components
        for (auto* comp : components) {
            comp->onDestroy();
            delete comp;
        }
        // Clean up game objects
        for (auto* obj : gameObjects) {
            delete obj;
        }
    }
    
    // Lifecycle
    virtual void onLoad() {}
    virtual void onUnload() {}
    
    // GameObject management
    GameObject* createGameObject(const std::string& name = "GameObject") {
        GameObject* obj = new GameObject(name);
        gameObjects.push_back(obj);
        return obj;
    }
    
    void destroyGameObject(GameObject* obj) {
        auto it = std::find(gameObjects.begin(), gameObjects.end(), obj);
        if (it != gameObjects.end()) {
            gameObjects.erase(it);
            delete obj;
        }
    }
    
    GameObject* findGameObject(const std::string& name) {
        for (auto* obj : gameObjects) {
            if (obj->name == name) return obj;
        }
        return nullptr;
    }
    
    // Component management
    template<typename T>
    T* addComponent(GameObject* obj) {
        static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
        T* component = new T();
        component->owner = obj;
        obj->components.push_back(component);
        components.push_back(component);
        component->onInit();
        return component;
    }
    
    std::vector<GameObject*> gameObjects;
    std::vector<Component*> components;
    std::string name = "Untitled Scene";
    CollisionSystem* collisionSystem = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Input System

class Input {
public:
    static void init(GLFWwindow* window) {
        s_window = window;
    }
    
    static void update() {
        s_prevKeyState = s_currentKeyState;
    }
    
    static void shutdown() {
        s_window = nullptr;
    }
    
    // Keyboard
    static bool isKeyDown(int keycode) {
        if (!s_window) return false;
        bool state = glfwGetKey(s_window, keycode) == GLFW_PRESS;
        s_currentKeyState[keycode] = state;
        return state;
    }
    
    static bool isKeyPressed(int keycode) {
        bool current = isKeyDown(keycode);
        bool previous = s_prevKeyState[keycode];
        return current && !previous;
    }
    
    static bool isKeyReleased(int keycode) {
        bool current = isKeyDown(keycode);
        bool previous = s_prevKeyState[keycode];
        return !current && previous;
    }
    
    // Mouse
    static glm::vec2 getMousePosition() {
        if (!s_window) return glm::vec2(0.0f);
        double x, y;
        glfwGetCursorPos(s_window, &x, &y);
        return glm::vec2(static_cast<float>(x), static_cast<float>(y));
    }
    
    static bool isMouseButtonDown(int button) {
        if (!s_window) return false;
        return glfwGetMouseButton(s_window, button) == GLFW_PRESS;
    }
    
    // Gamepad
    static bool isGamepadConnected(int gamepad = 0) {
        return glfwJoystickIsGamepad(gamepad);
    }
    
    static float getGamepadAxis(int axis, int gamepad = 0) {
        int count;
        const float* axes = glfwGetJoystickAxes(gamepad, &count);
        if (axes && axis < count) {
            return axes[axis];
        }
        return 0.0f;
    }
    
    // Helper for WASD movement
    static glm::vec2 getMovementInput() {
        glm::vec2 input(0.0f);
        if (isKeyDown(GLFW_KEY_W)) input.y -= 1.0f;
        if (isKeyDown(GLFW_KEY_S)) input.y += 1.0f;
        if (isKeyDown(GLFW_KEY_A)) input.x -= 1.0f;
        if (isKeyDown(GLFW_KEY_D)) input.x += 1.0f;
        return input;
    }
    
private:
    static GLFWwindow* s_window;
    static std::unordered_map<int, bool> s_currentKeyState;
    static std::unordered_map<int, bool> s_prevKeyState;
};

inline GLFWwindow* Input::s_window = nullptr;
inline std::unordered_map<int, bool> Input::s_currentKeyState;
inline std::unordered_map<int, bool> Input::s_prevKeyState;

///////////////////////////////////////////////////////////////////////////////
// Game Base Class

class Game {
public:
    virtual ~Game() = default;
    
    virtual void onInit() = 0;
    virtual void onUpdate(float deltaTime) = 0;
    virtual void onShutdown() = 0;
    virtual void onRenderUI() {}
    
    Scene* getCurrentScene() { return currentScene; }
    
    void setMainCamera(CameraComponent* camera) {
        mainCamera = camera;
    }
    
    CameraComponent* getMainCamera() { return mainCamera; }
    
    // Move loadScene to .cpp to avoid incomplete type issues
    void loadScene(Scene* scene);
    
    void loadModel(const std::string& name, const std::string& path);
    
protected:
    Scene* currentScene = nullptr;
    CameraComponent* mainCamera = nullptr;
    
    friend class Engine;
};

///////////////////////////////////////////////////////////////////////////////
// Engine - Main loop manager

class Engine {
public:
    static Engine& getInstance() {
        static Engine instance;
        return instance;
    }
    
    bool init(Game* gameInstance, int width = 1280, int height = 720);
    void run();
    void shutdown();
    
    float getDeltaTime() const { return deltaTime; }
    float getTime() const { return totalTime; }
    float getAlpha() const { return accumulator / fixedTimeStep; }
    
    Renderer* getRenderer() { return renderer; }
    
    void setZoom(float zoom);
    void setZoomCenter(float x, float y);
    float getZoom() const;
    float getAspectRatio() const;
    
private:
    Engine() = default;
    ~Engine() = default;
    
    void updateScene(Scene* scene, float deltaTime);
    void updateSceneFixed(Scene* scene, float fixedDeltaTime);
    
    Game* game = nullptr;
    Renderer* renderer = nullptr;
    
    float deltaTime = 0.0f;
    float totalTime = 0.0f;
    float accumulator = 0.0f;
    const float fixedTimeStep = 1.0f / 60.0f;
};

} // namespace froggi

#include "collision_system.h"

// Convenience macros
#define FROGGI_GAME_CLASS(ClassName) \
    class ClassName : public froggi::Game

#define FROGGI_MAIN(GameClass) \
    int main() { \
        froggi::Engine& engine = froggi::Engine::getInstance(); \
        GameClass game; \
        if (!engine.init(&game)) { \
            return 1; \
        } \
        engine.run(); \
        engine.shutdown(); \
        return 0; \
    }
