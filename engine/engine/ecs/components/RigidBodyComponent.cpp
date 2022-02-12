//
// Created by jglrxavpok on 31/12/2021.
//

#include "RigidBodyComponent.h"
#include "engine/utils/Macros.h"
#include "engine/physics/PhysicsSystem.h"
#include <core/utils/JSON.h>
#include <core/io/Logging.hpp>
#include <engine/render/Model.h>
#include <engine/Engine.h>
#include <engine/edition/DragDropTypes.h>
#include <engine/utils/conversions.h>
#include <core/utils/ImGuiUtils.hpp>
#include <imgui.h>
#include <map>
#include <filesystem>

namespace Carrot::ECS {

    RigidBodyComponent::RigidBodyComponent(const rapidjson::Value& json, Entity entity): RigidBodyComponent(std::move(entity)) {
        rigidbody.setBodyType(getTypeFromName(json["body_type"].GetString()));
        rigidbody.setMass(json["mass"].GetFloat());
        rigidbody.setLocalInertiaTensor(Carrot::JSON::read<3, float>(json["local_inertia_tensor"]));
        rigidbody.setLocalCenterOfMass(Carrot::JSON::read<3, float>(json["local_center_of_mass"]));

        for(const auto& colliderData : json["colliders"].GetArray()) {
            rigidbody.addColliderDirectly(Physics::Collider::loadFromJSON(colliderData));
        }
    }

    rapidjson::Value RigidBodyComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj{ rapidjson::kObjectType };
        rapidjson::Value bodyType { getTypeName(rigidbody.getBodyType()), doc.GetAllocator() };
        obj.AddMember("body_type", bodyType, doc.GetAllocator());
        obj.AddMember("mass", rigidbody.getMass(), doc.GetAllocator());
        obj.AddMember("local_inertia_tensor", Carrot::JSON::write(rigidbody.getLocalInertiaTensor(), doc), doc.GetAllocator());
        obj.AddMember("local_center_of_mass", Carrot::JSON::write(rigidbody.getLocalCenterOfMass(), doc), doc.GetAllocator());

        rapidjson::Value colliders{ rapidjson::kArrayType };
        for(const auto& collider : rigidbody.getColliders()) {
            colliders.PushBack(collider->toJSON(doc.GetAllocator()), doc.GetAllocator());
        }

        obj.AddMember("colliders", colliders, doc.GetAllocator());
        return obj;
    }

    void RigidBodyComponent::drawInspectorInternals(const Render::Context &renderContext, bool &modified) {
        const reactphysics3d::BodyType type = rigidbody.getBodyType();

        if(ImGui::BeginCombo("Type##rigidbodycomponent", getTypeName(type))) {
            for(reactphysics3d::BodyType bodyType : { reactphysics3d::BodyType::DYNAMIC, reactphysics3d::BodyType::KINEMATIC, reactphysics3d::BodyType::STATIC }) {
                bool selected = bodyType == type;
                if(ImGui::Selectable(getTypeName(bodyType), selected)) {
                    rigidbody.setBodyType(bodyType);
                    modified = true;
                }
            }
            ImGui::EndCombo();
        }

        auto drawColliderUI = [&](Physics::Collider& collider, std::size_t index) {
            ImGui::PushID(&collider);
            ImGui::Text("%s", Physics::ColliderTypeNames[collider.getType()]);
            ImGui::SameLine();
            bool shouldRemove = ImGui::Button("Remove collider");

            auto& shape = collider.getShape();
            switch(shape.getType()) {
                case Physics::ColliderType::Sphere: {
                    auto& sphere = static_cast<Physics::SphereCollisionShape&>(shape);
                    float radius = sphere.getRadius();
                    if(ImGui::DragFloat("Radius", &radius, 0.1f, 0.001f)) {
                        if(radius <= 0) {
                            radius = 10e-6f;
                        }
                        modified = true;
                        sphere.setRadius(radius);
                    }
                }
                break;

                case Physics::ColliderType::Box: {
                    auto& box = static_cast<Physics::BoxCollisionShape&>(shape);

                    float halfExtentsArray[3] = { box.getHalfExtents().x, box.getHalfExtents().y, box.getHalfExtents().z };
                    if(ImGui::SliderFloat3("Half extents", halfExtentsArray, 0.001f, 100)) {
                        modified = true;
                        box.setHalfExtents({ halfExtentsArray[0], halfExtentsArray[1], halfExtentsArray[2] });
                    }
                }
                break;

                case Physics::ColliderType::Capsule: {
                    auto& capsule = static_cast<Physics::CapsuleCollisionShape&>(shape);

                    float radius = capsule.getRadius();
                    float height = capsule.getHeight();
                    if(ImGui::DragFloat("Radius", &radius, 0.1f, 0.001f)) {
                        if(radius <= 0) {
                            radius = 10e-6f;
                        }
                        modified = true;
                        capsule.setRadius(radius);
                    }
                    if(ImGui::DragFloat("Height", &height, 0.1f, 0.001f)) {
                        if(height <= 0) {
                            height = 10e-6f;
                        }
                        modified = true;
                        capsule.setHeight(height);
                    }
                }
                break;

                case Physics::ColliderType::StaticConcaveTriangleMesh: {
                    auto& meshShape = static_cast<Physics::StaticConcaveMeshCollisionShape&>(shape);

                    auto setModel = [&](const std::string& modelPath) {
                        auto model = Engine::getInstance().getRenderer().getOrCreateModel(modelPath);
                        if(model) {
                            meshShape.setModel(model);
                            modified = true;
                        } else {
                            Carrot::Log::error("Could not open model for collisions: %s", modelPath.c_str());
                        }
                    };
                    std::string path = meshShape.getModel().getOriginatingResource().getName();
                    if(ImGui::InputText("Filepath##ModelComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        setModel(path);
                    }

                    if(ImGui::BeginDragDropTarget()) {
                        if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                            std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                            std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                            buffer.get()[payload->DataSize] = '\0';

                            std::filesystem::path newPath = buffer.get();

                            std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                            if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isModelFormatFromPath(fsPath)) {
                                setModel(Carrot::toString(fsPath.u8string()));
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }
                }
                break;

                default:
                    TODO
                    break;
            }

            ImGui::PopID();

            return shouldRemove;
        };

        if(ImGui::BeginChild("Colliders##rigidbodycomponent", ImVec2(), true)) {
            for(std::size_t index = 0; index < rigidbody.getColliderCount(); index++) {
                if(index != 0) {
                    ImGui::Separator();
                }
                bool remove = drawColliderUI(rigidbody.getCollider(index), index);
                if(remove) {
                    rigidbody.removeCollider(index);
                    index--;
                    modified = true;
                }
            }

            ImGui::Separator();

            if(ImGui::BeginMenu("Add##rigidbodycomponent colliders")) {
                if(ImGui::MenuItem("Sphere Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Physics::SphereCollisionShape(1.0f));
                    modified = true;
                }
                if(ImGui::MenuItem("Box Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Physics::BoxCollisionShape(glm::vec3(0.5f)));
                    modified = true;
                }
                if(ImGui::MenuItem("Capsule Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Physics::CapsuleCollisionShape(1.0f, 1.0f));
                    modified = true;
                }
                if(ImGui::MenuItem("Static Concave Mesh Collider##rigidbodycomponent colliders")) {
                    rigidbody.addCollider(Physics::StaticConcaveMeshCollisionShape(GetRenderer().getOrCreateModel("resources/models/simple_cube.obj")));
                    modified = true;
                }
                // TODO: convex

                ImGui::EndMenu();
            }
        }
        ImGui::EndChild();
    }

    const char* RigidBodyComponent::getTypeName(reactphysics3d::BodyType type) {
        switch (type) {
            case reactphysics3d::BodyType::DYNAMIC:
                return "Dynamic";

            case reactphysics3d::BodyType::KINEMATIC:
                return "Kinematic";

            case reactphysics3d::BodyType::STATIC:
                return "Static";
        }
        verify(false, "missing case");
        return "Dynamic";
    }

    const char* RigidBodyComponent::getShapeName(reactphysics3d::CollisionShapeType type) {
        switch (type) {
            case reactphysics3d::CollisionShapeType::SPHERE:
                return "Sphere Collider";

            case reactphysics3d::CollisionShapeType::CAPSULE:
                return "Capsule Collider";

            case reactphysics3d::CollisionShapeType::CONCAVE_SHAPE:
                return "Concave Shape Collider";

            case reactphysics3d::CollisionShapeType::CONVEX_POLYHEDRON:
                return "Mesh Collider";
        }
        verify(false, "missing case");
        return "Sphere Collider";
    }

    reactphysics3d::BodyType RigidBodyComponent::getTypeFromName(const std::string& name) {
        if(_stricmp(name.c_str(), "Dynamic") == 0) {
            return reactphysics3d::BodyType::DYNAMIC;
        }
        if(_stricmp(name.c_str(), "Kinematic") == 0) {
            return reactphysics3d::BodyType::KINEMATIC;
        }
        if(_stricmp(name.c_str(), "Static") == 0) {
            return reactphysics3d::BodyType::STATIC;
        }
        verify(false, "invalid string");
        return reactphysics3d::BodyType::DYNAMIC;
    }

    void RigidBodyComponent::reload() {
        rigidbody.setActive(wasActive);
    }

    void RigidBodyComponent::unload() {
        wasActive = rigidbody.isActive();
        rigidbody.setActive(false);
    }

}