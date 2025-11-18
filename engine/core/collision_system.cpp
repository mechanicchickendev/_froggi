#include "pond_interface.h"
#include "collision_system.h"
#include "resource_manager.h"
#include "tiny_obj_loader.h"
#include "jolt_debug_renderer.h"
#include <iostream>
#include <algorithm>
#include <cstdarg>
#include <cstdio>

// Jolt uses STL containers, disable warnings
JPH_SUPPRESS_WARNINGS

namespace froggi {

///////////////////////////////////////////////////////////////////////////////
// Helper: Convert glm to Jolt types

static JPH::Vec3 toJoltVec3(const glm::vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

static glm::vec3 toGlm(const JPH::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

static JPH::Quat toJoltQuat(const glm::vec3& eulerRadians) {
    // Convert Euler angles (XYZ) to quaternion
    JPH::Quat qx = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), eulerRadians.x);
    JPH::Quat qy = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), eulerRadians.y);
    JPH::Quat qz = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), eulerRadians.z);
    return qz * qy * qx;
}

// Trace callback function (not a lambda with variadic args)
static void TraceImpl(const char* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    std::cout << buffer << std::endl;
}

///////////////////////////////////////////////////////////////////////////////
// Object Layer Mapping

// Define object layers for broad phase
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

// Define broad phase layers
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS(2);
}

// BroadPhaseLayerInterface implementation
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }
    
    virtual uint GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }
    
    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }
    
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif
    
private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// ObjectLayerPairFilter implementation
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

// BroadPhaseLayerFilter implementation
class BPLayerFilterImpl : public JPH::BroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::BroadPhaseLayer inLayer) const override {
        // All layers collide
        return true;
    }
};

// ObjectVsBroadPhaseLayerFilter implementation
class ObjectVsBPLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                JPH_ASSERT(false);
                return false;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////
// Collider Implementation

Collider::~Collider() {
    // Cleanup handled by CollisionSystem
}

void Collider::buildCollisionShape() {
    // Shape creation handled by CollisionSystem now
}

void Collider::updateTransform() {
    // Will be synced via CollisionSystem
}

bool Collider::shouldCollideWith(const Collider* other) const {
    if (!other) return false;
    
    bool thisInOtherMask = (collisionLayer & other->collisionMask) != 0;
    bool otherInThisMask = (other->collisionLayer & collisionMask) != 0;
    
    return thisInOtherMask && otherInThisMask;
}

///////////////////////////////////////////////////////////////////////////////
// Rigidbody Implementation

void Rigidbody::onFixedUpdate(float fixedDeltaTime) {
    // Physics handled by Jolt now, but we still handle custom forces
    (void)fixedDeltaTime;
}

void Rigidbody::addForce(const glm::vec3& force) {
    if (mass > 0.0f) {
        acceleration += force / mass;
    }
}

void Rigidbody::addImpulse(const glm::vec3& impulse) {
    // For jump: just add to velocity, CollisionSystem will apply it
    velocity += impulse;
}

///////////////////////////////////////////////////////////////////////////////
// Contact Listener Implementation

JPH::ValidateResult ContactListenerImpl::OnContactValidate(
    const JPH::Body &inBody1, 
    const JPH::Body &inBody2, 
    JPH::RVec3Arg inBaseOffset,
    const JPH::CollideShapeResult &inCollisionResult) 
{
    (void)inBaseOffset;
    (void)inCollisionResult;
    
    // Check if bodies should collide based on layers
    GameObject* obj1 = collisionSystem->getGameObjectFromBodyID(inBody1.GetID());
    GameObject* obj2 = collisionSystem->getGameObjectFromBodyID(inBody2.GetID());
    
    if (!obj1 || !obj2) return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    
    Collider* col1 = obj1->getComponent<Collider>();
    Collider* col2 = obj2->getComponent<Collider>();
    
    if (!col1 || !col2) return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    
    if (!col1->shouldCollideWith(col2)) {
        return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
    }
    
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void ContactListenerImpl::OnContactAdded(
    const JPH::Body &inBody1,
    const JPH::Body &inBody2,
    const JPH::ContactManifold &inManifold,
    JPH::ContactSettings &ioSettings)
{
    (void)ioSettings;
    
    GameObject* obj1 = collisionSystem->getGameObjectFromBodyID(inBody1.GetID());
    GameObject* obj2 = collisionSystem->getGameObjectFromBodyID(inBody2.GetID());
    
    if (!obj1 || !obj2) return;
    
    Collider* col1 = obj1->getComponent<Collider>();
    Collider* col2 = obj2->getComponent<Collider>();
    
    if (!col1 || !col2) return;
    
    // Trigger callbacks
    if (col1->isTrigger || col2->isTrigger) {
        col1->onTriggerEnter(obj2);
        col2->onTriggerEnter(obj1);
    } else {
        col1->onCollisionEnter(obj2);
        col2->onCollisionEnter(obj1);
        
// Check for ground collision
glm::vec3 normal = toGlm(inManifold.mWorldSpaceNormal);

if (normal.z < -0.6f) { // Ground contact when normal points down
    Rigidbody* rb1 = obj1->getComponent<Rigidbody>();
    if (rb1) {
        rb1->isGrounded = true;
        rb1->groundNormal = -normal;
    }
}
if (normal.z > 0.6f) { // Inverted for other body (ground looking up at player)
    Rigidbody* rb2 = obj2->getComponent<Rigidbody>();
    if (rb2) {
        rb2->isGrounded = true;
        rb2->groundNormal = normal;
    }
}
    }
}

void ContactListenerImpl::OnContactPersisted(
    const JPH::Body &inBody1,
    const JPH::Body &inBody2,
    const JPH::ContactManifold &inManifold,
    JPH::ContactSettings &ioSettings)
{
    (void)ioSettings;
    
    GameObject* obj1 = collisionSystem->getGameObjectFromBodyID(inBody1.GetID());
    GameObject* obj2 = collisionSystem->getGameObjectFromBodyID(inBody2.GetID());
    
    if (!obj1 || !obj2) return;
    
    Collider* col1 = obj1->getComponent<Collider>();
    Collider* col2 = obj2->getComponent<Collider>();
    
    if (!col1 || !col2) return;
    
    if (!col1->isTrigger && !col2->isTrigger) {
        col1->onCollisionStay(obj2);
        col2->onCollisionStay(obj1);
        
        // ADD GROUND CHECK HERE TOO
        glm::vec3 normal = toGlm(inManifold.mWorldSpaceNormal);
        
        if (normal.z < -0.6f) { // Ground contact when normal points down
            Rigidbody* rb1 = obj1->getComponent<Rigidbody>();
            if (rb1) {
                rb1->isGrounded = true;
                rb1->groundNormal = -normal;
            }
        }
        if (normal.z > 0.6f) { // Inverted for other body
            Rigidbody* rb2 = obj2->getComponent<Rigidbody>();
            if (rb2) {
                rb2->isGrounded = true;
                rb2->groundNormal = normal;
            }
        }
    }
}  // <-- Make sure the function closes here!

void ContactListenerImpl::OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) {
    // Note: Jolt doesn't provide body pointers in OnContactRemoved
    // You may need to track this yourself if exit callbacks are critical
    (void)inSubShapePair;
}

///////////////////////////////////////////////////////////////////////////////
// CollisionSystem Implementation

CollisionSystem::CollisionSystem() {
    // Register allocation hook
    JPH::RegisterDefaultAllocator();
    
    // Install trace and assert callbacks
    JPH::Trace = TraceImpl;
    
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = [](const char* inExpression, const char* inMessage, const char* inFile, uint inLine) {
        std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << std::endl;
        return true;
    };)
    
    // Create factory
    JPH::Factory::sInstance = new JPH::Factory();
    
    // Register all Jolt physics types
    JPH::RegisterTypes();
    
    // Create temp allocator
    tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    
    // Create job system
    jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 4);
    
    // Create physics system
    const uint cMaxBodies = 1024;
    const uint cNumBodyMutexes = 0; // Auto-detect
    const uint cMaxBodyPairs = 1024;
    const uint cMaxContactConstraints = 1024;
    
    physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    physicsSystem->Init(
        cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        *new BPLayerInterfaceImpl(),
        *new ObjectVsBPLayerFilterImpl(),
        *new ObjectLayerPairFilterImpl()
    );
    
    // Create contact listener
    contactListener = std::make_unique<ContactListenerImpl>();
    contactListener->SetCollisionSystem(this);
    physicsSystem->SetContactListener(contactListener.get());
    
    // Set gravity
    physicsSystem->SetGravity(JPH::Vec3(0, 0, -30.0f)); // Z-up

    // Create debug renderer
    m_debugRenderer = std::make_unique<JoltDebugRenderer>();
    
}

CollisionSystem::~CollisionSystem() {
    // Cleanup
    colliders.clear();
    bodyToGameObject.clear();
    
    if (physicsSystem) {
        physicsSystem.reset();
    }
    
    if (jobSystem) {
        jobSystem.reset();
    }
    
    if (tempAllocator) {
        tempAllocator.reset();
    }
    
    // Unregister types
    JPH::UnregisterTypes();
    
    // Destroy factory
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void CollisionSystem::initialize(Scene* scene) {
    if (!scene) return;
    
    colliders.clear();
    bodyToGameObject.clear();
    
    // Create bodies for all colliders
    for (auto* component : scene->components) {
        Collider* collider = dynamic_cast<Collider*>(component);
        if (collider && collider->owner) {
            Rigidbody* rb = collider->owner->getComponent<Rigidbody>();
            JPH::BodyID bodyID = createBody(collider, rb);
            
            if (!bodyID.IsInvalid()) {
                collider->bodyID = bodyID;
                bodyToGameObject[bodyID] = collider->owner;
                colliders.push_back(collider);
            }
        }
    }
    
    std::cout << "[CollisionSystem] Initialized with " << colliders.size() << " colliders using Jolt Physics" << std::endl;
}

JPH::BodyID CollisionSystem::createBody(Collider* collider, Rigidbody* rigidbody) {
    if (!collider || !collider->owner) return JPH::BodyID();
    
    // Create shape
    JPH::RefConst<JPH::Shape> shape;
    switch (collider->shapeType) {
        case CollisionShapeType::Box:
            shape = new JPH::BoxShape(toJoltVec3(collider->size * 0.5f));
            break;
        case CollisionShapeType::Sphere:
            shape = new JPH::SphereShape(collider->radius);
            break;
        case CollisionShapeType::Capsule:
            shape = new JPH::CapsuleShape(collider->height * 0.5f, collider->radius);
            break;
      case CollisionShapeType::Mesh: {
            if (collider->meshPath.empty()) {
                std::cerr << "[Collider] Mesh path not set, using box" << std::endl;
                shape = new JPH::BoxShape(toJoltVec3(collider->size * 0.5f));
                break;
            }
            
            // Load OBJ file
            std::cout << "[Collider] Loading mesh collision from: " << collider->meshPath << std::endl;
            
            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string warn, err;
            
            if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, collider->meshPath.c_str())) {
                std::cerr << "[Collider] Failed to load mesh: " << err << std::endl;
                shape = new JPH::BoxShape(toJoltVec3(collider->size * 0.5f));
                break;
            }
            
            if (!warn.empty()) {
                std::cout << "[Collider] Warning: " << warn << std::endl;
            }
            
            // Convert to Jolt triangle list
            JPH::TriangleList triangles;
            
            for (const auto& objShape : shapes) {
                size_t index_offset = 0;
                
                for (size_t f = 0; f < objShape.mesh.num_face_vertices.size(); f++) {
                    int fv = objShape.mesh.num_face_vertices[f];
                    
                    if (fv == 3) {
                        // Triangle - get the 3 vertices
                        JPH::Float3 v0, v1, v2;
                        
                        for (int v = 0; v < 3; v++) {
                            tinyobj::index_t idx = objShape.mesh.indices[index_offset + v];
                            
                            float vx = attrib.vertices[3 * idx.vertex_index + 0];
                            float vy = attrib.vertices[3 * idx.vertex_index + 1];
                            float vz = attrib.vertices[3 * idx.vertex_index + 2];
                            
                            // Convert from OBJ coords (Y-up) to game coords (Z-up)
                            // Y-up to Z-up: (x, y, z) -> (x, -z, y)
                            if (v == 0) {
                                v0 = JPH::Float3(vx, -vz, vy);
                            } else if (v == 1) {
                                v1 = JPH::Float3(vx, -vz, vy);
                            } else {
                                v2 = JPH::Float3(vx, -vz, vy);
                            }
                        }
                        
                        triangles.push_back(JPH::Triangle(v0, v1, v2));
                    } else if (fv == 4) {
                        // Quad - split into 2 triangles
                        JPH::Float3 v0, v1, v2, v3;
                        
                        for (int v = 0; v < 4; v++) {
                            tinyobj::index_t idx = objShape.mesh.indices[index_offset + v];
                            
                            float vx = attrib.vertices[3 * idx.vertex_index + 0];
                            float vy = attrib.vertices[3 * idx.vertex_index + 1];
                            float vz = attrib.vertices[3 * idx.vertex_index + 2];
                            
                            JPH::Float3 vert(vx, -vz, vy);
                            
                            if (v == 0) v0 = vert;
                            else if (v == 1) v1 = vert;
                            else if (v == 2) v2 = vert;
                            else v3 = vert;
                        }
                        
                        // Split quad into 2 triangles
                        triangles.push_back(JPH::Triangle(v0, v1, v2));
                        triangles.push_back(JPH::Triangle(v0, v2, v3));
                    }
                    
                    index_offset += fv;
                }
            }
            
            if (triangles.empty()) {
                std::cerr << "[Collider] No triangles found in mesh, using box" << std::endl;
                shape = new JPH::BoxShape(toJoltVec3(collider->size * 0.5f));
                break;
            }
            
            std::cout << "[Collider] Created mesh with " << triangles.size() << " triangles" << std::endl;
            
            // Create mesh shape
            JPH::MeshShapeSettings meshSettings(triangles);
            JPH::ShapeSettings::ShapeResult result = meshSettings.Create();
            
            if (result.HasError()) {
                std::cerr << "[Collider] Failed to create mesh shape: " << result.GetError() << std::endl;
                shape = new JPH::BoxShape(toJoltVec3(collider->size * 0.5f));
            } else {
                shape = result.Get();
            }
            break;
        }
    }
    
    // Determine motion type
    // IMPORTANT: No Rigidbody = Static, Rigidbody with isKinematic = Kinematic, else Dynamic
    JPH::EMotionType motionType;
    if (!rigidbody) {
        // No rigidbody component = completely static (like ground)
        motionType = JPH::EMotionType::Static;
        std::cout << "[CollisionSystem] Creating STATIC body for: " << collider->owner->name << std::endl;
    } else if (rigidbody->isKinematic) {
        motionType = JPH::EMotionType::Kinematic;
        std::cout << "[CollisionSystem] Creating KINEMATIC body for: " << collider->owner->name << std::endl;
    } else {
        motionType = JPH::EMotionType::Dynamic;
        std::cout << "[CollisionSystem] Creating DYNAMIC body for: " << collider->owner->name << std::endl;
    }
    
    // Determine object layer
    JPH::ObjectLayer objectLayer = (motionType == JPH::EMotionType::Static) 
        ? Layers::NON_MOVING 
        : Layers::MOVING;
    
    // Create position and rotation
    glm::vec3 pos = collider->owner->position + collider->center;
    JPH::RVec3 position = toJoltVec3(pos);
    JPH::Quat rotation = toJoltQuat(collider->owner->rotation);
    
    // Create body settings
    JPH::BodyCreationSettings bodySettings(
        shape,
        position,
        rotation,
        motionType,
        objectLayer
    );
    
    // Set physics properties (only for dynamic/kinematic bodies)
    if (rigidbody) {
        bodySettings.mFriction = rigidbody->friction;
        bodySettings.mRestitution = rigidbody->restitution;
        
        if (motionType == JPH::EMotionType::Dynamic) {
            bodySettings.mLinearDamping = 0.05f;  // Small damping for stability
            bodySettings.mAngularDamping = 0.05f;
            bodySettings.mGravityFactor = rigidbody->useGravity ? 1.0f : 0.0f;
            bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            bodySettings.mMassPropertiesOverride.mMass = rigidbody->mass;
            
            // Lock rotation for character controllers
            bodySettings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | 
                                        JPH::EAllowedDOFs::TranslationY | 
                                        JPH::EAllowedDOFs::TranslationZ;
        }
    } else {
        // Static bodies have infinite friction by default
        bodySettings.mFriction = 0.5f;
        bodySettings.mRestitution = 0.0f;
    }
    
    // Set as sensor if trigger
    bodySettings.mIsSensor = collider->isTrigger;
    
    // Create body
    JPH::Body* body = physicsSystem->GetBodyInterface().CreateBody(bodySettings);
    if (!body) {
        std::cerr << "[CollisionSystem] Failed to create body for " << collider->owner->name << "!" << std::endl;
        return JPH::BodyID();
    }
    
    // Add to physics system
    JPH::EActivation activation = (motionType == JPH::EMotionType::Static) 
        ? JPH::EActivation::DontActivate 
        : JPH::EActivation::Activate;
    
    physicsSystem->GetBodyInterface().AddBody(body->GetID(), activation);
    
    std::cout << "[CollisionSystem] Body created successfully for " << collider->owner->name 
              << " (ID: " << body->GetID().GetIndex() << ")" << std::endl;
    
    return body->GetID();
}

void CollisionSystem::update(Scene* scene, float deltaTime) {
    if (!scene) return;
    
    // Reset grounded state
    for (auto* component : scene->components) {
        Rigidbody* rb = dynamic_cast<Rigidbody*>(component);
        if (rb && rb->enabled) {
            rb->isGrounded = false;
        }
    }
    
    // Update Jolt body transforms from GameObjects (for kinematic/updated objects)
    for (auto* collider : colliders) {
        if (!collider->enabled || !collider->owner->active) continue;
        
        Rigidbody* rb = collider->owner->getComponent<Rigidbody>();
        if (rb && rb->isKinematic) {
            glm::vec3 pos = collider->owner->position + collider->center;
            JPH::RVec3 position = toJoltVec3(pos);
            JPH::Quat rotation = toJoltQuat(collider->owner->rotation);
            
            physicsSystem->GetBodyInterface().SetPositionAndRotation(
                collider->bodyID,
                position,
                rotation,
                JPH::EActivation::Activate
            );
        }
        
        // Apply custom forces/impulses
        if (rb && !rb->isKinematic) {
            // Get current Jolt velocity
            JPH::Vec3 currentJoltVel = physicsSystem->GetBodyInterface().GetLinearVelocity(collider->bodyID);
            glm::vec3 currentVel = toGlm(currentJoltVel);
            
            // Only apply if velocity was changed from outside (player controller)
            if (glm::length(rb->velocity - currentVel) > 0.01f) {
                // Velocity was set by game code - apply it
                physicsSystem->GetBodyInterface().SetLinearVelocity(
                    collider->bodyID,
                    toJoltVec3(rb->velocity)
                );
            }
            
            // Apply forces (continuous acceleration)
            if (glm::length(rb->acceleration) > 0.001f) {
                physicsSystem->GetBodyInterface().AddForce(
                    collider->bodyID,
                    toJoltVec3(rb->acceleration * rb->mass)
                );
                rb->acceleration = glm::vec3(0.0f);
            }
        }
    }
    
    // Step physics simulation - INCREASE COLLISION STEPS
    const int collisionSteps = 4;  // Changed from 1 to 4
    physicsSystem->Update(deltaTime, collisionSteps, tempAllocator.get(), jobSystem.get());
    
    // Sync Jolt transforms back to GameObjects
    syncJoltToGameObjects();
}

void CollisionSystem::syncJoltToGameObjects() {
    JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
    
    for (auto* collider : colliders) {
        if (!collider->enabled || !collider->owner->active) continue;
        
        Rigidbody* rb = collider->owner->getComponent<Rigidbody>();
        if (!rb || rb->isKinematic) continue; // Only sync dynamic bodies
        
        JPH::RVec3 position = bodyInterface.GetPosition(collider->bodyID);
        JPH::Vec3 joltVelocity = bodyInterface.GetLinearVelocity(collider->bodyID);
        
        // Update GameObject position DIRECTLY (interpolation happens in engine loop)
        collider->owner->position = toGlm(position) - collider->center;
        
        // Update Rigidbody velocity
        rb->velocity = toGlm(joltVelocity);
    }
}

GameObject* CollisionSystem::getGameObjectFromBodyID(JPH::BodyID bodyID) {
    auto it = bodyToGameObject.find(bodyID);
    if (it != bodyToGameObject.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<CollisionResult> CollisionSystem::overlapBox(
    const glm::vec3& center, const glm::vec3& halfExtents, uint32_t layerMask) 
{
    (void)center;
    (void)halfExtents;
    (void)layerMask;
    std::vector<CollisionResult> results;
    // TODO: Implement using Jolt's shape casting
    return results;
}

std::vector<CollisionResult> CollisionSystem::overlapSphere(
    const glm::vec3& center, float radius, uint32_t layerMask)
{
    (void)center;
    (void)radius;
    (void)layerMask;
    std::vector<CollisionResult> results;
    // TODO: Implement using Jolt's shape casting
    return results;
}

RaycastHit CollisionSystem::raycast(const glm::vec3& origin, 
                                    const glm::vec3& direction,
                                    float maxDistance,
                                    uint32_t layerMask)
{
    (void)layerMask; // TODO: Implement layer filtering
    RaycastHit hit;
    
    glm::vec3 normalizedDir = glm::normalize(direction);
    JPH::Vec3 rayOrigin = toJoltVec3(origin);
    JPH::Vec3 rayDirection = toJoltVec3(normalizedDir * maxDistance);
    
    // Setup raycast
    JPH::RRayCast ray { rayOrigin, rayDirection };
    
    // Perform raycast
    JPH::RayCastResult result;
    JPH::BroadPhaseLayerFilter broadPhaseFilter;
    JPH::ObjectLayerFilter objectLayerFilter;
    JPH::BodyFilter bodyFilter;
    
    if (physicsSystem->GetNarrowPhaseQuery().CastRay(
        ray, 
        result,
        broadPhaseFilter,
        objectLayerFilter,
        bodyFilter)) 
    {
        hit.hit = true;
        hit.distance = result.mFraction * maxDistance;
        hit.point = origin + normalizedDir * hit.distance;
        
        GameObject* obj = getGameObjectFromBodyID(result.mBodyID);
        if (obj) {
            hit.object = obj;
        }
        
        // Get normal from body
        JPH::BodyLockRead lock(physicsSystem->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            hit.normal = toGlm(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction)));
        }
    }
    
    return hit;
}

bool CollisionSystem::checkGrounded(GameObject* object, float distance) {
    if (!object) return false;
    
    Collider* collider = object->getComponent<Collider>();
    if (!collider) return false;
    
    glm::vec3 origin = object->position + collider->center;
    glm::vec3 direction = glm::vec3(0, 0, -1);
    
    RaycastHit hit = raycast(origin, direction, distance, CollisionLayer::Ground);
    
    if (hit.hit) {
        Rigidbody* rb = object->getComponent<Rigidbody>();
        if (rb) {
            rb->isGrounded = true;
            rb->groundNormal = hit.normal;
        }
        return true;
    }
    
    return false;
}

void CollisionSystem::drawDebugShapes() {
    if (!m_debugDrawEnabled || !physicsSystem) return;
    
    m_debugRenderer->clear();
    
    const JPH::BodyLockInterface &lockInterface = physicsSystem->GetBodyLockInterfaceNoLock();
    
    // ═══════════════════════════════════════════════════════════════
    // PART 1: Draw static meshes (cached)
    // ═══════════════════════════════════════════════════════════════
    
    if (!m_staticLinesCached) {
        std::cout << "[CollisionSystem] Generating static mesh cache..." << std::endl;
        
        // Draw all static mesh shapes and cache the lines
        for (const auto& pair : bodyToGameObject) {
            JPH::BodyID bodyID = pair.first;
            
            JPH::BodyLockRead lock(lockInterface, bodyID);
            if (lock.Succeeded()) {
                const JPH::Body &body = lock.GetBody();
                
                // Only cache static bodies
                if (body.GetMotionType() != JPH::EMotionType::Static) continue;
                
                const JPH::Shape* shape = body.GetShape();
                
                // Only cache mesh shapes
                if (shape->GetType() == JPH::EShapeType::Mesh) {
                    std::cout << "[CollisionSystem]   Caching mesh triangles..." << std::endl;
                    
                    const JPH::MeshShape* meshShape = static_cast<const JPH::MeshShape*>(shape);
                    
                    JPH::Shape::GetTrianglesContext context;
                    meshShape->GetTrianglesStart(
                        context, 
                        JPH::AABox::sBiggest(),
                        body.GetPosition(),
                        body.GetRotation(),
                        JPH::Vec3::sReplicate(1.0f)
                    );
                    
                    JPH::Color color = JPH::Color::sYellow;
                    int triangleCount = 0;
                    
                    for (;;) {
                        constexpr int cMaxTriangles = 32;
                        JPH::Float3 vertices[cMaxTriangles * 3];
                        int count = meshShape->GetTrianglesNext(context, cMaxTriangles, vertices);
                        
                        if (count == 0) break;
                        
                        for (int i = 0; i < count; ++i) {
                            JPH::Float3 v0 = vertices[i * 3 + 0];
                            JPH::Float3 v1 = vertices[i * 3 + 1];
                            JPH::Float3 v2 = vertices[i * 3 + 2];
                            
                            JPH::Vec3 w0(v0.x, v0.y, v0.z);
                            JPH::Vec3 w1(v1.x, v1.y, v1.z);
                            JPH::Vec3 w2(v2.x, v2.y, v2.z);
                            
                            // Draw to debug renderer (will be cached below)
                            m_debugRenderer->DrawLine(w0, w1, color);
                            m_debugRenderer->DrawLine(w1, w2, color);
                            m_debugRenderer->DrawLine(w2, w0, color);
                            
                            triangleCount++;
                        }
                    }
                    
                    std::cout << "[CollisionSystem]   Cached " << triangleCount << " triangles" << std::endl;
                }
            }
        }
        
        // Cache the lines
        m_cachedStaticLines = m_debugRenderer->getLines();
        m_staticLinesCached = true;
        
        std::cout << "[CollisionSystem] Static cache created: " << m_cachedStaticLines.size() << " lines" << std::endl;
        
        // Clear for dynamic rendering
        m_debugRenderer->clear();
    }
    
    // Draw cached static lines
    for (const auto& line : m_cachedStaticLines) {
        JPH::Color color(
            static_cast<uint8_t>(line.color.r * 255.0f),
            static_cast<uint8_t>(line.color.g * 255.0f),
            static_cast<uint8_t>(line.color.b * 255.0f),
            static_cast<uint8_t>(line.color.a * 255.0f)
        );
        
        m_debugRenderer->DrawLine(
            JPH::RVec3(line.start.x, line.start.y, line.start.z),
            JPH::RVec3(line.end.x, line.end.y, line.end.z),
            color
        );
    }
    
    // ═══════════════════════════════════════════════════════════════
    // PART 2: Draw dynamic bodies (every frame)
    // ═══════════════════════════════════════════════════════════════
    
    for (const auto& pair : bodyToGameObject) {
        JPH::BodyID bodyID = pair.first;
        
        JPH::BodyLockRead lock(lockInterface, bodyID);
        if (lock.Succeeded()) {
            const JPH::Body &body = lock.GetBody();
            
// Only draw dynamic/kinematic bodies here
            if (body.GetMotionType() == JPH::EMotionType::Static) continue;
            
            JPH::Color color = JPH::Color::sGreen;
            
            // Draw bounding box for dynamic bodies (capsule, etc)
            JPH::AABox bbox = body.GetWorldSpaceBounds();
            JPH::Vec3 min = bbox.mMin;
            JPH::Vec3 max = bbox.mMax;
            
            // Draw 12 edges
            m_debugRenderer->DrawLine(JPH::RVec3(min.GetX(), min.GetY(), min.GetZ()), 
                                     JPH::RVec3(max.GetX(), min.GetY(), min.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(max.GetX(), min.GetY(), min.GetZ()), 
                                     JPH::RVec3(max.GetX(), max.GetY(), min.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(max.GetX(), max.GetY(), min.GetZ()), 
                                     JPH::RVec3(min.GetX(), max.GetY(), min.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(min.GetX(), max.GetY(), min.GetZ()), 
                                     JPH::RVec3(min.GetX(), min.GetY(), min.GetZ()), color);
            
            m_debugRenderer->DrawLine(JPH::RVec3(min.GetX(), min.GetY(), max.GetZ()), 
                                     JPH::RVec3(max.GetX(), min.GetY(), max.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(max.GetX(), min.GetY(), max.GetZ()), 
                                     JPH::RVec3(max.GetX(), max.GetY(), max.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(max.GetX(), max.GetY(), max.GetZ()), 
                                     JPH::RVec3(min.GetX(), max.GetY(), max.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(min.GetX(), max.GetY(), max.GetZ()), 
                                     JPH::RVec3(min.GetX(), min.GetY(), max.GetZ()), color);
            
            m_debugRenderer->DrawLine(JPH::RVec3(min.GetX(), min.GetY(), min.GetZ()), 
                                     JPH::RVec3(min.GetX(), min.GetY(), max.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(max.GetX(), min.GetY(), min.GetZ()), 
                                     JPH::RVec3(max.GetX(), min.GetY(), max.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(max.GetX(), max.GetY(), min.GetZ()), 
                                     JPH::RVec3(max.GetX(), max.GetY(), max.GetZ()), color);
            m_debugRenderer->DrawLine(JPH::RVec3(min.GetX(), max.GetY(), min.GetZ()), 
                                     JPH::RVec3(min.GetX(), max.GetY(), max.GetZ()), color);
        }
    }
}

} // namespace froggi
