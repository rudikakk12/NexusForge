#pragma once

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>

// A saját moduljaid
#include "VulkanCommandManager.hpp"
#include "VulkanRenderer.hpp"
#include "VulkanPipeline.hpp"

namespace NF::Render {

    class VulkanCore {
    private:
        // --- GPU Erőforrások ---
        GLFWwindow* window;
        VkInstance instance;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;
        VkQueue graphicsQueue;
        VkSurfaceKHR surface;

        // --- Rendereléshez szükséges tagváltozók ---
        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;

        VkRenderPass renderPass;
        VkPipeline graphicsPipeline;
        VkCommandBuffer commandBuffer;
        VkCommandPool commandPool;
        VkExtent2D swapChainExtent = {1280, 720};
        VkSemaphore imageAvailableSemaphore;

        const uint32_t WIDTH = 1280;
        const uint32_t HEIGHT = 720;

        // --- Inicializálási metódusok ---

        void initWindow() {
            if (!glfwInit()) throw std::runtime_error("GLFW init hiba!");
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            window = glfwCreateWindow(WIDTH, HEIGHT, "NexusForge Titan Engine", nullptr, nullptr);
        }

        void createInstance() {
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.apiVersion = VK_API_VERSION_1_2;
            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            uint32_t count;
            const char** exts = glfwGetRequiredInstanceExtensions(&count);
            createInfo.enabledExtensionCount = count;
            createInfo.ppEnabledExtensionNames = exts;
            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("Instance hiba!");
        }

        void createSurface() {
            if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) throw std::runtime_error("Surface hiba!");
        }

        void pickPhysicalDevice() {
            uint32_t count = 0;
            vkEnumeratePhysicalDevices(instance, &count, nullptr);
            std::vector<VkPhysicalDevice> devices(count);
            vkEnumeratePhysicalDevices(instance, &count, devices.data());
            physicalDevice = devices[0];
        }

        void createLogicalDevice() {
            float prio = 1.0f;
            VkDeviceQueueCreateInfo qInfo{};
            qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo.queueFamilyIndex = 0;
            qInfo.queueCount = 1;
            qInfo.pQueuePriorities = &prio;
            VkDeviceCreateInfo dInfo{};
            dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            dInfo.pQueueCreateInfos = &qInfo;
            dInfo.queueCreateInfoCount = 1;
            const std::vector<const char*> ext = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            dInfo.enabledExtensionCount = static_cast<uint32_t>(ext.size());
            dInfo.ppEnabledExtensionNames = ext.data();
            if (vkCreateDevice(physicalDevice, &dInfo, nullptr, &device) != VK_SUCCESS) throw std::runtime_error("Device hiba!");
            vkGetDeviceQueue(device, 0, 0, &graphicsQueue);
        }

        void createSwapChain() {
            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;
            createInfo.minImageCount = 2;
            createInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
            createInfo.imageExtent = swapChainExtent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) throw std::runtime_error("Swapchain hiba!");

            uint32_t imageCount;
            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        }

        void createImageViews() {
            swapChainImageViews.resize(swapChainImages.size());
            for (size_t i = 0; i < swapChainImages.size(); i++) {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = swapChainImages[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.layerCount = 1;
                vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]);
            }
        }

        void createRenderPass() {
            renderPass = NF::Render::VulkanPipeline::CreateRenderPass(device, VK_FORMAT_B8G8R8A8_SRGB);
        }

        void createFramebuffers() {
            swapChainFramebuffers.resize(swapChainImageViews.size());
            for (size_t i = 0; i < swapChainImageViews.size(); i++) {
                VkFramebufferCreateInfo fbInfo{};
                fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                fbInfo.renderPass = renderPass;
                fbInfo.attachmentCount = 1;
                fbInfo.pAttachments = &swapChainImageViews[i];
                fbInfo.width = swapChainExtent.width;
                fbInfo.height = swapChainExtent.height;
                fbInfo.layers = 1;
                vkCreateFramebuffer(device, &fbInfo, nullptr, &swapChainFramebuffers[i]);
            }
        }

        void createCommandPool() {
            commandPool = NF::Render::CommandManager::CreateCommandPool(device, 0);
        }

        void createCommandBuffers() {
            commandBuffer = NF::Render::CommandManager::CreateCommandBuffer(device, commandPool);
        }

        void createSyncObjects() {
            VkSemaphoreCreateInfo sInfo{};
            sInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkCreateSemaphore(device, &sInfo, nullptr, &imageAvailableSemaphore);
        }

        void initVulkan() {
            std::cout << "[Init] Instance creation..." << std::endl;
            createInstance();
            std::cout << "[Init] Surface creation..." << std::endl;
            createSurface();
            std::cout << "[Init] Picking physical device..." << std::endl;
            pickPhysicalDevice();
            std::cout << "[Init] Logical device..." << std::endl;
            createLogicalDevice();
            std::cout << "[Init] Swapchain..." << std::endl;
            createSwapChain();
            std::cout << "[Init] ImageViews..." << std::endl;
            createImageViews();
            std::cout << "[Init] RenderPass..." << std::endl;
            createRenderPass();
            std::cout << "[Init] Framebuffers..." << std::endl;
            createFramebuffers();
            std::cout << "[Init] GraphicsPipeline..." << std::endl;
            createGraphicsPipeline(); // Itt kell lennie a vert.spv-nek!
            std::cout << "[Init] CommandPool..." << std::endl;
            createCommandPool();
            std::cout << "[Init] CommandBuffers..." << std::endl;
            createCommandBuffers();
            std::cout << "[Init] SyncObjects..." << std::endl;
            createSyncObjects();

            std::cout << "[Vulkan] Motor inicializalva!" << std::endl;
        }

        void createGraphicsPipeline() {
            // 1. Shaderek betöltése
            auto vertCode = NF::Render::VulkanRenderer::ReadFile("Shaders/vert.spv");
            auto fragCode = NF::Render::VulkanRenderer::ReadFile("Shaders/frag.spv");

            VkShaderModule vertShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, vertCode);
            VkShaderModule fragShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, fragCode);

            // 2. Pipeline struktúra
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            VkPipelineLayout pipelineLayout;
            vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

            // Itt használjuk a korábban írt renderer-t (ez építi fel a GPU gyárat)
            graphicsPipeline = NF::Render::VulkanRenderer::CreateGraphicsPipeline(
                device, renderPass, pipelineLayout, vertShaderModule, fragShaderModule
            );

            // 3. Takarítás (a modulok már nem kellenek, a pipeline-ban benne vannak)
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
        }

    public:
        void run() {
            initWindow();
            initVulkan();
            mainLoop();
            cleanup();
        }

        void mainLoop() {
            std::cout << "[Engine] Belepes a fohurokba..." << std::endl;
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                drawFrame();
            }
        }

        void cleanup() {
            vkDeviceWaitIdle(device);
            vkDestroyPipeline(device, graphicsPipeline, nullptr); // ÚJ SOR
            for(auto fb : swapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
            for(auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
            vkDestroySwapchainKHR(device, swapChain, nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            vkDestroyCommandPool(device, commandPool, nullptr);
            vkDestroyRenderPass(device, renderPass, nullptr);
            vkDestroyDevice(device, nullptr);
            vkDestroyInstance(instance, nullptr);
            glfwDestroyWindow(window);
            glfwTerminate();
        }

        void drawFrame() {
            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            vkResetCommandBuffer(commandBuffer, 0);
            CommandManager::BeginCommandBuffer(commandBuffer);

            VkClearValue clearColor = {{{0.1f, 0.1f, 0.15f, 1.0f}}}; // Sötétkék/Szürke háttér

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
            renderPassInfo.renderArea.extent = swapChainExtent;

            // A LÉNYEG: Itt állítjuk be, hogy mivel törölje ki a képet
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            // Itt a pipeline végre éles!
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            // Itt rajzolunk (3 vertex, 1 háromszög)
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(commandBuffer);

            CommandManager::EndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapChain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }
    };
}