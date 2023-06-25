//
// Created by jglrxavpok on 12/01/2022.
//

#include "Colliders.h"
#include <engine/utils/Macros.h>
#include <engine/utils/conversions.h>
#include <core/utils/JSON.h>
#include <engine/utils/conversions.h>
#include <engine/physics/PhysicsSystem.h>
#include <engine/physics/RigidBody.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

using namespace JPH;

namespace Carrot::Physics {

    Collider::Collider(std::unique_ptr<CollisionShape>&& collisionShape, const Carrot::Math::Transform& localTransform): shape(std::move(collisionShape)) {
        shape->setCollider(this);
        setLocalTransform(localTransform);
    }

    std::unique_ptr<Collider> Collider::loadFromJSON(const rapidjson::Value& object) {
        ColliderType colliderType = ColliderTypeNames[object["type"].GetString()];

        std::unique_ptr<CollisionShape> collisionShape;
        switch(colliderType) {
            case ColliderType::Box:
                collisionShape = std::make_unique<BoxCollisionShape>(object);
                break;

            case ColliderType::Sphere:
                collisionShape = std::make_unique<SphereCollisionShape>(object);
                break;

            case ColliderType::Capsule:
                collisionShape = std::make_unique<CapsuleCollisionShape>(object);
                break;

            case ColliderType::StaticConcaveTriangleMesh:
                TODO;
                break;

            default:
                TODO
                verify(false, "missing case");
                break;
        }

        Carrot::Math::Transform localTransform;
        localTransform.loadJSON(object["transform"]);
        return std::make_unique<Collider>(std::move(collisionShape), localTransform);
    }

    ColliderType Collider::getType() const {
        return shape->getType();
    }

    rapidjson::Value Collider::toJSON(rapidjson::Document::AllocatorType& allocator) const {
        rapidjson::Value data{ rapidjson::kObjectType };
        data.AddMember("type", rapidjson::Value(ColliderTypeNames[getType()], allocator), allocator);
        data.AddMember("transform", getLocalTransform().toJSON(allocator), allocator);
        shape->fillJSON(data, allocator);
        return data;
    }

    void Collider::addToBody(RigidBody& body) {
        rigidbody = &body;
    }

    void Collider::removeFromBody(RigidBody& body) {
        verify(rigidbody, "Must already be on a body");
        rigidbody = nullptr;
    }

    Carrot::Math::Transform Collider::getLocalTransform() const {
        return localTransform;
    }

    void Collider::setLocalTransform(const Carrot::Math::Transform& transform) {
        localTransform = transform;
        verify(shape->shape, "Must already have a shape");
        auto oldShapeRef = shape->shape;
        const JPH::Shape* oldShape = oldShapeRef.GetPtr();
        if(auto rotatedTranslated = dynamic_cast<const JPH::RotatedTranslatedShape*>(oldShapeRef.GetPtr())) {
            oldShape = rotatedTranslated->GetInnerShape();
        }
        shape->shape = JPH::RotatedTranslatedShapeSettings{
            carrotToJolt(transform.position),
            carrotToJolt(transform.rotation),
            oldShape
        }.Create().Get();
        oldShapeRef->Release();
        reattach();
    }

    [[nodiscard]] CollisionShape& Collider::getShape() const {
        return *shape;
    }

    void Collider::reattach() {
        verify(shape, "Must have a shape!");
        verify(rigidbody, "Must be associated to a RigidBody");

        // modifies the reactphysics rigidbody, without modifying the engine representation
        RigidBody* body = rigidbody;
        removeFromBody(*body);
        addToBody(*body);
    }

    // ---- Collision shapes ----

    bool CollisionShape::raycast(const glm::vec3& startPoint, const glm::vec3& direction, float maxLength, RaycastInfo& raycastInfo) {
        TODO;
    }

    void CollisionShape::reattachCollider() {
        if(owner) {
            owner->reattach();
        }
    }

    void CollisionShape::setCollider(Collider* collider) {
        this->owner = collider;
    }

    BoxCollisionShape::BoxCollisionShape() {
        setHalfExtents({1,1,1});
    }

    BoxCollisionShape::BoxCollisionShape(const rapidjson::Value& json): BoxCollisionShape() {
        setHalfExtents(JSON::read<3, float>(json["half_extents"]));
    }

    BoxCollisionShape::BoxCollisionShape(const glm::vec3& halfExtents): BoxCollisionShape() {
        setHalfExtents(halfExtents);
    }

    void BoxCollisionShape::setHalfExtents(const glm::vec3& halfExtents) {
        currentHalfExtents = halfExtents;
        shape = BoxShapeSettings{carrotToJolt(halfExtents)}.Create().Get();
        reattachCollider();
    }

    glm::vec3 BoxCollisionShape::getHalfExtents() const {
        return currentHalfExtents;
    }

    void BoxCollisionShape::fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const {
        object.AddMember("half_extents", JSON::write(getHalfExtents(), allocator), allocator);
    }

    BoxCollisionShape::~BoxCollisionShape() {

    }

    SphereCollisionShape::SphereCollisionShape() {
        setRadius(1.0f);
    }

    SphereCollisionShape::SphereCollisionShape(const rapidjson::Value& json): SphereCollisionShape() {
        setRadius(json["radius"].GetFloat());
    }

    SphereCollisionShape::SphereCollisionShape(float radius): SphereCollisionShape() {
        setRadius(radius);
    }

    void SphereCollisionShape::setRadius(float radius) {
        verify(radius > 0, "radius must be > 0");
        currentRadius = radius;
        shape = SphereShapeSettings{radius}.Create().Get();
        reattachCollider();
    }

    float SphereCollisionShape::getRadius() const {
        return currentRadius;
    }

    void SphereCollisionShape::fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const {
        object.AddMember("radius", getRadius(), allocator);
    }

    SphereCollisionShape::~SphereCollisionShape() {}

    CapsuleCollisionShape::CapsuleCollisionShape() {
        setRadiusAndHeight(1.0f, 1.0f);
    }

    CapsuleCollisionShape::CapsuleCollisionShape(const rapidjson::Value& json): CapsuleCollisionShape() {
        setRadiusAndHeight(json["radius"].GetFloat(), json["height"].GetFloat());
    }

    CapsuleCollisionShape::CapsuleCollisionShape(float radius, float height): CapsuleCollisionShape() {
        setRadiusAndHeight(radius, height);
    }

    void CapsuleCollisionShape::setRadius(float radius) {
        setRadiusAndHeight(radius, currentHeight);
    }

    float CapsuleCollisionShape::getRadius() const {
        return currentRadius;
    }
    
    void CapsuleCollisionShape::setHeight(float height) {
        setRadiusAndHeight(currentRadius, height);
    }

    float CapsuleCollisionShape::getHeight() const {
        return currentHeight;
    }

    void CapsuleCollisionShape::setRadiusAndHeight(float radius, float height) {
        currentRadius = radius;
        currentHeight = height;
        shape = CapsuleShapeSettings{height/2, radius}.Create().Get();
        reattachCollider();
    }

    void CapsuleCollisionShape::fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const {
        object.AddMember("radius", getRadius(), allocator);
        object.AddMember("height", getHeight(), allocator);
    }

    CapsuleCollisionShape::~CapsuleCollisionShape() {
    }
}