//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/ecs/components/Component.h"
#include "engine/ecs/components/Transform.h"
#include "engine/ecs/components/RaycastedShadowsLight.h"
#include "System.h"

namespace Carrot::ECS {
    class SystemUpdateLightPosition: public LogicSystem<Transform, RaycastedShadowsLight> {
    public:
        explicit SystemUpdateLightPosition(World& world): LogicSystem<Transform, RaycastedShadowsLight>(world) {}

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override;
    };
}


