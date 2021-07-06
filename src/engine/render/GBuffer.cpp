//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"

Carrot::GBuffer::GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer): renderer(renderer), raytracer(raytracer) {
    screenQuadMesh = make_unique<Mesh>(renderer.getVulkanDriver(),
                                       vector<ScreenSpaceVertex>{
                                               { { -1, -1} },
                                               { { 1, -1} },
                                               { { 1, 1} },
                                               { { -1, 1} },
                                       },
                                       vector<uint32_t>{
                                               2,1,0,
                                               3,2,0,
                                       });
}

void Carrot::GBuffer::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::GBuffer::onSwapchainSizeChange(int newWidth, int newHeight) {
    // TODO
}

Carrot::Render::Pass<Carrot::Render::PassData::GBuffer>& Carrot::GBuffer::addGBufferPass(Carrot::Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Carrot::Render::FrameData&, vk::CommandBuffer&)> callback) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
    vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
            .depth = 1.0f,
            .stencil = 0
    };
    vk::ClearValue clearIntProperties = vk::ClearColorValue();
    auto& swapchainExtent = renderer.getVulkanDriver().getSwapchainExtent();
    return graph.addPass<Carrot::Render::PassData::GBuffer>("gbuffer",
           [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GBuffer>& pass, Carrot::Render::PassData::GBuffer& data)
           {

                data.albedo = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                       vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                       vk::AttachmentLoadOp::eClear,
                                                       clearColor,
                                                       vk::ImageLayout::eColorAttachmentOptimal);

                data.depthStencil = graph.createRenderTarget(renderer.getVulkanDriver().getDepthFormat(),
                                                             vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                             vk::AttachmentLoadOp::eClear,
                                                             clearDepth,
                                                             vk::ImageLayout::eDepthStencilAttachmentOptimal);

                data.positions = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                          vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                          vk::AttachmentLoadOp::eClear,
                                                          positionClear,
                                                          vk::ImageLayout::eColorAttachmentOptimal);

                data.normals = graph.createRenderTarget(vk::Format::eR32G32B32A32Sfloat,
                                                        vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                        vk::AttachmentLoadOp::eClear,
                                                        positionClear,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

                data.flags = graph.createRenderTarget(vk::Format::eR32Uint,
                                                      vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                      vk::AttachmentLoadOp::eClear,
                                                      clearIntProperties,
                                                      vk::ImageLayout::eColorAttachmentOptimal);
           },
           [callback](const Render::CompiledPass& pass, const Render::FrameData& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& buffer){
                callback(pass, frame, buffer);
           }
    );
}

Carrot::Render::Pass<Carrot::Render::PassData::GResolve>& Carrot::GBuffer::addGResolvePass(const Carrot::Render::PassData::GBuffer& data,
                                                                                      const Carrot::Render::PassData::Raytracing& rtData,
                                                                                      const Carrot::Render::PassData::ImGui& imguiData,
                                                                                      const Carrot::Render::FrameResource& skyboxOutput,
                                                                                      Carrot::Render::GraphBuilder& graph) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,1.0f});
    auto& swapchainExtent = renderer.getVulkanDriver().getSwapchainExtent();
    return graph.addPass<Carrot::Render::PassData::GResolve>("gresolve",
           [&, skyboxOutput = skyboxOutput](GraphBuilder& graph, Pass<Carrot::Render::PassData::GResolve>& pass, Carrot::Render::PassData::GResolve& resolveData)
           {
                resolveData.positions = graph.read(data.positions, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.normals = graph.read(data.normals, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.albedo = graph.read(data.albedo, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.depthStencil = graph.read(data.depthStencil, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
                resolveData.flags = graph.read(data.flags, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.raytracing = graph.read(rtData.output, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.ui = graph.read(imguiData.output, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.skybox = graph.read(skyboxOutput, vk::ImageLayout::eShaderReadOnlyOptimal);
                resolveData.resolved = graph.createRenderTarget(vk::Format::eR8G8B8A8Unorm,
                                                        vk::Extent3D{swapchainExtent.width, swapchainExtent.height, 1},
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearColor,
                                                        vk::ImageLayout::eColorAttachmentOptimal);
           },
           [this](const Render::CompiledPass& pass, const Render::FrameData& frame, const Carrot::Render::PassData::GResolve& data, vk::CommandBuffer& buffer) {
                auto resolvePipeline = renderer.getOrCreatePipeline("gResolve-rendergraph");
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.albedo, frame.frameIndex), 0, 0, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.depthStencil, frame.frameIndex), 0, 1, nullptr, vk::ImageAspectFlagBits::eDepth);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.positions, frame.frameIndex), 0, 2, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.normals, frame.frameIndex), 0, 3, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.flags, frame.frameIndex), 0, 4, renderer.getVulkanDriver().getNearestSampler());
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.raytracing, frame.frameIndex), 0, 5, nullptr);
                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.ui, frame.frameIndex), 0, 6, nullptr);

                renderer.bindTexture(*resolvePipeline, frame, pass.getGraph().getTexture(data.skybox, frame.frameIndex), 0, 10, nullptr);

                resolvePipeline->bind(pass.getRenderPass(), frame.frameIndex, buffer);
                screenQuadMesh->bind(buffer);
                screenQuadMesh->draw(buffer);
           }
    );
}
