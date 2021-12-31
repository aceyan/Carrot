//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/ForceSinPosition.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemSinPosition: public LogicSystem<TransformComponent, ForceSinPosition> {
    private:
        double time = 0.0;

    public:
        explicit SystemSinPosition(World& world): LogicSystem<TransformComponent, ForceSinPosition>(world) {}

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;
    };
}


