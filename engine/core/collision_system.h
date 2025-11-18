#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLock.h>

#include "jolt_debug_renderer.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace froggi {

// Forward declarations
class GameObject;
class Scene;
class JoltDebugRenderer;

///////////////////////////////////////////////////////////////////////////////
// Collision Shape Types

enum class CollisionShapeType {
    Box,
    Sphere,
    Capsule,
    Mesh
};

///////////////////////////////////////////////////////////////////////////////
// Collision Layer System (for filtering)

enum CollisionLayer : uint32_t {
    None = 0,
    Player = 1 << 0,
    Ground = 1 << 1,
    Wall = 1 << 2,
    Enemy = 1 << 3,
    Pickup = 1 << 4,
    Trigger = 1 << 5,
    All = 0xFFFFFFFF
};

///////////////////////////////////////////////////////////////////////////////
// Collision Result

struct CollisionResult {
    bool hasCollision = false;
    glm::vec3 contactPoint = glm::vec3(0.0f);
    glm::vec3 contactNormal = glm::vec3(0.0f);
    float penetrationDepth = 0.0f;
    GameObject* otherObject = nullptr;
};

struct RaycastHit {
    bool hit = false;
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
    GameObject* object = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Collider Component

class Collider : public Component {
public:
    Collider() = default;
    virtual ~Collider();
    
    // Shape configuration
    CollisionShapeType shapeType = CollisionShapeType::Box;
    glm::vec3 center = glm::vec3(0.0f);
    glm::vec3 size = glm::vec3(1.0f);  // For box
    float radius = 0.5f;                // For sphere/capsule
    float height = 1.0f;                // For capsule
    
    std::string meshPath;
    
    // Collision filtering
    uint32_t collisionLayer = CollisionLayer::None;
    uint32_t collisionMask = CollisionLayer::All;
    
    // Trigger mode (no physical response, just detection)
    bool isTrigger = false;
    
    // Callbacks
    virtual void onCollisionEnter(GameObject* other) { (void)other; }
    virtual void onCollisionStay(GameObject* other) { (void)other; }
    virtual void onCollisionExit(GameObject* other) { (void)other; }
    virtual void onTriggerEnter(GameObject* other) { (void)other; }
    virtual void onTriggerExit(GameObject* other) { (void)other; }
    
    // Jolt body ID (managed by CollisionSystem)
    JPH::BodyID bodyID;
    
    void updateTransform();
    bool shouldCollideWith(const Collider* other) const;
    
private:
    friend class CollisionSystem;
    void buildCollisionShape();
};

///////////////////////////////////////////////////////////////////////////////
// Rigidbody Component (for physics-based movement)

class Rigidbody : public Component {
public:
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 acceleration = glm::vec3(0.0f);
    float mass = 1.0f;
    float drag = 6.0f;
    float restitution = 0.0f;  // Bounciness
    float friction = 0.5f;
    float gravity = -30.0f;
    bool useGravity = true;
    bool isKinematic = false;
    
    // Ground detection
    bool isGrounded = false;
    glm::vec3 groundNormal = glm::vec3(0.0f, 0.0f, 1.0f);
    float groundCheckDistance = 0.1f;
    
    // For interpolation
    glm::vec3 previousPosition = glm::vec3(0.0f);
    glm::vec3 currentPosition = glm::vec3(0.0f);
    
    void onFixedUpdate(float fixedDeltaTime) override;
    void addForce(const glm::vec3& force);
    void addImpulse(const glm::vec3& impulse);
};

///////////////////////////////////////////////////////////////////////////////
// Contact Listener (for collision callbacks)

class ContactListenerImpl : public JPH::ContactListener {
public:
    void SetCollisionSystem(class CollisionSystem* system) { collisionSystem = system; }
    
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body &inBody1, 
        const JPH::Body &inBody2, 
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult &inCollisionResult) override;
    
    virtual void OnContactAdded(
        const JPH::Body &inBody1,
        const JPH::Body &inBody2,
        const JPH::ContactManifold &inManifold,
        JPH::ContactSettings &ioSettings) override;
    
    virtual void OnContactPersisted(
        const JPH::Body &inBody1,
        const JPH::Body &inBody2,
        const JPH::ContactManifold &inManifold,
        JPH::ContactSettings &ioSettings) override;
    
    virtual void OnContactRemoved(
        const JPH::SubShapeIDPair &inSubShapePair) override;
    
private:
    CollisionSystem* collisionSystem = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Collision System (manages all collision detection)

class CollisionSystem {
public:
    CollisionSystem();
    ~CollisionSystem();
    
    // Initialize/update collision world
    void initialize(Scene* scene);
    void update(Scene* scene, float deltaTime);
    
    // Collision queries
    std::vector<CollisionResult> overlapBox(const glm::vec3& center, 
                                           const glm::vec3& halfExtents,
                                           uint32_t layerMask = CollisionLayer::All);
    std::vector<CollisionResult> overlapSphere(const glm::vec3& center,
                                              float radius,
                                              uint32_t layerMask = CollisionLayer::All);
    
    RaycastHit raycast(const glm::vec3& origin,
                      const glm::vec3& direction,
                      float maxDistance = 100.0f,
                      uint32_t layerMask = CollisionLayer::All);
    
    bool checkGrounded(GameObject* object, float distance = 0.1f);
    
    GameObject* getGameObjectFromBodyID(JPH::BodyID bodyID);
    
    // Debug visualization - KEEP ONLY THESE, REMOVE DUPLICATES
    void enableDebugDraw(bool enable) { m_debugDrawEnabled = enable; }
    bool isDebugDrawEnabled() const { return m_debugDrawEnabled; }
    void drawDebugShapes();
    JoltDebugRenderer* getDebugRenderer() { return m_debugRenderer.get(); }
    
    JPH::PhysicsSystem* getPhysicsSystem() { return physicsSystem.get(); }
    
private:
    std::vector<JoltDebugRenderer::DebugLine> m_cachedStaticLines;
    bool m_staticLinesCached = false;
    // Jolt Physics objects
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;
    std::unique_ptr<ContactListenerImpl> contactListener;
    
    // Collision tracking
    std::vector<Collider*> colliders;
    std::unordered_map<JPH::BodyID, GameObject*> bodyToGameObject;
    std::unordered_map<Collider*, std::vector<Collider*>> activeCollisions;
    
    // Debug rendering
    std::unique_ptr<JoltDebugRenderer> m_debugRenderer;
    bool m_debugDrawEnabled = false;
    
    // Helper functions
    JPH::BodyID createBody(Collider* collider, Rigidbody* rigidbody);
    void updateRigidbodies(Scene* scene, float deltaTime);
    void syncJoltToGameObjects();
    
    static JPH::ObjectLayer getObjectLayer(uint32_t collisionLayer);
    static JPH::BroadPhaseLayer getBroadPhaseLayer(JPH::ObjectLayer layer);
};
} // namespace froggi
