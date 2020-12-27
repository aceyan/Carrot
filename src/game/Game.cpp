//
// Created by jglrxavpok on 05/12/2020.
//

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <memory>
#include "Game.h"
#include "Engine.h"
#include "render/Model.h"
#include "render/Buffer.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include <iostream>
#include <render/shaders/ShaderModule.h>

int maxInstanceCount = 100; // TODO: change

Carrot::Game::Game(Carrot::Engine& engine): engine(engine) {
    mapModel = make_unique<Model>(engine, "resources/models/map/map.obj");
    model = make_unique<Model>(engine, "resources/models/unit.fbx");

    int groupSize = maxInstanceCount /3;
    instanceBuffer = make_unique<Buffer>(engine,
                                         maxInstanceCount*sizeof(AnimatedInstanceData),
                                         vk::BufferUsageFlagBits::eVertexBuffer,
                                         vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    modelInstance = instanceBuffer->map<AnimatedInstanceData>();

    mapInstanceBuffer = make_unique<Buffer>(engine,
                                            sizeof(InstanceData),
                                            vk::BufferUsageFlagBits::eVertexBuffer,
                                            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    auto* mapData = mapInstanceBuffer->map<InstanceData>();
    mapData[0] = {
            {1,1,1,1},
            glm::mat4(1.0f)
    };

    // TODO: abstract
    map<MeshID, vector<vk::DrawIndexedIndirectCommand>> indirectCommands{};
    auto meshes = model->getMeshes();
    uint64_t maxVertexCount = 0;
    vector<uint32_t> meshSizes{};
    size_t vertexCountPerInstance = 0;
    map<MeshID, size_t> meshOffsets{};
    for(const auto& mesh : meshes) {
        indirectBuffers[mesh->getMeshID()] = make_shared<Buffer>(engine,
                                             maxInstanceCount * sizeof(vk::DrawIndexedIndirectCommand),
                                             vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                             vk::MemoryPropertyFlagBits::eDeviceLocal);
        size_t meshSize = mesh->getVertexCount();
        maxVertexCount = max(meshSize, maxVertexCount);
        meshSizes.push_back(static_cast<uint32_t>(meshSize));

        meshOffsets[mesh->getMeshID()] = vertexCountPerInstance;
        vertexCountPerInstance += meshSize;
    }

    fullySkinnedUnitVertices = make_unique<Buffer>(engine,
                                                   sizeof(SkinnedVertex) * vertexCountPerInstance * maxInstanceCount,
                                                   vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                   vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                   engine.createGraphicsAndTransferFamiliesSet());

    const float spacing = 0.5f;
    for(int i = 0; i < maxInstanceCount; i++) {
        float x = (i % (int)sqrt(maxInstanceCount)) * spacing;
        float y = (i / (int)sqrt(maxInstanceCount)) * spacing;
        auto position = glm::vec3(x, y, 0);
        auto color = static_cast<Unit::Type>((i / max(1, groupSize)) % 3);
        auto unit = make_unique<Unit>(color, modelInstance[i]);
        unit->teleport(position);
        units.emplace_back(move(unit));

        for(const auto& mesh : meshes) {
            int32_t vertexOffset = static_cast<int32_t>(i * vertexCountPerInstance + meshOffsets[mesh->getMeshID()]);

            // TODO: tmp, remove
            //  copies original vertices to fullySkinnedUnitVertices
            engine.performSingleTimeTransferCommands([&](vk::CommandBuffer& commands) {
                vk::BufferCopy region {
                        .srcOffset = mesh->getVertexStartOffset(),
                        .dstOffset = vertexOffset*sizeof(SkinnedVertex),
                        .size = mesh->getVertexCount()*sizeof(SkinnedVertex),
                };
                commands.copyBuffer(mesh->getBackingBuffer().getVulkanBuffer(), fullySkinnedUnitVertices->getVulkanBuffer(), region);
            });

            indirectCommands[mesh->getMeshID()].push_back(vk::DrawIndexedIndirectCommand {
                    .indexCount = static_cast<uint32_t>(mesh->getIndexCount()),
                    .instanceCount = 1,
                    .firstIndex = 0,
                    .vertexOffset = vertexOffset,
                    .firstInstance = static_cast<uint32_t>(i),
            });
        }
    }
    for(const auto& mesh : meshes) {
        indirectBuffers[mesh->getMeshID()]->stageUploadWithOffsets(make_pair(static_cast<uint64_t>(0),
                                                                             indirectCommands[mesh->getMeshID()]));
    }

    createSkinningComputePipeline(meshSizes, maxVertexCount);
}

void Carrot::Game::createSkinningComputePipeline(const vector<uint32_t>& meshSizes, uint64_t maxVertexCount) {
    auto& computeCommandPool = engine.getComputeCommandPool();

    // command buffers which will be sent to the compute queue to compute skinning
    skinningCommandBuffers = engine.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = computeCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = engine.getSwapchainImageCount(),
    });

    // sets the mesh count (per instance) for this skinning pipeline
    vk::SpecializationMapEntry specEntries[] = {
            {
                .constantID = 0,
                .offset = 0,
                .size = sizeof(uint32_t),
            },

            {
                .constantID = 1,
                .offset = 1*sizeof(uint32_t),
                .size = sizeof(uint32_t),
            },
    };

    uint32_t specData[] = {
            static_cast<uint32_t>(meshSizes.size()),
            static_cast<uint32_t>(maxInstanceCount),
    };
    vk::SpecializationInfo specialization {
        .mapEntryCount = 2,
        .pMapEntries = specEntries,

        .dataSize = 2*sizeof(uint32_t),
        .pData = specData,
    };

    auto computeStage = ShaderModule(engine, "resources/shaders/skinning.compute.glsl.spv");

    vk::PushConstantRange pushConstant {
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = static_cast<uint32_t>(sizeof(uint32_t)*meshSizes.size()),
    };
    // create the pipeline
    computePipelineLayout = engine.getLogicalDevice().createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
        .setLayoutCount = 0, // TODO
        .pSetLayouts = nullptr, // TODO
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant,
    }, engine.getAllocator());
    computePipeline = engine.getLogicalDevice().createComputePipelineUnique(nullptr, vk::ComputePipelineCreateInfo {
        .stage = computeStage.createPipelineShaderStage(vk::ShaderStageFlagBits::eCompute, &specialization),
        .layout = *computePipelineLayout,
    }, engine.getAllocator());

    uint32_t vertexGroups = (maxVertexCount + 63)/64;
    uint32_t instanceGroups = (maxInstanceCount + 7)/8;
    uint32_t meshGroups = (meshSizes.size() + 1)/2;
    for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
        vk::CommandBuffer& commands = skinningCommandBuffers[i];
        commands.begin(vk::CommandBufferBeginInfo {
        });
        {
            // TODO: tracy zone
            commands.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
            commands.pushConstants(*computePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, meshSizes.size()*sizeof(uint32_t), meshSizes.data());
            commands.dispatch(vertexGroups, instanceGroups, meshGroups);
        }
        commands.end();

        skinningSemaphores.emplace_back(move(engine.getLogicalDevice().createSemaphoreUnique({})));
    }

    // TODO: create flat buffer (merge all meshes) for skinning input
    // TODO: create output buffer
}

void Carrot::Game::onFrame(uint32_t frameIndex) {
    ZoneScoped;
/*    map<Unit::Type, glm::vec3> centers{};
    map<Unit::Type, uint32_t> counts{};

    for(const auto& unit : units) {
        glm::vec3& currentCenter = centers[unit->getType()];
        counts[unit->getType()]++;
        currentCenter += unit->getPosition();
    }
    centers[Unit::Type::Red] /= counts[Unit::Type::Red];
    centers[Unit::Type::Green] /= counts[Unit::Type::Green];
    centers[Unit::Type::Blue] /= counts[Unit::Type::Blue];
*/
    static double lastTime = glfwGetTime();
    float dt = static_cast<float>(glfwGetTime() - lastTime);
    for(const auto& unit : units) {
        ZoneScoped;
  //      unit->moveTo(centers[unit->getType()]);
        unit->update(dt);
    }
   // TracyPlot("onFrame delta time", dt*1000);
    lastTime = glfwGetTime();

    // TODO: optimize
    // submit skinning command buffer
    engine.getComputeQueue().submit(vk::SubmitInfo {
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &skinningCommandBuffers[frameIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &(*skinningSemaphores[frameIndex]),
    });
}

void Carrot::Game::recordCommandBuffer(uint32_t frameIndex, vk::CommandBuffer& commands) {
    {
        TracyVulkanZone(*engine.tracyCtx[frameIndex], commands, "Render map");
        mapModel->draw(frameIndex, commands, *mapInstanceBuffer, 1);
    }

    {
        TracyVulkanZone(*engine.tracyCtx[frameIndex], commands, "Render units");
        const int indirectDrawCount = maxInstanceCount; // TODO: Not all units will be always on the field, + visibility culling?
        commands.bindVertexBuffers(0, fullySkinnedUnitVertices->getVulkanBuffer(), {0});
        model->indirectDraw(frameIndex, commands, *instanceBuffer, indirectBuffers, indirectDrawCount);
    }
}

float yaw = 0.0f;
float pitch = 0.0f;

void Carrot::Game::onMouseMove(double dx, double dy) {
    auto& camera = engine.getCamera();
    yaw -= dx * 0.01f;
    pitch -= dy * 0.01f;

    const float distanceFromCenter = 5.0f;
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);

    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    camera.position = glm::vec3{cosYaw * cosPitch, sinYaw * cosPitch, sinPitch} * distanceFromCenter;
    camera.position += camera.target;
}

void Carrot::Game::changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) {
    semaphores.emplace_back(*skinningSemaphores[frameIndex]);
    waitStages.emplace_back(vk::PipelineStageFlagBits::eVertexInput);
}

