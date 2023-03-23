//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "core/utils/Identifiable.h"
#include <engine/ecs/EntityTypes.h>
#include <rapidjson/document.h>
#include <utility>
#include <core/utils/Library.hpp>
#include <sol/sol.hpp>

namespace Carrot::Render {
    struct Context;
}

namespace Carrot::ECS {

    class Entity;

    struct Component {
    public:
        explicit Component(Entity entity): entity(std::move(entity)) {}

        Entity& getEntity() { return entity; }
        const Entity& getEntity() const { return entity; }

        void drawInspector(const Carrot::Render::Context& renderContext, bool& shouldKeep, bool& modified);
        virtual void drawInspectorInternals(const Carrot::Render::Context& renderContext, bool& modified) {};

        virtual const char* const getName() const = 0;

        virtual std::unique_ptr<Component> duplicate(const Entity& newOwner) const = 0;

        virtual rapidjson::Value toJSON(rapidjson::Document& doc) const {
            return rapidjson::Value(rapidjson::kObjectType);
        };

        virtual ~Component() = default;

        [[nodiscard]] virtual ComponentID getComponentTypeID() const = 0;

    private:
        Entity entity;
    };

    template<class Self>
    struct IdentifiableComponent: public Component, Identifiable<Self> {
        explicit IdentifiableComponent(Entity entity): Component(std::move(entity)) {}

        virtual const char* const getName() const override {
            return Self::getStringRepresentation();
        }

        virtual ComponentID getComponentTypeID() const override {
            return Self::getID();
        }
    };

    // Lua support
    template<typename T>
    concept LuaAccessibleComponent = requires(sol::state& s)
    {
        { T::registerUsertype(s) } -> std::convertible_to<void>;
    };

    class ComponentLibrary {
    private:
        using LuaBindingFunc = std::function<void(sol::state&, sol::usertype<Entity>&)>;
        using Storage = Library<std::unique_ptr<Component>, Entity>;

    public:
        using ID = Storage::ID;
        using LuaUsertypeSupplier = std::function<void(sol::state&)>;

        template<typename T> requires std::is_base_of_v<Component, T>
        void add() {
            storage.addUniquePtrBased<T>();
            bindingFuncs.push_back([](sol::state& state, sol::usertype<Entity>& u) {
                u.set(T::getStringRepresentation(), sol::property([](Entity& e) -> T* {
                    auto comp = e.getComponent<T>();
                    if(!comp.hasValue()) {
                        return nullptr;
                    }
                    return comp.asPtr();
                }));
            });
            if constexpr(LuaAccessibleComponent<T>) {
                usertypeDefinitionSuppliers.push_back(T::registerUsertype);
            }
        }

        void add(const Storage::ID& id, const Storage::DeserialiseFunction& deserialiseFunc, const Storage::CreateNewFunction& createNewFunc);

        [[nodiscard]] std::unique_ptr<Component> deserialise(const Storage::ID& id, const rapidjson::Value& json, const Entity& entity) const;
        [[nodiscard]] std::unique_ptr<Component> create(const Storage::ID& id, const Entity& entity) const;
        [[nodiscard]] std::vector<std::string> getAllIDs() const;

        void registerBindings(sol::state& d, sol::usertype<Entity>& uEntity);

        void remove(const Storage::ID& id);

    private:
        Storage storage;
        std::vector<LuaUsertypeSupplier> usertypeDefinitionSuppliers;
        std::vector<LuaBindingFunc> bindingFuncs;
    };

    ComponentLibrary& getComponentLibrary();
}
