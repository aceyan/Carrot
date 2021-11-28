//
// Created by jglrxavpok on 05/11/2021.
//

#include "Lights.h"

#include <utility>
#include "engine/Engine.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/utils/Macros.h"

namespace Carrot::Render {
    static const std::uint32_t BindingCount = 1;

    LightHandle::LightHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, Lighting& system): WeakPoolHandle::WeakPoolHandle(index, std::move(destructor)), lightingSystem(system) {}

    void LightHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto& data = lightingSystem.getLightData(*this);
        data = light;
    }

    Lighting::Lighting() {
        reallocateBuffer(DefaultLightBufferSize);
        vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;
        std::array<vk::DescriptorSetLayoutBinding, BindingCount> bindings = {
                // Light Buffer
                vk::DescriptorSetLayoutBinding {
                        .binding = 0,
                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                        .descriptorCount = 1,
                        .stageFlags = stageFlags
                },
        };
        descriptorSetLayout = GetVulkanDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
                .bindingCount = static_cast<std::uint32_t>(bindings.size()),
                .pBindings = bindings.data(),
        });
        std::array<vk::DescriptorPoolSize, BindingCount> poolSizes = {
                vk::DescriptorPoolSize {
                        .type = vk::DescriptorType::eStorageBuffer,
                        .descriptorCount = GetEngine().getSwapchainImageCount(),
                },
        };
        descriptorSetPool = GetVulkanDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
                .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = GetEngine().getSwapchainImageCount(),
                .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        });

        reallocateDescriptorSets();
    }

    std::shared_ptr<LightHandle> Lighting::create() {
        auto ptr = lightHandles.create(std::ref(*this));
        if(lightHandles.size() >= lightBufferSize) {
            reallocateBuffer(lightBufferSize*2);
        }
        return ptr;
    }

    void Lighting::reallocateBuffer(std::uint32_t lightCount) {
        lightBufferSize = std::max(lightCount, DefaultLightBufferSize);
        lightBuffer = GetResourceAllocator().allocateDedicatedBuffer(
                sizeof(Data) + lightBufferSize * sizeof(Light),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );
        data = lightBuffer->map<Data>();
        descriptorNeedsUpdate = std::vector<bool>(descriptorSets.size(), true);
    }

    void Lighting::bind(const Context& renderContext, vk::CommandBuffer& cmds, std::uint32_t index, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint bindPoint) {
        cmds.bindDescriptorSets(bindPoint, pipelineLayout, index, {descriptorSets[renderContext.swapchainIndex]}, {});
    }

    void Lighting::onFrame(const Context& renderContext) {
        lightHandles.erase(std::find_if(WHOLE_CONTAINER(lightHandles), [](auto handlePtr) { return handlePtr.second.expired(); }), lightHandles.end());
        data->lightCount = lightBufferSize;
        data->ambient = ambientColor;
        for(auto& [slot, handlePtr] : lightHandles) {
            if(auto handle = handlePtr.lock()) {
                handle->updateHandle(renderContext);
            }
        }

        if(descriptorNeedsUpdate[renderContext.swapchainIndex]) {
            auto& set = descriptorSets[renderContext.swapchainIndex];
            auto lightBufferInfo = lightBuffer->getWholeView().asBufferInfo();
            std::array<vk::WriteDescriptorSet, BindingCount> writes = {
                    // Material buffer
                    vk::WriteDescriptorSet {
                            .dstSet = set,
                            .dstBinding = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &lightBufferInfo,
                    },
            };
            GetVulkanDevice().updateDescriptorSets(writes, {});
            descriptorNeedsUpdate[renderContext.swapchainIndex] = false;
        }
    }

    void Lighting::reallocateDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts{GetEngine().getSwapchainImageCount(), *descriptorSetLayout};
        descriptorSets = GetVulkanDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *descriptorSetPool,
                .descriptorSetCount = GetEngine().getSwapchainImageCount(),
                .pSetLayouts = layouts.data(),
        });
        descriptorNeedsUpdate = std::vector<bool>(descriptorSets.size(), true);
    }

    void Lighting::onSwapchainImageCountChange(size_t newCount) {
        reallocateDescriptorSets();
    }

    void Lighting::onSwapchainSizeChange(int newWidth, int newHeight) {
        // no-op
    }

    LightType Light::fromString(std::string_view str) {
        if(_stricmp(str.data(), "point") == 0) {
            return LightType::Point;
        }
        if(_stricmp(str.data(), "directional") == 0) {
            return LightType::Directional;
        }
        if(_stricmp(str.data(), "spot") == 0) {
            return LightType::Spot;
        }
        verify(false, "Unknown light type!");
    }

    const char* Light::nameOf(LightType type) {
        switch(type) {
            case LightType::Point:
                return "Point";

            case LightType::Directional:
                return "Directional";

            case LightType::Spot:
                return "Spot";
        }
        verify(false, "Unknown light type!");
    }
}