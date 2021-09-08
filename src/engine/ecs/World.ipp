#include "World.h"
#include <algorithm>

template<class Comp>
Comp* Carrot::World::getComponent(Entity_Ptr& entity) const {
    auto componentMapLocation = this->entityComponents.find(*entity);
    if(componentMapLocation == this->entityComponents.end()) {
        // no such entity
        return nullptr;
    }

    auto& componentMap = componentMapLocation->second;
    auto componentLocation = componentMap.find(Comp::id);

    if(componentLocation == componentMap.end()) {
        // no such component
        return nullptr;
    }
    return dynamic_cast<Comp*>(componentLocation->second.get());
}

template<typename Comp>
Carrot::EasyEntity& Carrot::EasyEntity::addComponent(std::unique_ptr<Comp>&& component) {
    auto& componentMap = worldRef.entityComponents[*internalEntity];
    componentMap[Comp::id] = move(component);
    return *this;
}

template<typename Comp, typename... Args>
Carrot::EasyEntity& Carrot::EasyEntity::addComponent(Args&&... args) {
    auto& componentMap = worldRef.entityComponents[*internalEntity];
    componentMap[Comp::id] = make_unique<Comp>(*this, args...);
    return *this;
}

template<typename Comp, typename... Args>
Carrot::EasyEntity& Carrot::EasyEntity::addComponentIf(bool condition, Args&&... args) {
    if(condition) {
        addComponent<Comp>(std::forward<Args>(args)...);
    }
    return *this;
}

template<typename Comp>
Comp* Carrot::EasyEntity::getComponent() {
    return worldRef.getComponent<Comp>(internalEntity);
}

template<class RenderSystemType, typename... Args>
void Carrot::World::addRenderSystem(Args&&... args) {
    renderSystems.push_back(move(make_unique<RenderSystemType>(*this, args...)));
}

template<class LogicSystemType, typename... Args>
void Carrot::World::addLogicSystem(Args&&... args) {
    logicSystems.push_back(move(make_unique<LogicSystemType>(*this, args...)));
}

template<Carrot::SystemType type, typename... RequiredComponents>
void Carrot::SignedSystem<type, RequiredComponents...>::forEachEntity(const std::function<void(Entity_Ptr, RequiredComponents&...)>& action) {
    for(const auto& e : entities) {
        if (auto entity = e.lock()) {
            action(entity, *world.getComponent<RequiredComponents>(entity)...);
        }
    }
}
