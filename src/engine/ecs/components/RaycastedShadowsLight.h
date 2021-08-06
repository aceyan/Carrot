//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once

#include "Component.h"
#include <engine/render/lighting/Lights.h>

namespace Carrot {
    struct RaycastedShadowsLight: public IdentifiableComponent<RaycastedShadowsLight> {
        Light& lightRef;

        explicit RaycastedShadowsLight(EasyEntity entity, Light& light): IdentifiableComponent<RaycastedShadowsLight>(std::move(entity)), lightRef(light) {
            light.enabled = true;
        };
    };
}