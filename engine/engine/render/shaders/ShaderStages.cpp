//
// Created by jglrxavpok on 30/11/2020.
//

#include "ShaderStages.h"

Carrot::ShaderStages::ShaderStages(Carrot::VulkanDriver& driver, const std::vector<std::string>& filenames): driver(driver) {
    for(const auto& filename: filenames) {
        auto stage = static_cast<vk::ShaderStageFlagBits>(0);
        if(filename.ends_with(".vertex.glsl.spv")) {
            stage = vk::ShaderStageFlagBits::eVertex;
        }
        if(filename.ends_with(".fragment.glsl.spv")) {
            stage = vk::ShaderStageFlagBits::eFragment;
        }
        if(filename.ends_with(".rchit.spv")) {
            stage = vk::ShaderStageFlagBits::eClosestHitKHR;
        }
        if(filename.ends_with(".rmiss.spv")) {
            stage = vk::ShaderStageFlagBits::eMissKHR;
        }
        if(filename.ends_with(".rgen.spv")) {
            stage = vk::ShaderStageFlagBits::eRaygenKHR;
        }
        stages.emplace_back(std::make_pair(stage, std::move(std::make_unique<ShaderModule>(driver, filename))));
    }
}

Carrot::ShaderStages::ShaderStages(Carrot::VulkanDriver& driver, const std::vector<Carrot::IO::Resource>& resources, const std::vector<vk::ShaderStageFlagBits>& stages): driver(driver) {
    for(size_t i = 0; i < resources.size(); i++) {
        this->stages.emplace_back(std::make_pair(stages[i], std::move(std::make_unique<ShaderModule>(driver, resources[i]))));
    }
}

std::vector<vk::PipelineShaderStageCreateInfo> Carrot::ShaderStages::createPipelineShaderStages(const vk::SpecializationInfo* specialization) const {
    std::vector<vk::PipelineShaderStageCreateInfo> creates{};
    for(const auto& [stage, module] : stages) {
        creates.push_back(module->createPipelineShaderStage(stage, specialization));
    }
    return creates;
}

vk::UniqueDescriptorSetLayout Carrot::ShaderStages::createDescriptorSetLayout0(const std::map<std::string, std::uint32_t>& constants) const {
    auto& device = driver.getLogicalDevice();

    std::vector<NamedBinding> bindings{};

    for(const auto& [stage, module] : stages) {
        module->addBindingsSet0(stage, bindings, constants);
    }

    std::vector<vk::DescriptorSetLayoutBinding> vkBindings{bindings.size()};
    for (int i = 0; i < vkBindings.size(); ++i) {
        vkBindings[i] = bindings[i].vkBinding;
    }
    vk::DescriptorSetLayoutCreateInfo createInfo{
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = vkBindings.data(),
    };

    return device.createDescriptorSetLayoutUnique(createInfo, driver.getAllocationCallbacks());
}

const std::vector<std::pair<vk::ShaderStageFlagBits, std::unique_ptr<Carrot::ShaderModule>>>& Carrot::ShaderStages::getModuleMap() const {
    return stages;
}