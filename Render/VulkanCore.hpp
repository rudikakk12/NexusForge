#pragma once

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
#include "VulkanMegaArena.hpp" // ÚJ BEILLESZTÉS
#include "../Core/Physics.hpp"

extern void BreakBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz);
extern void PlaceBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz, uint8_t blockID);
extern std::vector<NF::Core::MacroChunk_Small> realWorld;

namespace NF::Render {

    // Emlékszik arra, hogy a Chunk hol él a Mega-Bufferben!
    struct ChunkRenderState {
        uint32_t cmdIndex = 0xFFFFFFFF; // Ha max uint, még nincs lefoglalva
        uint32_t vOffset = 0;
        uint32_t iOffset = 0;
        uint32_t maxV = 0; // Túltervezett méret, hogy ne kelljen mindig újra allokálni
        uint32_t maxI = 0;
    };

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

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        VkCommandBuffer commandBuffer;
        VkCommandPool commandPool;
        VkExtent2D swapChainExtent = {1280, 720};

        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;

        const uint32_t WIDTH = 1280;
        const uint32_t HEIGHT = 720;

        // --- ÚJ: MDI MEGA ARENA ÉS CHUNK ÁLLAPOTOK ---
        VulkanMegaArena megaArena;
        std::vector<ChunkRenderState> chunkRenderStates;

        double lastX = WIDTH / 2.0;
        double lastY = HEIGHT / 2.0;
        float yaw = -90.0f;
        float pitch = 0.0f;
        Vec3 cameraPos = {64.0f, 60.0f, 64.0f};
        Vec3 cameraFront = {0.0f, 0.0f, -1.0f};
        Vec3 cameraUp = {0.0f, 1.0f, 0.0f};
        bool firstMouse = true;
        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        NF::Core::Physics::Entity player = {
            {64.0f, 60.0f, 64.0f}, {0.0f, 0.0f, 0.0f}, {0.6f, 1.8f, 0.6f}, false, false, 0.0f
        };

        int playerHP = 100;
        int maxHP = 100;
        int selectedHotbarSlot = 0;
        uint8_t hotbarBlocks[9] = {1, 2, 3, 0, 0, 0, 0, 0, 0};

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
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            window = glfwCreateWindow(WIDTH, HEIGHT, "NexusForge - MDI GPU-Driven Render", nullptr, nullptr);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        void createInstance() {
            VkApplicationInfo appInfo{}; appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; appInfo.apiVersion = VK_API_VERSION_1_2;
            VkInstanceCreateInfo createInfo{}; createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; createInfo.pApplicationInfo = &appInfo;
            uint32_t count; const char** exts = glfwGetRequiredInstanceExtensions(&count);
            createInfo.enabledExtensionCount = count; createInfo.ppEnabledExtensionNames = exts;
            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("Instance hiba!");
        }
        void createSurface() { if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) throw std::runtime_error("Surface hiba!"); }
        void pickPhysicalDevice() { uint32_t count = 0; vkEnumeratePhysicalDevices(instance, &count, nullptr); std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance, &count, devices.data()); physicalDevice = devices[0]; }
        void createLogicalDevice() {
            float prio = 1.0f; VkDeviceQueueCreateInfo qInfo{}; qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; qInfo.queueFamilyIndex = 0; qInfo.queueCount = 1; qInfo.pQueuePriorities = &prio;
            VkDeviceCreateInfo dInfo{}; dInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; dInfo.pQueueCreateInfos = &qInfo; dInfo.queueCreateInfoCount = 1;
            const std::vector<const char*> ext = { VK_KHR_SWAPCHAIN_EXTENSION_NAME }; dInfo.enabledExtensionCount = static_cast<uint32_t>(ext.size()); dInfo.ppEnabledExtensionNames = ext.data();
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

            // --- JAVÍTVA: VSYNC KIKAPCSOLÁSA (UNLOCKED FPS) ---
            // A VK_PRESENT_MODE_IMMEDIATE_KHR azonnal kilöki a képet (Tearing lehet, de max FPS-t ad)
            createInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

            if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) throw std::runtime_error("Swapchain hiba!");
            uint32_t imageCount; vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr); swapChainImages.resize(imageCount); vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        }
        void createImageViews() {
            swapChainImageViews.resize(swapChainImages.size());
            for (size_t i = 0; i < swapChainImages.size(); i++) {
                VkImageViewCreateInfo viewInfo{}; viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; viewInfo.image = swapChainImages[i]; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB; viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
                vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]);
            }
        }
        void createRenderPass() { renderPass = NF::Render::VulkanPipeline::CreateRenderPass(device, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_D32_SFLOAT); }
        void createDepthResources() {
            VkFormat depthFormat = VK_FORMAT_D32_SFLOAT; VkImageCreateInfo imageInfo{}; imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; imageInfo.imageType = VK_IMAGE_TYPE_2D; imageInfo.extent.width = swapChainExtent.width; imageInfo.extent.height = swapChainExtent.height; imageInfo.extent.depth = 1; imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1; imageInfo.format = depthFormat; imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateImage(device, &imageInfo, nullptr, &depthImage);
            VkMemoryRequirements memRequirements; vkGetImageMemoryRequirements(device, depthImage, &memRequirements);
            VkMemoryAllocateInfo allocInfo{}; allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; allocInfo.allocationSize = memRequirements.size; allocInfo.memoryTypeIndex = VulkanBuffer::FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory); vkBindImageMemory(device, depthImage, depthImageMemory, 0);
            VkImageViewCreateInfo viewInfo{}; viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; viewInfo.image = depthImage; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = depthFormat; viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; viewInfo.subresourceRange.baseMipLevel = 0; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.baseArrayLayer = 0; viewInfo.subresourceRange.layerCount = 1;
            vkCreateImageView(device, &viewInfo, nullptr, &depthImageView);
        }
        void createFramebuffers() {
            swapChainFramebuffers.resize(swapChainImageViews.size());
            for (size_t i = 0; i < swapChainImageViews.size(); i++) {
                std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };
                VkFramebufferCreateInfo fbInfo{}; fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; fbInfo.renderPass = renderPass; fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); fbInfo.pAttachments = attachments.data(); fbInfo.width = swapChainExtent.width; fbInfo.height = swapChainExtent.height; fbInfo.layers = 1;
                vkCreateFramebuffer(device, &fbInfo, nullptr, &swapChainFramebuffers[i]);
            }
        }
        void createGraphicsPipeline() {
            auto vertCode = NF::Render::VulkanRenderer::ReadFile("vert.spv"); auto fragCode = NF::Render::VulkanRenderer::ReadFile("frag.spv");
            VkShaderModule vertShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, vertCode); VkShaderModule fragShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, fragCode);
            graphicsPipeline = NF::Render::VulkanRenderer::CreateGraphicsPipeline(device, swapChainExtent, renderPass, pipelineLayout, vertShaderModule, fragShaderModule);
            vkDestroyShaderModule(device, fragShaderModule, nullptr); vkDestroyShaderModule(device, vertShaderModule, nullptr);
        }
        void createCommandPool() { commandPool = NF::Render::CommandManager::CreateCommandPool(device, 0); }
        void createCommandBuffers() { commandBuffer = NF::Render::CommandManager::CreateCommandBuffer(device, commandPool); }
        void createSyncObjects() {
            VkSemaphoreCreateInfo sInfo{}; sInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkFenceCreateInfo fInfo{}; fInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            vkCreateSemaphore(device, &sInfo, nullptr, &imageAvailableSemaphore); vkCreateSemaphore(device, &sInfo, nullptr, &renderFinishedSemaphore); vkCreateFence(device, &fInfo, nullptr, &inFlightFence);
        }

        void initImGui() {
            VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }, { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 }, { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 }, { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 }, { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };
            VkDescriptorPoolCreateInfo pool_info = {}; pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO; pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; pool_info.maxSets = 1000; pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes)); pool_info.pPoolSizes = pool_sizes;
            vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);

            IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; ImGui::StyleColorsDark();
            ImGui_ImplGlfw_InitForVulkan(window, true);

            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = instance; init_info.PhysicalDevice = physicalDevice; init_info.Device = device; init_info.QueueFamily = 0; init_info.Queue = graphicsQueue; init_info.PipelineCache = VK_NULL_HANDLE; init_info.DescriptorPool = descriptorPool; init_info.MinImageCount = 2; init_info.ImageCount = static_cast<uint32_t>(swapChainImages.size()); init_info.Allocator = nullptr; init_info.CheckVkResultFn = nullptr;
            init_info.PipelineInfoMain.RenderPass = renderPass; init_info.PipelineInfoMain.Subpass = 0; init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            ImGui_ImplVulkan_Init(&init_info);
        }

        void initVulkan() {
            createInstance(); createSurface(); pickPhysicalDevice(); createLogicalDevice(); createSwapChain(); createImageViews();
            createRenderPass(); createDepthResources(); createFramebuffers(); createGraphicsPipeline(); createCommandPool(); createCommandBuffers(); createSyncObjects();

            // MDI Mega Arena Inicializálás!
            megaArena.Initialize(device, physicalDevice);
            chunkRenderStates.resize(200); // 200 a realWorld maximális mérete

            RebuildAll(); // Kezdeti 128 chunk legenerálása

            initImGui();
            std::cout << "[Vulkan] Motor inicializalva! MDI aktív." << std::endl;
        }

    public:
        // --- 1. CSAK EGYETLEN CHUNK ÚJRAMESHELÉSE (MDI MÁGIA) ---
        void UpdateSingleChunk(uint32_t chunkIndex, int cx, int cy, int cz) {
            vkDeviceWaitIdle(device); // Biztonsági stop (később Fencere cseréljük)

            auto chunkMesh = NF::Render::BasicMesher::GenerateMesh(realWorld[chunkIndex], cx, cy, cz);
            auto& state = chunkRenderStates[chunkIndex];

            // Ha a chunk üres lett (kiütöttük az utolsó blokkot is)
            if (chunkMesh.vertices.empty()) {
                if (state.cmdIndex != 0xFFFFFFFF) {
                    megaArena.mappedIndirectData[state.cmdIndex].instanceCount = 0; // GPU ignorálja!
                }
                return;
            }

            // Ha még sosem volt lefoglalva, VAGY nagyobb hely kell neki, mint a régi: ÚJ ALLOKÁCIÓ
            bool needsNewAlloc = (state.cmdIndex == 0xFFFFFFFF) ||
                                 (chunkMesh.vertices.size() > state.maxV) ||
                                 (chunkMesh.indices.size() > state.maxI);

            if (needsNewAlloc) {
                if (state.cmdIndex != 0xFFFFFFFF) {
                    megaArena.mappedIndirectData[state.cmdIndex].instanceCount = 0; // Régi parancs inaktiválása
                }
                // Túltervezés: Kérjünk +50% helyet, hogy ne kelljen folyton újra allokálni ha épít!
                uint32_t vCount = static_cast<uint32_t>(chunkMesh.vertices.size() * 1.5f);
                uint32_t iCount = static_cast<uint32_t>(chunkMesh.indices.size() * 1.5f);

                megaArena.Allocate(vCount, iCount, state.cmdIndex, state.vOffset, state.iOffset);
                state.maxV = vCount;
                state.maxI = iCount;
            }

            // 1. Nyers adat bemásolása a gigantikus VRAM tömbbe a kapott Offsetre
            memcpy(megaArena.mappedVertexData + state.vOffset, chunkMesh.vertices.data(), chunkMesh.vertices.size() * sizeof(Vertex));
            memcpy(megaArena.mappedIndexData + state.iOffset, chunkMesh.indices.data(), chunkMesh.indices.size() * sizeof(uint32_t));

            // 2. Parancs frissítése a GPU-nak!
            VkDrawIndexedIndirectCommand& cmd = megaArena.mappedIndirectData[state.cmdIndex];
            cmd.indexCount = static_cast<uint32_t>(chunkMesh.indices.size());
            cmd.instanceCount = 1; // 1 = Rajzold ki!
            cmd.firstIndex = state.iOffset;
            cmd.vertexOffset = state.vOffset;
            cmd.firstInstance = 0;
        }

        // --- 2. AZ EGÉSZ VILÁG GENERÁLÁSA (CSAK INDÍTÁSKOR) ---
        void RebuildAll() {
            std::cout << "[Mesher] 128 Chunk epitese es MDI bejegyzes..." << std::endl;
            for (int cx = 0; cx < 8; ++cx) {
                for (int cz = 0; cz < 8; ++cz) {
                    for (int cy = 0; cy < 2; ++cy) {
                        uint32_t chunkIndex = cx + (cz * 8) + (cy * 64);
                        UpdateSingleChunk(chunkIndex, cx, cy, cz);
                    }
                }
            }
        }

    private:
        void processInput() {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

            static bool f3PressedLastFrame = false;
            bool f3PressedNow = (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS);
            if (f3PressedNow && !f3PressedLastFrame) {
                telemetryMenuOpen = !telemetryMenuOpen;
                if (telemetryMenuOpen) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                if (telemetryMenuOpen) { menuOpenFrames = 0; menuOpenTimeElapsed = 0.0f; }
            }
            f3PressedLastFrame = f3PressedNow;

            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) selectedHotbarSlot = 0;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) selectedHotbarSlot = 1;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) selectedHotbarSlot = 2;
            if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) selectedHotbarSlot = 3;
            if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) selectedHotbarSlot = 4;
            if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) selectedHotbarSlot = 5;
            if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) selectedHotbarSlot = 6;
            if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) selectedHotbarSlot = 7;
            if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) selectedHotbarSlot = 8;

            if (!telemetryMenuOpen) {
                double xpos, ypos; glfwGetCursorPos(window, &xpos, &ypos);
                if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
                float xoffset = xpos - lastX; float yoffset = lastY - ypos; lastX = xpos; lastY = ypos;
                float sensitivity = 0.1f; xoffset *= sensitivity; yoffset *= sensitivity;
                yaw += xoffset; pitch += yoffset;
                if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;

                Vec3 front;
                front.x = cos(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0));
                front.y = sin(pitch * (M_PI/180.0));
                front.z = sin(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0));
                cameraFront = front.normalize();
            } else {
                firstMouse = true;
            }

            static bool spaceWasPressed = false;
            static float lastSpacePressTime = 0.0f;
            bool spaceIsPressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
            float currentTime = glfwGetTime();

            if (spaceIsPressed && !spaceWasPressed) {
                if (currentTime - lastSpacePressTime < 0.3f) {
                    player.isFlying = !player.isFlying;
                }
                lastSpacePressTime = currentTime;
            }
            spaceWasPressed = spaceIsPressed;

            float moveForce = 50.0f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && !player.isFlying) moveForce *= 2.0f;
            if (!player.isGrounded && !player.isFlying) moveForce *= 0.1f;

            Vec3 forwardFlat = {cameraFront.x, 0.0f, cameraFront.z};
            forwardFlat = forwardFlat.normalize();
            Vec3 rightFlat = forwardFlat.cross(cameraUp).normalize();

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { player.velocity.x += forwardFlat.x * moveForce; player.velocity.z += forwardFlat.z * moveForce; }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { player.velocity.x -= forwardFlat.x * moveForce; player.velocity.z -= forwardFlat.z * moveForce; }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { player.velocity.x -= rightFlat.x * moveForce; player.velocity.z -= rightFlat.z * moveForce; }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { player.velocity.x += rightFlat.x * moveForce; player.velocity.z += rightFlat.z * moveForce; }

            if (player.isFlying) {
                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) player.velocity.y += 50.0f * deltaTime;
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) player.velocity.y -= 50.0f * deltaTime;
            } else {
                if (spaceIsPressed && player.isGrounded) {
                    player.velocity.y = 10.0f;
                }
            }

            NF::Core::Physics::UpdateEntityPhysics(player, deltaTime);

            if (player.pendingFallDamage > 0.0f) {
                int damage = static_cast<int>(player.pendingFallDamage * 2.0f);
                playerHP -= damage;
                player.pendingFallDamage = 0.0f;
                if (playerHP <= 0) {
                    player.position = {64.0f, 60.0f, 64.0f};
                    player.velocity = {0.0f, 0.0f, 0.0f};
                    player.isFlying = false;
                    playerHP = maxHP;
                }
            }

            cameraPos = { player.position.x, player.position.y + 1.6f, player.position.z };

            static bool leftMousePressed = false;
            static bool rightMousePressed = false;

            if (!telemetryMenuOpen) {
                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                    if (!leftMousePressed) {
                        auto hit = NF::Core::Physics::VoxelRaycast(cameraPos, cameraFront, 5.0f);
                        if (hit.hit) BreakBlockAndRemesh(hit.hitX, hit.hitY, hit.hitZ);
                        leftMousePressed = true;
                    }
                } else { leftMousePressed = false; }

                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                    if (!rightMousePressed) {
                        auto hit = NF::Core::Physics::VoxelRaycast(cameraPos, cameraFront, 5.0f);
                        if (hit.hit) {
                            uint8_t selectedBlock = hotbarBlocks[selectedHotbarSlot];
                            if (selectedBlock != 0) {
                                int px = hit.hitX + hit.normalX; int py = hit.hitY + hit.normalY; int pz = hit.hitZ + hit.normalZ;

                                NF::Render::Vec3 pMin, pMax;
                                player.GetBounds(player.position, pMin, pMax);
                                bool intersectX = (pMin.x < px + 1.0f && pMax.x > px);
                                bool intersectY = (pMin.y < py + 1.0f && pMax.y > py);
                                bool intersectZ = (pMin.z < pz + 1.0f && pMax.z > pz);

                                if (!(intersectX && intersectY && intersectZ)) PlaceBlockAndRemesh(px, py, pz, selectedBlock);
                            }
                        }
                        rightMousePressed = true;
                    }
                } else { rightMousePressed = false; }
            }
        }

        void updateTelemetryState(float dt) {
            totalFrames++;
            totalTimeElapsed += dt;

            // Mindig rögzítjük a best/worst értékeket az első 10 frame után (bemelegedés)
            if (totalFrames > 10) {
                if (dt < bestFrameTime) bestFrameTime = dt;
                if (dt > worstFrameTime) worstFrameTime = dt;
            }

            // Mindig rögzítjük a history-t, FÜGGETLENÜL attól, hogy nyitva van-e a menü!
            float currentTimestamp = totalTimeElapsed;
            frameHistory.push_back({currentTimestamp, dt});

            // Töröljük a 10 másodpercnél régebbi adatokat, hogy ne fogyjon el a RAM
            while (!frameHistory.empty() && (currentTimestamp - frameHistory.front().first > 10.0f)) {
                frameHistory.erase(frameHistory.begin());
            }

            // A menü-specifikus változókat csak akkor növeljük, ha nyitva van
            if (telemetryMenuOpen) {
                menuOpenFrames++;
                menuOpenTimeElapsed += dt;
            }
        }

    public:
        void run() { initWindow(); initVulkan(); mainLoop(); cleanup(); }

        void mainLoop() {
            while (!glfwWindowShouldClose(window)) {
                float currentFrame = glfwGetTime(); deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;
                glfwPollEvents(); processInput(); updateTelemetryState(deltaTime); drawFrame();
            }
        }

        void cleanup() {
            vkDeviceWaitIdle(device);
            ImGui_ImplVulkan_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();

            megaArena.Cleanup(); // ÚJ: Mega Arena Törlése

            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr); vkDestroySemaphore(device, imageAvailableSemaphore, nullptr); vkDestroyFence(device, inFlightFence, nullptr);
            vkDestroyImageView(device, depthImageView, nullptr); vkDestroyImage(device, depthImage, nullptr); vkFreeMemory(device, depthImageMemory, nullptr);
            vkDestroyPipeline(device, graphicsPipeline, nullptr); vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            for(auto fb : swapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr); for(auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
            vkDestroySwapchainKHR(device, swapChain, nullptr); vkDestroyCommandPool(device, commandPool, nullptr); vkDestroyRenderPass(device, renderPass, nullptr); vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr); glfwDestroyWindow(window); glfwTerminate();
        }

        void drawFrame() {
            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &inFlightFence);

            uint32_t imageIndex;
            vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            ImGui_ImplVulkan_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(ImVec2(WIDTH, HEIGHT));
            ImGui::Begin("InGameHUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 center(WIDTH * 0.5f, HEIGHT * 0.5f);
            drawList->AddLine(ImVec2(center.x - 10, center.y), ImVec2(center.x + 10, center.y), IM_COL32(255, 255, 255, 200), 2.0f);
            drawList->AddLine(ImVec2(center.x, center.y - 10), ImVec2(center.x, center.y + 10), IM_COL32(255, 255, 255, 200), 2.0f);

            ImGui::SetCursorPos(ImVec2(20, HEIGHT - 70));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(200, 40, 40, 255)); ImGui::ProgressBar((float)playerHP / maxHP, ImVec2(200, 20), ""); ImGui::PopStyleColor();
            ImGui::SetCursorPos(ImVec2(25, HEIGHT - 70)); ImGui::Text("HP: %d / %d", playerHP, maxHP);

            float hotbarWidth = 9 * 50.0f; float startX = (WIDTH - hotbarWidth) * 0.5f; float startY = HEIGHT - 65.0f;
            for (int i = 0; i < 9; ++i) {
                ImVec2 pos(startX + i * 50.0f, startY);
                ImU32 bgColor = (i == selectedHotbarSlot) ? IM_COL32(200, 200, 50, 180) : IM_COL32(50, 50, 50, 180);
                drawList->AddRectFilled(pos, ImVec2(pos.x + 45, pos.y + 45), bgColor, 4.0f);
                drawList->AddRect(pos, ImVec2(pos.x + 45, pos.y + 45), IM_COL32(0, 0, 0, 255), 4.0f, 0, 2.0f);
                char text[16]; snprintf(text, sizeof(text), "%d", i + 1); drawList->AddText(ImVec2(pos.x + 5, pos.y + 5), IM_COL32(255, 255, 255, 255), text);
                if (hotbarBlocks[i] != 0) { char blkText[16]; snprintf(blkText, sizeof(blkText), "ID:%d", hotbarBlocks[i]); drawList->AddText(ImVec2(pos.x + 5, pos.y + 25), IM_COL32(150, 255, 150, 255), blkText); }
            }
            ImGui::End();

            if (telemetryMenuOpen) {
                float avg1s = 0.0f; float avg10s = 0.0f; size_t count1s = 0; size_t count10s = 0; float currentTimestamp = totalTimeElapsed;
                for (const auto& record : frameHistory) { if (currentTimestamp - record.first <= 1.0f) { avg1s += record.second; count1s++; } if (currentTimestamp - record.first <= 10.0f) { avg10s += record.second; count10s++; } }
                avg1s = (count1s > 0) ? (avg1s / count1s) : deltaTime; avg10s = (count10s > 0) ? (avg10s / count10s) : deltaTime;
                int64_t gcx = static_cast<int64_t>(std::floor(cameraPos.x / 16.0f)); int64_t gcy = static_cast<int64_t>(std::floor(cameraPos.y / 16.0f)); int64_t gcz = static_cast<int64_t>(std::floor(cameraPos.z / 16.0f));
                ImGui::SetNextWindowBgAlpha(0.85f);
                ImGui::Begin("NexusForge Telemetry", &telemetryMenuOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "PERFORMANCE"); ImGui::Separator();
                ImGui::Text("FPS: %d", static_cast<int>(1.0f / (deltaTime > 0.0f ? deltaTime : 0.001f)));
                ImGui::Text("FrameTime (Current): %.2f ms", deltaTime * 1000.0f);
                // IDE JÖNNEK VISSZA A HIÁNYZÓ ADATOK:
                ImGui::Text("FrameTime (1s Avg): %.2f ms", avg1s * 1000.0f);
                ImGui::Text("FrameTime (10s Avg): %.2f ms", avg10s * 1000.0f);
                ImGui::Text("Best Frame: %.2f ms", (bestFrameTime == 999999.0f ? 0.0f : bestFrameTime * 1000.0f));
                ImGui::Text("Worst Frame: %.2f ms", worstFrameTime * 1000.0f);
                ImGui::Text("Draw Commands: %d", megaArena.currentCommandCount.load());
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

            // =================================================================
            // AZ EGYETLEN EGY RAJZOLÓ PARANCS (MDI MÁGIA)
            // =================================================================
            uint32_t activeCommands = megaArena.currentCommandCount.load(std::memory_order_relaxed);
            if (activeCommands > 0) {
                VkBuffer vertexBuffers[] = {megaArena.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, megaArena.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                Mat4 view = Mat4::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
                Mat4 proj = Mat4::perspective(45.0f * (M_PI / 180.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
                Mat4 mvp = proj * view;
                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &mvp);

                // A GPU végigolvassa az Indirect Buffert és mindent megrajzol, CPU beavatkozás NÉLKÜL!
                vkCmdDrawIndexedIndirect(commandBuffer, megaArena.indirectBuffer, 0, activeCommands, sizeof(VkDrawIndexedIndirectCommand));
            }

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
            vkCmdEndRenderPass(commandBuffer);
            CommandManager::EndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[] = {imageAvailableSemaphore}; VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1; submitInfo.pWaitSemaphores = waitSemaphores; submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &commandBuffer;
            VkSemaphore signalSemaphores[] = {renderFinishedSemaphore}; submitInfo.signalSemaphoreCount = 1; submitInfo.pSignalSemaphores = signalSemaphores;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

            VkPresentInfoKHR presentInfo{}; presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1; presentInfo.pWaitSemaphores = signalSemaphores;
            presentInfo.swapchainCount = 1; presentInfo.pSwapchains = &swapChain;
            presentInfo.pImageIndices = &imageIndex;

            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }
    };
}