//
// Created by jglrxavpok on 16/04/2021.
//

#pragma once

#include "engine/Engine.h"
#include "engine/render/InstanceData.h"

namespace Carrot {
    class AnimatedInstances {
    private:
        std::size_t maxInstanceCount = 0;
        Carrot::Engine& engine;
        std::shared_ptr<Model> model = nullptr;
        std::unique_ptr<Buffer> fullySkinnedUnitVertices = nullptr;
        std::unique_ptr<Buffer> flatVertices = nullptr;
        std::map<MeshID, std::shared_ptr<Buffer>> indirectBuffers{};
        AnimatedInstanceData* animatedInstances = nullptr;
        std::unique_ptr<Buffer> instanceBuffer = nullptr;

        std::unordered_map<MeshID, size_t> meshOffsets{};
        std::size_t vertexCountPerInstance = 0;

        std::vector<vk::UniqueDescriptorPool> computeDescriptorPools{};
        std::vector<vk::DescriptorSet> computeDescriptorSet0{};
        std::vector<vk::DescriptorSet> computeDescriptorSet1{};
        vk::UniqueDescriptorSetLayout computeSetLayout0{};
        vk::UniqueDescriptorSetLayout computeSetLayout1{};
        vk::UniquePipelineLayout computePipelineLayout{};
        vk::UniquePipeline computePipeline{};
        std::vector<vk::CommandBuffer> skinningCommandBuffers{};
        std::vector<vk::UniqueSemaphore> skinningSemaphores{};

        void createSkinningComputePipeline();

    public:
        explicit AnimatedInstances(Carrot::Engine& engine, std::shared_ptr<Model> animatedModel, std::size_t maxInstanceCount);

    /// Getters
        Model& getModel() { return *model; };

        AnimatedInstanceData* getInstancePtr() { return animatedInstances; };

        AnimatedInstanceData& getInstance(std::size_t index) {
            assert(index < maxInstanceCount);
            return animatedInstances[index];
        };

        inline vk::DeviceSize getVertexOffset(std::size_t instanceIndex, MeshID meshID) {
            return static_cast<std::int32_t>(instanceIndex * vertexCountPerInstance + meshOffsets[meshID]);
        }

        /**
         * Buffer containing all vertices for all instances after skinning
         */
        Buffer& getFullySkinnedBuffer() { return *fullySkinnedUnitVertices; };

        vk::Semaphore& getSkinningSemaphore(std::size_t frameIndex) { return *skinningSemaphores[frameIndex]; };

#pragma region RenderingUpdate
        vk::Semaphore& onFrame(std::size_t frameIndex);

        void recordGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, std::size_t instanceCount);
#pragma endregion RenderingUpdate
    };
}