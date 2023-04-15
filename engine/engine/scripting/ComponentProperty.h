//
// Created by jglrxavpok on 29/03/2023.
//

#pragma once

#include <string>
#include <optional>
#include <core/scripting/csharp/forward.h>

namespace Carrot::Scripting {
    struct IntRange {
        int min;
        int max;
    };

    struct FloatRange {
        float min;
        float max;
    };

    enum class ComponentType {
        Int,
        Float,
        Boolean,
        Entity,

        UserDefined,
    };

    /**
     * Property of a component visible to editor
     */
    struct ComponentProperty {
        CSField* field = nullptr;

        std::string fieldName;
        ComponentType type;
        std::string typeStr;
        std::string serializationName;
        std::string displayName;

        std::optional<IntRange> intRange;
        std::optional<FloatRange> floatRange;

        /**
         * Called after loading the class from the assembly, ensure all data inside this struct is coherent
         */
        void validate();
    };
}