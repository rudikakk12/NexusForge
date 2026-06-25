#pragma once

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// --- ÚJ: DEAR IMGUI BACKENDEK ---
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <sstream>

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

        VkImage depthImage;
        VkDeviceMemory depthImageMemory;
        VkImageView depthImageView;

        VkRenderPass renderPass;
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;

        // VULKAN DESCRIPTOR POOL
        // Ez felelős a Textúrák, Samplerek és Uniform Bufferek memóriacímzéséért a GPU felé
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        VkCommandBuffer commandBuffer;
        VkCommandPool commandPool;
        VkExtent2D swapChainExtent = {1280, 720};

        // JAVÍTOTT VULKAN SZINKRONIZÁCIÓ
        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;

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

        // TELEMETRIA ADATOK
        bool telemetryMenuOpen = true;
        uint64_t totalFrames = 0;
        float totalTimeElapsed = 0.0f;
        float bestFrameTime = 999999.0f;
        float worstFrameTime = 0.0f;

        uint64_t menuOpenFrames = 0;
        float menuOpenTimeElapsed = 0.0f;

        std::vector<std::pair<float, float>> frameHistory;

        void initWindow() {
            if (!glfwInit()) throw std::runtime_error("GLFW init hiba!");
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            window = glfwCreateWindow(WIDTH, HEIGHT, "NexusForge Titan Engine - ImGui HUD", nullptr, nullptr);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        void createInstance() { VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.apiVersion = VK_API_VERSION_1_2; VkInstanceCreateInfo createInfo{}; createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; createInfo.pApplicationInfo = &appInfo; uint32_t count; const char** exts = glfwGetRequiredInstanceExtensions(&count); createInfo.enabledExtensionCount = count; createInfo.ppEnabledExtensionNames = exts; if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("Instance hiba!"); }
        void createSurface() { if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) throw std::runtime_error("Surface hiba!"); }
        void pickPhysicalDevice() { uint32_t count = 0; vkEnumeratePhysicalDevices(instance, &count, nullptr); std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance, &count, devices.data()); physicalDevice = devices[0]; }
        void createLogicalDevice() { float prio = 1.0f; VkDeviceQueueCreateInfo qInfo{}; qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qInfo.queueFamilyIndex = 0; qInfo.queueCount = 1; qInfo.pQueuePriorities = &prio; VkDeviceCreateInfo dInfo{}; dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dInfo.pQueueCreateInfos = &qInfo; dInfo.queueCreateInfoCount = 1; const std::vector<const char*> ext = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; dInfo.enabledExtensionCount = static_cast<uint32_t>(ext.size()); dInfo.ppEnabledExtensionNames = ext.data(); if (vkCreateDevice(physicalDevice, &dInfo, nullptr, &device) != VK_SUCCESS) throw std::runtime_error("Device hiba!"); vkGetDeviceQueue(device, 0, 0, &graphicsQueue); }

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
                VkImageViewCreateInfo viewInfo{}; viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; viewInfo.image = swapChainImages[i]; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB; viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
                vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]);
            }
        }

        void createRenderPass() {
            renderPass = NF::Render::VulkanPipeline::CreateRenderPass(device, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_D32_SFLOAT);
        }

        void createDepthResources() {
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
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
                std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };
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

        void createSyncObjects() {
            VkSemaphoreCreateInfo sInfo{}; sInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkFenceCreateInfo fInfo{}; fInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            if (vkCreateSemaphore(device, &sInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS || vkCreateSemaphore(device, &sInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS || vkCreateFence(device, &fInfo, nullptr, &inFlightFence) != VK_SUCCESS) throw std::runtime_error("Szinkronizacios hiba!");
        }

        void createVertexAndIndexBuffers() {
            if (currentMesh.vertices.empty()) return;
            VkDeviceSize vertexBufferSize = sizeof(currentMesh.vertices[0]) * currentMesh.vertices.size();
            VulkanBuffer::CreateBuffer(device, physicalDevice, vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory);
            void* vertexData; vkMapMemory(device, vertexBufferMemory, 0, vertexBufferSize, 0, &vertexData); memcpy(vertexData, currentMesh.vertices.data(), (size_t)vertexBufferSize); vkUnmapMemory(device, vertexBufferMemory);

            VkDeviceSize indexBufferSize = sizeof(currentMesh.indices[0]) * currentMesh.indices.size();
            VulkanBuffer::CreateBuffer(device, physicalDevice, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexBuffer, indexBufferMemory);
            void* indexData; vkMapMemory(device, indexBufferMemory, 0, indexBufferSize, 0, &indexData); memcpy(indexData, currentMesh.indices.data(), (size_t)indexBufferSize); vkUnmapMemory(device, indexBufferMemory);
        }

        // --- IMGUI INICIALIZÁLÁSA A LEGFRISEBB DOCKING BRANCH ALAPJÁN ---
        void initImGui() {
            VkDescriptorPoolSize pool_sizes[] = {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };

            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000;
            pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
            pool_info.pPoolSizes = pool_sizes;

            if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
                throw std::runtime_error("Hiba a Descriptor Pool letrehozasakor!");
            }

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

            ImGui::StyleColorsDark();

            ImGui_ImplGlfw_InitForVulkan(window, true);

            // AZ ÚJ VULKAN INIT ARCHITEKTÚRA (Dynamic Rendering kompatibilis)
            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = instance;
            init_info.PhysicalDevice = physicalDevice;
            init_info.Device = device;
            init_info.QueueFamily = 0;
            init_info.Queue = graphicsQueue;
            init_info.PipelineCache = VK_NULL_HANDLE;
            init_info.DescriptorPool = descriptorPool;
            init_info.MinImageCount = 2;
            init_info.ImageCount = static_cast<uint32_t>(swapChainImages.size());
            init_info.Allocator = nullptr;
            init_info.CheckVkResultFn = nullptr;

            // JAVÍTÁS: A RenderPass, Subpass és MSAASamples kikerült a főkstruktúrából,
            // és egy új PipelineInfoMain objektumba rendezték őket.
            // Ezen felül az ImGui_ImplVulkan_Init() már nem várja 2. paraméterként a renderPass-t.
            init_info.PipelineInfoMain.RenderPass = renderPass;
            init_info.PipelineInfoMain.Subpass = 0;
            init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

            ImGui_ImplVulkan_Init(&init_info);

            // JAVÍTÁS: Az ImGui_ImplVulkan_CreateFontsTexture() hívást eltávolítottuk!
            // A friss docking branch már magától elvégzi a font textúra feltöltését
            // a GPU-ra a legelső képkocka renderelésekor (dinamikus upload), így a manuális hívás törölve lett.
        }

        void initVulkan() {
            createInstance(); createSurface(); pickPhysicalDevice(); createLogicalDevice(); createSwapChain(); createImageViews();
            createRenderPass();
            createDepthResources();
            createFramebuffers();
            createGraphicsPipeline(); createCommandPool(); createCommandBuffers(); createSyncObjects();
            createVertexAndIndexBuffers();

            initImGui();

            std::cout << "[Vulkan] Motor inicializalva, Dear ImGui (Docking) integracio KESZ!" << std::endl;
        }

        void processInput() {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
            float cameraSpeed = 60.0f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos = cameraPos + cameraFront * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos = cameraPos - cameraFront * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos = cameraPos - cameraFront.cross(cameraUp).normalize() * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos = cameraPos + cameraFront.cross(cameraUp).normalize() * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos = cameraPos + cameraUp * cameraSpeed;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) cameraPos = cameraPos - cameraUp * cameraSpeed;

            static bool f3PressedLastFrame = false;
            bool f3PressedNow = (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS);
            if (f3PressedNow && !f3PressedLastFrame) {
                telemetryMenuOpen = !telemetryMenuOpen;
                if (telemetryMenuOpen) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                if (telemetryMenuOpen) { menuOpenFrames = 0; menuOpenTimeElapsed = 0.0f; }
            }
            f3PressedLastFrame = f3PressedNow;

            if (!telemetryMenuOpen) {
                double xpos, ypos; glfwGetCursorPos(window, &xpos, &ypos);
                if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
                float xoffset = xpos - lastX; float yoffset = lastY - ypos; lastX = xpos; lastY = ypos;
                float sensitivity = 0.1f; xoffset *= sensitivity; yoffset *= sensitivity;
                yaw += xoffset; pitch += yoffset;
                if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;
                Vec3 front; front.x = cos(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0)); front.y = sin(pitch * (M_PI/180.0)); front.z = sin(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0));
                cameraFront = front.normalize();
            } else {
                firstMouse = true;
            }
        }

        void updateTelemetryState(float dt) {
            totalFrames++; totalTimeElapsed += dt;
            if (totalFrames > 10) {
                if (dt < bestFrameTime) bestFrameTime = dt;
                if (dt > worstFrameTime) worstFrameTime = dt;
            }

            if (!telemetryMenuOpen) return;

            menuOpenFrames++; menuOpenTimeElapsed += dt;
            float currentTimestamp = totalTimeElapsed;
            frameHistory.push_back({currentTimestamp, dt});

            while (!frameHistory.empty() && (currentTimestamp - frameHistory.front().first > 10.0f)) {
                frameHistory.erase(frameHistory.begin());
            }
        }

    public:
        void loadMesh(const MeshData& mesh) { currentMesh = mesh; }

        void run() {
            initWindow(); initVulkan(); mainLoop(); cleanup();
        }

        void mainLoop() {
            while (!glfwWindowShouldClose(window)) {
                float currentFrame = glfwGetTime(); deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;
                glfwPollEvents(); processInput(); updateTelemetryState(deltaTime); drawFrame();
            }
        }

        void cleanup() {
            vkDeviceWaitIdle(device);

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);

            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
            vkDestroyFence(device, inFlightFence, nullptr);

            vkDestroyImageView(device, depthImageView, nullptr);
            vkDestroyImage(device, depthImage, nullptr);
            vkFreeMemory(device, depthImageMemory, nullptr);

            if (vertexBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, vertexBuffer, nullptr); vkFreeMemory(device, vertexBufferMemory, nullptr); vkDestroyBuffer(device, indexBuffer, nullptr); vkFreeMemory(device, indexBufferMemory, nullptr); }
            vkDestroyPipeline(device, graphicsPipeline, nullptr); vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            for(auto fb : swapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr); for(auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
            vkDestroySwapchainKHR(device, swapChain, nullptr); vkDestroyCommandPool(device, commandPool, nullptr); vkDestroyRenderPass(device, renderPass, nullptr); vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr); glfwDestroyWindow(window); glfwTerminate();
        }

        void drawFrame() {
            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (telemetryMenuOpen) {
                float avg1s = 0.0f; float avg10s = 0.0f; size_t count1s = 0; size_t count10s = 0;
                float currentTimestamp = totalTimeElapsed;
                for (const auto& record : frameHistory) {
                    if (currentTimestamp - record.first <= 1.0f) { avg1s += record.second; count1s++; }
                    if (currentTimestamp - record.first <= 10.0f) { avg10s += record.second; count10s++; }
                }
                avg1s = (count1s > 0) ? (avg1s / count1s) : deltaTime;
                avg10s = (count10s > 0) ? (avg10s / count10s) : deltaTime;

                int64_t gcx = static_cast<int64_t>(std::floor(cameraPos.x / 16.0f));
                int64_t gcy = static_cast<int64_t>(std::floor(cameraPos.y / 16.0f));
                int64_t gcz = static_cast<int64_t>(std::floor(cameraPos.z / 16.0f));
                int lcx = static_cast<int>(std::floor(cameraPos.x)) % 16; if (lcx < 0) lcx += 16;
                int lcy = static_cast<int>(std::floor(cameraPos.y)) % 16; if (lcy < 0) lcy += 16;
                int lcz = static_cast<int>(std::floor(cameraPos.z)) % 16; if (lcz < 0) lcz += 16;

                ImGui::SetNextWindowBgAlpha(0.85f);
                ImGui::Begin("NexusForge Telemetry", &telemetryMenuOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "WORLD METRICS");
                ImGui::Separator();
                ImGui::Text("Grid ID: 1");
                ImGui::Text("Global: X: %lld, Y: %lld, Z: %lld", gcx, gcy, gcz);
                ImGui::Text("Local : X: %d, Y: %d, Z: %d", lcx, lcy, lcz);
                ImGui::Text("Camera: Yaw: %.1f, Pitch: %.1f", yaw, pitch);
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "PERFORMANCE");
                ImGui::Separator();
                ImGui::Text("FPS: %d", static_cast<int>(1.0f / (deltaTime > 0.0f ? deltaTime : 0.001f)));
                ImGui::Text("FrameTime (Current): %.2f ms", deltaTime * 1000.0f);
                ImGui::Text("FrameTime (1s Avg): %.2f ms", avg1s * 1000.0f);
                ImGui::Text("FrameTime (10s Avg): %.2f ms", avg10s * 1000.0f);
                ImGui::Text("Best Frame: %.2f ms", (bestFrameTime == 999999.0f ? 0.0f : bestFrameTime * 1000.0f));
                ImGui::Text("Worst Frame: %.2f ms", worstFrameTime * 1000.0f);
                ImGui::End();
            }

            ImGui::Render();

            vkResetCommandBuffer(commandBuffer, 0);
            CommandManager::BeginCommandBuffer(commandBuffer);

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
            renderPassInfo.renderArea.extent = swapChainExtent;

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = VkClearColorValue{0.05f, 0.6f, 0.8f, 1.0f};
            clearValues[1].depthStencil = VkClearDepthStencilValue{1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            if (!currentMesh.vertices.empty()) {
                VkBuffer vertexBuffers[] = {vertexBuffer}; VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                Mat4 view = Mat4::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
                Mat4 proj = Mat4::perspective(45.0f * (M_PI / 180.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
                Mat4 mvp = proj * view;

                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &mvp);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(currentMesh.indices.size()), 1, 0, 0, 0);
            }

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

            vkCmdEndRenderPass(commandBuffer);
            CommandManager::EndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapChain;
            presentInfo.pImageIndices = &imageIndex;
            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }
    };
}