//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <engine/ecs/components/Component.h>
#include <engine/ecs/systems/System.h>
#include "EntityTypes.h"

using namespace std;

namespace Carrot {

    using Tags = std::uint64_t;

    class World {
    private:

        /// Wrapper struct to allow easy addition of components
        struct EasyEntity {
            EasyEntity(Entity_Ptr ent, World& worldRef): internalEntity(ent), worldRef(worldRef) {}

            template<typename Comp>
            EasyEntity& addComponent(unique_ptr<Comp>&& component);

            template<typename Comp, typename... Arg>
            EasyEntity& addComponent(Arg&&... args);

            template<typename Comp>
            Comp* getComponent();

            EasyEntity& addTag(Tags tag);

            operator Entity_Ptr() {
                return internalEntity;
            }

            operator Entity_Ptr() const {
                return internalEntity;
            }

        private:
            Entity_Ptr internalEntity;
            World& worldRef;

        };

        EntityID freeEntityID = 0;
        vector<Entity_Ptr> entities;
        vector<Entity_Ptr> entitiesToAdd;
        vector<Entity_Ptr> entitiesToRemove;

        unordered_map<EntityID, unordered_map<ComponentID, unique_ptr<Component>>> entityComponents;
        unordered_map<EntityID, Tags> entityTags;

        vector<unique_ptr<System>> logicSystems;
        vector<unique_ptr<System>> renderSystems;

    public:
        Signature getSignature(const Entity_Ptr& entity) const;

        template<class Comp>
        Comp* getComponent(Entity_Ptr& entity) const;

        Tags getTags(const Entity_Ptr& entity) const;

        std::vector<Entity_Ptr> getEntitiesWithTags(Tags tags) const;

        void tick(double dt);
        void onFrame(Carrot::Render::Context renderContext);
        void recordGBufferPass(vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands);

        EasyEntity newEntity();

        template<class RenderSystemType, typename... Args>
        void addRenderSystem(Args&&... args);

        template<class LogicSystemType, typename... Args>
        void addLogicSystem(Args&&... args);
    };
}

#include "World.ipp"
