#pragma once

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <array>

#include "VulkanCommandManager.hpp"
#include "VulkanRenderer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanBuffer.hpp"
#include "BasicMesher.hpp"
#include "CameraMath.hpp"

namespace NF::Render {

    class VulkanCore {
    private:
        GLFWwindow* window;
        VkInstance instance;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;
        VkQueue graphicsQueue;
        VkSurfaceKHR surface;

        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;

        // --- ÚJ: DEPTH BUFFER VÁLTOZÓK ---
        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkRenderPass renderPass;
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;

        VkCommandBuffer commandBuffer;
        VkCommandPool commandPool;
        VkExtent2D swapChainExtent = {1280, 720};
        VkSemaphore imageAvailableSemaphore;

        const uint32_t WIDTH = 1280;
        const uint32_t HEIGHT = 720;

        MeshData currentMesh;
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

        double lastX = WIDTH / 2.0;
        double lastY = HEIGHT / 2.0;
        float yaw = -90.0f;
        float pitch = 0.0f;
        Vec3 cameraPos = {8.0f, 8.0f, 40.0f};
        Vec3 cameraFront = {0.0f, 0.0f, -1.0f};
        Vec3 cameraUp = {0.0f, 1.0f, 0.0f};
        bool firstMouse = true;
        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        void initWindow() {
            if (!glfwInit()) throw std::runtime_error("GLFW init hiba!");
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            window = glfwCreateWindow(WIDTH, HEIGHT, "NexusForge Titan Engine - Z-BUFFER AKTIV", nullptr, nullptr);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        void createInstance() { /* ... */ VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.apiVersion = VK_API_VERSION_1_2; VkInstanceCreateInfo createInfo{}; createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; createInfo.pApplicationInfo = &appInfo; uint32_t count; const char** exts = glfwGetRequiredInstanceExtensions(&count); createInfo.enabledExtensionCount = count; createInfo.ppEnabledExtensionNames = exts; if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("Instance hiba!"); }
        void createSurface() { if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) throw std::runtime_error("Surface hiba!"); }
        void pickPhysicalDevice() { uint32_t count = 0; vkEnumeratePhysicalDevices(instance, &count, nullptr); std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance, &count, devices.data()); physicalDevice = devices[0]; }
        void createLogicalDevice() { float prio = 1.0f; VkDeviceQueueCreateInfo qInfo{}; qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qInfo.queueFamilyIndex = 0; qInfo.queueCount = 1; qInfo.pQueuePriorities = &prio; VkDeviceCreateInfo dInfo{}; dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dInfo.pQueueCreateInfos = &qInfo; dInfo.queueCreateInfoCount = 1; const std::vector<const char*> ext = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; dInfo.enabledExtensionCount = static_cast<uint32_t>(ext.size()); dInfo.ppEnabledExtensionNames = ext.data(); if (vkCreateDevice(physicalDevice, &dInfo, nullptr, &device) != VK_SUCCESS) throw std::runtime_error("Device hiba!"); vkGetDeviceQueue(device, 0, 0, &graphicsQueue); }
        void createSwapChain() { VkSwapchainCreateInfoKHR createInfo{}; createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; createInfo.surface = surface; createInfo.minImageCount = 2; createInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB; createInfo.imageExtent = swapChainExtent; createInfo.imageArrayLayers = 1; createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) throw std::runtime_error("Swapchain hiba!"); uint32_t imageCount; vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr); swapChainImages.resize(imageCount); vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data()); }

        void createImageViews() {
            swapChainImageViews.resize(swapChainImages.size());
            for (size_t i = 0; i < swapChainImages.size(); i++) {
                VkImageViewCreateInfo viewInfo{}; viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; viewInfo.image = swapChainImages[i]; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB; viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
                vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]);
            }
        }

        void createRenderPass() {
            // A legáltalánosabban támogatott 32 bites Depth formátumot használjuk
            renderPass = NF::Render::VulkanPipeline::CreateRenderPass(device, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_D32_SFLOAT);
        }

        // --- ÚJ: Mélység Memória Lefoglalása ---
        void createDepthResources() {
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

            // 1. A VRAM lefoglalása
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = swapChainExtent.width;
            imageInfo.extent.height = swapChainExtent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = depthFormat;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateImage(device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) throw std::runtime_error("Hiba a Depth Image letrehozasakor!");

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(device, depthImage, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = VulkanBuffer::FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) throw std::runtime_error("Hiba a Depth memoria foglalasakor!");
            vkBindImageMemory(device, depthImage, depthImageMemory, 0);

            // 2. A Nézet (View) létrehozása a GPU-nak
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = depthImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = depthFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) throw std::runtime_error("Hiba a Depth View letrehozasakor!");
        }

        void createFramebuffers() {
            swapChainFramebuffers.resize(swapChainImageViews.size());
            for (size_t i = 0; i < swapChainImageViews.size(); i++) {
                // A Framebuffer ezentúl 2 képet vár: A Színt és a Távolságot (Depth)
                std::array<VkImageView, 2> attachments = {
                    swapChainImageViews[i],
                    depthImageView
                };

                VkFramebufferCreateInfo fbInfo{}; fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; fbInfo.renderPass = renderPass;
                fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                fbInfo.pAttachments = attachments.data();
                fbInfo.width = swapChainExtent.width; fbInfo.height = swapChainExtent.height; fbInfo.layers = 1;
                vkCreateFramebuffer(device, &fbInfo, nullptr, &swapChainFramebuffers[i]);
            }
        }

        void createGraphicsPipeline() { auto vertCode = NF::Render::VulkanRenderer::ReadFile("vert.spv"); auto fragCode = NF::Render::VulkanRenderer::ReadFile("frag.spv"); VkShaderModule vertShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, vertCode); VkShaderModule fragShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, fragCode); graphicsPipeline = NF::Render::VulkanRenderer::CreateGraphicsPipeline(device, swapChainExtent, renderPass, pipelineLayout, vertShaderModule, fragShaderModule); vkDestroyShaderModule(device, fragShaderModule, nullptr); vkDestroyShaderModule(device, vertShaderModule, nullptr); }
        void createCommandPool() { commandPool = NF::Render::CommandManager::CreateCommandPool(device, 0); }
        void createCommandBuffers() { commandBuffer = NF::Render::CommandManager::CreateCommandBuffer(device, commandPool); }
        void createSyncObjects() { VkSemaphoreCreateInfo sInfo{}; sInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO; vkCreateSemaphore(device, &sInfo, nullptr, &imageAvailableSemaphore); }
        void createVertexAndIndexBuffers() { if (currentMesh.vertices.empty()) return; VkDeviceSize vertexBufferSize = sizeof(currentMesh.vertices[0]) * currentMesh.vertices.size(); VulkanBuffer::CreateBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory); void* vertexData; vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vertexData); memcpy(vertexData, currentMesh.vertices.data(), (size_t)vertexBufferSize); vkUnmapMemory(device, vertexBufferMemory); VkDeviceSize indexBufferSize = sizeof(currentMesh.indices[0]) * currentMesh.indices.size(); VulkanBuffer::CreateBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBuffer, indexBufferMemory); void* indexData; vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &indexData); memcpy(indexData, currentMesh.indices.data(), (size_t)indexBufferSize); vkUnmapMemory(device, indexBufferMemory); }

        void initVulkan() {
            createInstance(); createSurface(); pickPhysicalDevice(); createLogicalDevice(); createSwapChain(); createImageViews();
            createRenderPass();
            createDepthResources(); // <-- Létrehozzuk a láthatatlan mélységtérképet
            createFramebuffers();   // <-- Belerakjuk a keretbe
            createGraphicsPipeline(); createCommandPool(); createCommandBuffers(); createSyncObjects();
            createVertexAndIndexBuffers();
            std::cout << "[Vulkan] Motor inicializalva, Z-Buffer ON!" << std::endl;
        }

        void processInput() {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
            float cameraSpeed = 60.0f * deltaTime; // 20-ról 60-ra emelve, hogy haladjunk is!
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos = cameraPos + cameraFront * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos = cameraPos - cameraFront * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos = cameraPos - cameraFront.cross(cameraUp).normalize() * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos = cameraPos + cameraFront.cross(cameraUp).normalize() * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos = cameraPos + cameraUp * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cameraPos = cameraPos - cameraUp * cameraSpeed;

            double xpos, ypos; glfwGetCursorPos(window, &xpos, &ypos);
            if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
            float xoffset = xpos - lastX; float yoffset = lastY - ypos; lastX = xpos; lastY = ypos;
            float sensitivity = 0.1f; xoffset *= sensitivity; yoffset *= sensitivity;
            yaw += xoffset; pitch += yoffset;
            if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;
            Vec3 front; front.x = cos(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0)); front.y = sin(pitch * (M_PI/180.0)); front.z = sin(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0));
            cameraFront = front.normalize();
        }

    public:
        void loadMesh(const MeshData& mesh) { currentMesh = mesh; }

        void run() {
            initWindow(); initVulkan(); mainLoop(); cleanup();
        }

        void mainLoop() {
            while (!glfwWindowShouldClose(window)) {
                float currentFrame = glfwGetTime(); deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;
                glfwPollEvents(); processInput(); drawFrame();
            }
        }

        void cleanup() {
            vkDeviceWaitIdle(device);

            // ÚJ: Töröljük a Z-Buffert kilépéskor
            vkDestroyImageView(device, depthImageView, nullptr);
            vkDestroyImage(device, depthImage, nullptr);
            vkFreeMemory(device, depthImageMemory, nullptr);

            if (vertexBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, vertexBuffer, nullptr); vkFreeMemory(device, vertexBufferMemory, nullptr); vkDestroyBuffer(device, indexBuffer, nullptr); vkFreeMemory(device, indexBufferMemory, nullptr); }
            vkDestroyPipeline(device, graphicsPipeline, nullptr); vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            for(auto fb : swapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr); for(auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
            vkDestroySwapchainKHR(device, swapChain, nullptr); vkDestroySemaphore(device, imageAvailableSemaphore, nullptr); vkDestroyCommandPool(device, commandPool, nullptr); vkDestroyRenderPass(device, renderPass, nullptr); vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr); glfwDestroyWindow(window); glfwTerminate();
        }

        void drawFrame() {
            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            vkResetCommandBuffer(commandBuffer, 0);
            CommandManager::BeginCommandBuffer(commandBuffer);

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
            renderPassInfo.renderArea.extent = swapChainExtent;

            // --- ÚJ: A Törlés (Clear) most már 2 dolgot végez ---
            // 1. Törli a színt (Égszínkékre)
            // 2. Törli a távolságot (1.0f a maximum távolság)


            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = VkClearColorValue{0.05f, 0.6f, 0.8f, 1.0f};
            clearValues[1].depthStencil = VkClearDepthStencilValue{1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            if (!currentMesh.vertices.empty()) {
                VkBuffer vertexBuffers[] = {vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                Mat4 view = Mat4::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
                Mat4 proj = Mat4::perspective(45.0f * (M_PI / 180.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
                Mat4 mvp = proj * view;

                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &mvp);

                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(currentMesh.indices.size()), 1, 0, 0, 0);
            }

            vkCmdEndRenderPass(commandBuffer);
            CommandManager::EndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &commandBuffer;
            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

            VkPresentInfoKHR presentInfo{}; presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR; presentInfo.swapchainCount = 1; presentInfo.pSwapchains = &swapChain; presentInfo.pImageIndices = &imageIndex;
            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }
    };
}