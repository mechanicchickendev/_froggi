#include "pond_interface.h"
#include "renderer.h"
#include "collision_system.h"
#include <iostream>

namespace froggi {

///////////////////////////////////////////////////////////////////////////////
// Engine Implementation

bool Engine::init(Game* gameInstance, int width, int height) {
    if (!gameInstance) {
        std::cerr << "no_game_instance_provided₍!.!₎" << std::endl;
        return false;
    }
    
    std::cout << "_froggi_initializing...₍ᵔ~ᵔ₎" << std::endl;
    
    game = gameInstance;
    
    renderer = new Renderer();
    if (!renderer->init(width, height)) {
        std::cerr << "_failed_to_initialize_renderer₍!.!₎" << std::endl;
        return false;
    }
    
    Input::init(renderer->getWindow());
    
    std::cout << "_initializing_game...₍ᵔ~ᵔ₎" << std::endl;
    game->onInit();
    
    std::cout << "_engine_initialized_successfully₍ᵔ.ᵔ₎" << std::endl;
    return true;
}

void Engine::run() {
    std::cout << "_starting_game_loop...₍ᵔ~ᵔ₎" << std::endl;
    
    float lastTime = glfwGetTime();
    
    while (renderer->isRunning()) {
        float currentTime = glfwGetTime();
        deltaTime = currentTime - lastTime;
        if (deltaTime <= 0.0f) deltaTime = 1.0f / 60.0f;
        lastTime = currentTime;
        totalTime = currentTime;
        
        glfwPollEvents();
        Input::update();
        
        // ═══════════════════════════════════════════════════════════════
        // GAME UPDATE
        // ═══════════════════════════════════════════════════════════════
        
        game->onUpdate(deltaTime);
        
        if (game->currentScene) {
            updateScene(game->currentScene, deltaTime);
        }
        
        // ═══════════════════════════════════════════════════════════════
        // FIXED UPDATE (Physics & Collision)
        // ═══════════════════════════════════════════════════════════════
        
        accumulator += deltaTime;
        while (accumulator >= fixedTimeStep) {
            // STORE PREVIOUS POSITIONS BEFORE PHYSICS UPDATE
            if (game->currentScene) {
                for (auto* component : game->currentScene->components) {
                    froggi::Rigidbody* rb = dynamic_cast<froggi::Rigidbody*>(component);
                    if (rb && rb->enabled && rb->owner && !rb->isKinematic) {
                        rb->previousPosition = rb->owner->position;
                    }
                }
            }
            
            if (game->currentScene) {
                updateSceneFixed(game->currentScene, fixedTimeStep);
                
                // Update collision system
                if (game->currentScene->collisionSystem) {
                    game->currentScene->collisionSystem->update(
                        game->currentScene, fixedTimeStep);
                }
            }
            
            // STORE CURRENT POSITIONS AFTER PHYSICS UPDATE
            if (game->currentScene) {
                for (auto* component : game->currentScene->components) {
                    froggi::Rigidbody* rb = dynamic_cast<froggi::Rigidbody*>(component);
                    if (rb && rb->enabled && rb->owner && !rb->isKinematic) {
                        rb->currentPosition = rb->owner->position;
                    }
                }
            }
            
            accumulator -= fixedTimeStep;
        }
        
        // ═══════════════════════════════════════════════════════════════
        // INTERPOLATE VISUAL POSITIONS
        // ═══════════════════════════════════════════════════════════════
        
        float alpha = accumulator / fixedTimeStep;
        
        if (game->currentScene) {
            for (auto* component : game->currentScene->components) {
                froggi::Rigidbody* rb = dynamic_cast<froggi::Rigidbody*>(component);
                if (rb && rb->enabled && rb->owner && !rb->isKinematic) {
                    // Interpolate visual position between previous and current physics positions
                    glm::vec3 renderPosition = glm::mix(rb->previousPosition, rb->currentPosition, alpha);
                    rb->owner->position = renderPosition;
                }
            }
        }
       
// ═══════════════════════════════════════════════════════════════
// RENDER
// ═══════════════════════════════════════════════════════════════

if (game->currentScene && game->mainCamera) {
    glm::mat4 viewMatrix = game->mainCamera->getViewMatrix();
    glm::mat4 projectionMatrix = game->mainCamera->getProjectionMatrix(
        renderer->getAspectRatio()
    );
    
    // Pass UI callback to renderer
    renderer->renderScene(
        game->currentScene,
        viewMatrix,
        projectionMatrix,
        [this]() { game->onRenderUI(); }
    );
}
    }
    
    std::cout << "_game_loop_ended₍ᵔ!ᵔ₎" << std::endl;
}
void Engine::updateScene(Scene* scene, float deltaTime) {
    for (auto* component : scene->components) {
        if (component->enabled) {
            component->onUpdate(deltaTime);
        }
    }
}

void Engine::updateSceneFixed(Scene* scene, float fixedDeltaTime) {
    for (auto* component : scene->components) {
        if (component->enabled) {
            component->onFixedUpdate(fixedDeltaTime);
        }
    }
}

void Engine::shutdown() {
    std::cout << "_shutting_down...₍ᵔ~ᵔ₎" << std::endl;
    
    if (game) {
        game->onShutdown();
    }
    
    if (renderer) {
        renderer->shutdown();
        delete renderer;
        renderer = nullptr;
    }
    
    Input::shutdown();
    
    std::cout << "_engine_shutdown_complete₍ᵔ!ᵔ₎" << std::endl;
}

void Engine::setZoom(float zoom) {
    if (renderer) {
        renderer->setZoom(zoom);
    }
}

void Engine::setZoomCenter(float x, float y) {
    if (renderer) {
        renderer->setZoomCenter(x, y);
    }
}

float Engine::getZoom() const {
    if (renderer) {
        return renderer->getZoom();
    }
    return 1.0f;
}

float Engine::getAspectRatio() const {
    if (renderer) {
        return renderer->getAspectRatio();
    }
    return 16.0f / 9.0f;
}

///////////////////////////////////////////////////////////////////////////////
// Game Implementation

void Game::loadScene(Scene* scene) {
    if (currentScene) {
        if (currentScene->collisionSystem) {
            delete currentScene->collisionSystem;
            currentScene->collisionSystem = nullptr;
        }
        currentScene->onUnload();
        delete currentScene;
    }
    currentScene = scene;
    if (currentScene) {
        currentScene->onLoad();

        currentScene->collisionSystem = new CollisionSystem();
        currentScene->collisionSystem->initialize(currentScene);
    }
}

void Game::loadModel(const std::string& name, const std::string& path) {
    Engine& engine = Engine::getInstance();
    if (engine.getRenderer()) {
        engine.getRenderer()->loadMesh(name, path);
    }
}

} // namespace froggi
