//
// Created by jglrxavpok on 13/03/2021.
//

#pragma once

#include <memory>
#include <map>
#include <functional>
#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/resources/Texture.h"
#include "engine/vulkan/SwapchainAware.h"

namespace Carrot {
    class GBuffer;
    class ASBuilder;
    class RayTracer;

    using CommandBufferConsumer = std::function<void(vk::CommandBuffer&)>;

    class VulkanRenderer: public SwapchainAware {
    private:
        VulkanDriver& driver;

        map<string, shared_ptr<Pipeline>> pipelines{};
        map<string, unique_ptr<Render::Texture>> textures{};

        vector<unique_ptr<Render::Texture>> uiTextures{};
        vector<unique_ptr<Render::Texture>> skyboxTextures{};

        vk::UniqueDescriptorPool imguiDescriptorPool{};

        vk::UniqueRenderPass gRenderPass{};
        vk::UniqueRenderPass imguiRenderPass{};
        vk::UniqueRenderPass skyboxRenderPass{};
        vector<vk::UniqueFramebuffer> imguiFramebuffers{};
        vector<vk::UniqueFramebuffer> skyboxFramebuffers{};
        vector<vk::UniqueFramebuffer> swapchainFramebuffers{};

        unique_ptr<RayTracer> raytracer = nullptr;
        unique_ptr<ASBuilder> asBuilder = nullptr;
        unique_ptr<GBuffer> gBuffer = nullptr;

        std::list<CommandBufferConsumer> beforeFrameCommands;
        std::list<CommandBufferConsumer> afterFrameCommands;

        void createUIResources();

        /// Create the pipeline responsible for lighting via the gbuffer
        void createGBuffer();

        /// Create the object responsible of raytracing operations and subpasses
        void createRayTracer();

        void initImgui();

        void createSkyboxRenderPass();

        void createUIImages();

    public:
        explicit VulkanRenderer(VulkanDriver& driver);

        shared_ptr<Pipeline> getOrCreatePipeline(const string& name);

        unique_ptr<Render::Texture>& getOrCreateTexture(const string& textureName);

        void recreateDescriptorPools(size_t frameCount);

        /// Create the render pass
        void createRenderPasses();

        /// Create the framebuffers to render to
        void createFramebuffers();

    public:
        void beforeFrameCommand(const CommandBufferConsumer& command);
        void afterFrameCommand(const CommandBufferConsumer& command);

        void preFrame();
        void postFrame();

    public:
        size_t getSwapchainImageCount() { return driver.getSwapchainImageCount(); };
        VulkanDriver& getVulkanDriver() { return driver; };

        ASBuilder& getASBuilder() { return *asBuilder; };
        RayTracer& getRayTracer() { return *raytracer; };

        GBuffer& getGBuffer() { return *gBuffer; };

        vector<vk::UniqueFramebuffer>& getSwapchainFramebuffers() { return swapchainFramebuffers; };
        vector<vk::UniqueFramebuffer>& getImguiFramebuffers() { return imguiFramebuffers; };
        vector<vk::UniqueFramebuffer>& getSkyboxFramebuffers() { return skyboxFramebuffers; };

        vk::RenderPass& getGRenderPass() { return *gRenderPass; };
        vk::RenderPass& getImguiRenderPass() { return *imguiRenderPass; };
        vk::RenderPass& getSkyboxRenderPass() { return *skyboxRenderPass; };

        vector<unique_ptr<Render::Texture>>& getUITextures() { return uiTextures; };

        vector<unique_ptr<Render::Texture>>& getSkyboxTextures() { return skyboxTextures; };

        vk::Device& getLogicalDevice() { return driver.getLogicalDevice(); };

    public:
        void createSkyboxResources();

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(size_t newCount) override;
    };
}
