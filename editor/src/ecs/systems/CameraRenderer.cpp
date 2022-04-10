//
// Created by jglrxavpok on 20/02/2022.
//

#include "CameraRenderer.h"
#include <engine/render/DrawData.h>

namespace Peeler::ECS {
    CameraRenderer::CameraRenderer(Carrot::ECS::World& world): Carrot::ECS::RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::CameraComponent>(world) {
        cameraModel = Carrot::AsyncModelResource(GetRenderer().coloadModel("resources/models/camera.obj"));
        primaryCameraPipeline = GetRenderer().getOrCreatePipeline("gBuffer");
        secondaryCameraPipeline = GetRenderer().getOrCreatePipeline("gBufferWireframe");
    }

    void CameraRenderer::onFrame(Carrot::Render::Context renderContext) {
        Carrot::Render::Packet& packet = GetRenderer().makeRenderPacket(Carrot::Render::PassEnum::OpaqueGBuffer, renderContext.viewport);
        packet.useMesh(*cameraModel->getStaticMeshes()[0]);

        static glm::mat4 scaling = glm::scale(glm::mat4{ 1.0f }, glm::vec3 { 0.1f, 0.1f, 0.1f });

        Carrot::InstanceData instanceData;
        Carrot::DrawData data;

        Carrot::Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
        pushConstant.id = "drawDataPush";
        pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

        data.materialIndex = GetRenderer().getWhiteMaterial().getSlot();
        pushConstant.setData(data);

        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::TransformComponent& transform, Carrot::ECS::CameraComponent& cameraComponent) {
            instanceData.uuid = entity.getID();
            instanceData.transform = transform.toTransformMatrix() * scaling;
            packet.useInstance(instanceData);

            packet.pipeline = cameraComponent.isPrimary ? primaryCameraPipeline : secondaryCameraPipeline;

            GetRenderer().render(packet);
        });
    }

    std::unique_ptr<Carrot::ECS::System> CameraRenderer::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<CameraRenderer>(newOwner);
    }
}