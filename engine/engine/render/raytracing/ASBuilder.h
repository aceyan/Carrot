//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "engine/Engine.h"
#include "AccelerationStructure.h"
#include <glm/matrix.hpp>

namespace Carrot {
    struct GeometryInput {
        std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges{};

        std::unique_ptr<AccelerationStructure> as{};

        // cached structures for rebuilding
        std::unique_ptr<Buffer> scratchBuffer{};
        std::unique_ptr<vk::AccelerationStructureBuildGeometryInfoKHR> cachedBuildInfo{};
        std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> cachedBuildRanges{};
    };

    struct InstanceInput {
        glm::mat4 transform{1.0f};
        std::uint32_t customInstanceIndex;
        std::uint32_t geometryIndex;

        std::uint32_t mask;
        std::uint32_t hitGroup;
    };

    struct TLAS {
        std::unique_ptr<AccelerationStructure> as{};
    };

    /// Helpers build Acceleration Structures for raytracing
    class ASBuilder {
    public:
        explicit ASBuilder(VulkanRenderer& renderer);

        // TODO: mesh registration

        void addInstance(const InstanceInput input);

        void buildBottomLevelAS(bool enableUpdate = true);
        void buildTopLevelAS(bool update, bool waitForCompletion = false);

        void updateBottomLevelAS(const std::vector<size_t>& blasIndices, vk::Semaphore skinningSemaphore = {});
        void updateTopLevelAS();

        void startFrame();
        void waitForCompletion(vk::CommandBuffer& cmds);

        TLAS& getTopLevelAS();

        std::vector<InstanceInput>& getTopLevelInstances();

    private:
        VulkanRenderer& renderer;
        bool enabled = false;
        std::vector<GeometryInput> bottomLevelGeometries{};
        std::vector<InstanceInput> topLevelInstances{};
        std::unique_ptr<AccelerationStructure> topLevelAS{};
        std::unique_ptr<Buffer> instancesBuffer{};
        TLAS tlas{};

    private: // reuse between builds
        vk::CommandBuffer tlasBuildCommands{};
        std::size_t lastInstanceCount = 0;
        vk::DeviceAddress instanceBufferAddress = 0;

        std::unique_ptr<Carrot::Buffer> scratchBuffer = nullptr;
        std::size_t lastScratchSize = 0;
        vk::DeviceAddress scratchBufferAddress = 0;

    private:
        std::vector<vk::BufferMemoryBarrier2KHR> bottomLevelBarriers;
        std::vector<vk::BufferMemoryBarrier2KHR> topLevelBarriers;

    private:
        vk::AccelerationStructureInstanceKHR convertToVulkanInstance(const InstanceInput& instance);

    };
}
