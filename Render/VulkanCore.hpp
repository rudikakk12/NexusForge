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

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <atomic>
#include <deque>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

#include "VulkanCommandManager.hpp"
#include "VulkanRenderer.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanBuffer.hpp"
#include "BasicMesher.hpp"
#include "CameraMath.hpp"
#include "VulkanMegaArena.hpp"
#include "TelemetryHUD.hpp"
#include "TextureManager.hpp"
#include "../Core/Physics.hpp"
#include "../Core/World.hpp"
#include "../Core/WorldGeneration.hpp" // <-- BEKÖTVE A GENERÁTOR

extern void BreakBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz);
extern void PlaceBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz, uint8_t blockID);

namespace NF::Render {

    struct CullData {
        uint64_t hash;
        float minX, minY, minZ;
        float maxX, maxY, maxZ;
        uint32_t cmdIndex;
    };

    struct ChunkRenderState {
        uint32_t cmdIndex = 0xFFFFFFFF;
        uint32_t cullIndex = 0xFFFFFFFF;
        uint32_t activeHashIndex = 0xFFFFFFFF;
        uint32_t vOffset = 0;
        uint32_t iOffset = 0;
        uint32_t maxV = 0;
        uint32_t maxI = 0;
        bool isEmpty = true;
    };

    enum class TaskType { TERRAIN_GENERATED, MESH_READY, UNLOAD };
    struct ChunkTask {
        TaskType type;
        uint64_t hash;
        int cx, cy, cz;
        std::unique_ptr<NF::Core::MacroChunk_Small> newChunk;
        NF::Render::MeshData meshData;
    };

    // A Lock-Free hálósítás Szent Grálja
    struct MeshJob {
        uint64_t hash;
        int cx, cy, cz;
        NF::Core::MacroChunk_Small* center;
        const uint8_t *pX, *nX, *pY, *nY, *pZ, *nZ;
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

        VulkanMegaArena megaArena;
        std::unordered_map<uint64_t, ChunkRenderState> chunkRenderStates;
        std::vector<CullData> cullArray;

        std::vector<VkDrawIndexedIndirectCommand> masterCommands;
        std::vector<VkDrawIndexedIndirectCommand> visibleCommands;

        std::vector<ChunkRenderState> freeVramSlots;

        std::mutex uploadMutex;
        std::vector<ChunkTask> uploadQueue;
        std::deque<ChunkTask> mainThreadTaskQueue;

        std::mutex unloadMutex;
        std::vector<ChunkTask> unloadQueue;

        std::mutex requestMutex;
        std::unordered_set<uint64_t> requestedChunks;

        std::atomic<bool> workerRunning{true};
        std::thread workerThread;

        std::vector<uint64_t> activeChunkHashes;

        int currentlyDrawnChunks = 0;
        int renderDistance = 8;

        bool isMouseCaptured = true;

        double lastX = WIDTH / 2.0;
        double lastY = HEIGHT / 2.0;
        float yaw = -90.0f;
        float pitch = 0.0f;

        Vec3 cameraPos = {256.0f, 80.0f, 256.0f};
        Vec3 cameraFront = {0.0f, 0.0f, -1.0f};
        Vec3 cameraUp = {0.0f, 1.0f, 0.0f};
        bool firstMouse = true;
        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        NF::Core::Physics::Entity player = { {256.0f, 80.0f, 256.0f}, {0.0f, 0.0f, 0.0f}, {0.6f, 1.8f, 0.6f}, false, false, 0.0f };

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
        float avg1s = 0.0f;
        float avg10s = 0.0f;

        std::mutex jobMutex;
        std::vector<MeshJob> pendingMeshJobs;
        std::unordered_set<uint64_t> dispatchedMeshes; // Ez szigorúan csak a főszálon él!

        // EZ A HÁROM SOR HIÁNYZIK VAGY CSONKA NÁLAD:
        NF::Render::TextureManager textureManager;
        VkDescriptorSet mainDescriptorSet;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE; // <--- EZT KERESI A FORDÍTÓ!

        VkDescriptorPool engineDescriptorPool = VK_NULL_HANDLE; // <-- Ez a tiéd!

        void initWindow() {
            if (!glfwInit()) throw std::runtime_error("GLFW init hiba!");
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
            window = glfwCreateWindow(WIDTH, HEIGHT, "NexusForge - Infinite World Streaming", nullptr, nullptr);
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
            auto vertCode = NF::Render::VulkanRenderer::ReadFile("vert.spv");
            auto fragCode = NF::Render::VulkanRenderer::ReadFile("frag.spv");

            VkShaderModule vertShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, vertCode);
            VkShaderModule fragShaderModule = NF::Render::VulkanRenderer::CreateShaderModule(device, fragCode);

            // JAVÍTVA: Itt adjuk át 7. paraméterként a descriptorSetLayout-ot!
            graphicsPipeline = NF::Render::VulkanRenderer::CreateGraphicsPipeline(
                device,
                swapChainExtent,
                renderPass,
                pipelineLayout,
                vertShaderModule,
                fragShaderModule,
                descriptorSetLayout // <--- EZ HIÁNYZOTT!
            );

            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);
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

        void createDescriptorSetLayout() {
            // 1. Textúra sampler binding (kell a shadernek: layout(binding = 1))
            VkDescriptorSetLayoutBinding samplerLayoutBinding{};
            samplerLayoutBinding.binding = 1;
            samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerLayoutBinding.descriptorCount = 1;
            samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            samplerLayoutBinding.pImmutableSamplers = nullptr;

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &samplerLayoutBinding;

            // Itt jön létre a descriptorSetLayout, ami eddig hiányzott
            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("Kritikus hiba: DescriptorSetLayout létrehozása meghiúsult!");
            }
        }

void initVulkan() {
            createInstance(); createSurface(); pickPhysicalDevice(); createLogicalDevice(); createSwapChain(); createImageViews();
            createRenderPass();
            createDescriptorSetLayout();
            createDepthResources(); createFramebuffers();
            createGraphicsPipeline();
            createCommandPool(); createCommandBuffers(); createSyncObjects();

            // 1. Saját Descriptor Pool létrehozása (NEM az ImGui-é!)
            VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = pool_sizes;
            poolInfo.maxSets = 1;
            vkCreateDescriptorPool(device, &poolInfo, nullptr, &engineDescriptorPool);

            megaArena.Initialize(device, physicalDevice);
            cullArray.reserve(300000);
            masterCommands.resize(megaArena.MAX_COMMANDS);
            visibleCommands.reserve(300000);

            initImGui();

            // 2. Textúrák
            textureManager.LoadBlockTextures(device, physicalDevice, commandPool, graphicsQueue, "../Assets/Textures/Blocks");

            // 3. Descriptor Set lefoglalása a SAJÁT poolból
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = engineDescriptorPool; // <-- SAJÁT POOL!
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &descriptorSetLayout;

            if (vkAllocateDescriptorSets(device, &allocInfo, &mainDescriptorSet) != VK_SUCCESS) {
                throw std::runtime_error("Textura Descriptor Set lefoglalasa sikertelen!");
            }

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureManager.textureArrayView;
            imageInfo.sampler = textureManager.textureSampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = mainDescriptorSet;
            descriptorWrite.dstBinding = 1;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

            workerThread = std::thread(&VulkanCore::AsyncChunkWorker, this);
            std::cout << "[Vulkan] Motor inicializalva! Okos Yield & Dense MDI aktiv." << std::endl;
        }

    public:
        void UpdateSingleChunk(uint64_t hash, int cx, int cy, int cz) {
            vkDeviceWaitIdle(device);

            // Megkeressük a memóriában a szomszédos chunkok pointereit (Ha nincsenek ott, nullptr lesz)
            const uint8_t* pX = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx+1, cy, cz)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx+1, cy, cz)]->tickAfter : nullptr;
            const uint8_t* nX = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx-1, cy, cz)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx-1, cy, cz)]->tickAfter : nullptr;
            const uint8_t* pY = (cy < 3 && NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy+1, cz))) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy+1, cz)]->tickAfter : nullptr;
            const uint8_t* nY = (cy > 0 && NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy-1, cz))) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy-1, cz)]->tickAfter : nullptr;
            const uint8_t* pZ = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy, cz+1)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy, cz+1)]->tickAfter : nullptr;
            const uint8_t* nZ = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy, cz-1)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy, cz-1)]->tickAfter : nullptr;

            auto chunkMesh = NF::Render::BasicMesher::GenerateMesh(*NF::Core::realWorld[hash], cx, cy, cz, pX, nX, pY, nY, pZ, nZ);            auto& state = chunkRenderStates[hash];
            bool isNew = (state.cmdIndex == 0xFFFFFFFF);

            if (chunkMesh.vertices.empty()) {
                state.isEmpty = true;
                if (!isNew) {
                    if (state.cmdIndex != 0xFFFFFFFF) {
                        freeVramSlots.push_back(state);
                        state.cmdIndex = 0xFFFFFFFF;
                    }
                    if (state.cullIndex != 0xFFFFFFFF && state.cullIndex < cullArray.size()) {
                        uint32_t cIdx = state.cullIndex;
                        cullArray[cIdx] = cullArray.back();
                        chunkRenderStates[cullArray[cIdx].hash].cullIndex = cIdx;
                        cullArray.pop_back();
                        state.cullIndex = 0xFFFFFFFF;
                    }
                }
                return;
            }

            state.isEmpty = false;
            bool needsNewAlloc = isNew || (chunkMesh.vertices.size() > state.maxV) || (chunkMesh.indices.size() > state.maxI);

            if (needsNewAlloc) {
                if (!isNew && state.cmdIndex != 0xFFFFFFFF) { freeVramSlots.push_back(state); }

                uint32_t vCount = static_cast<uint32_t>(chunkMesh.vertices.size() * 1.2f);
                uint32_t iCount = static_cast<uint32_t>(chunkMesh.indices.size() * 1.2f);

                bool foundReuse = false;
                for (size_t i = 0; i < freeVramSlots.size(); ++i) {
                    if (freeVramSlots[i].maxV >= vCount && freeVramSlots[i].maxI >= iCount) {
                        state.cmdIndex = freeVramSlots[i].cmdIndex; state.vOffset = freeVramSlots[i].vOffset; state.iOffset = freeVramSlots[i].iOffset; state.maxV = freeVramSlots[i].maxV; state.maxI = freeVramSlots[i].maxI;
                        freeVramSlots[i] = freeVramSlots.back(); freeVramSlots.pop_back(); foundReuse = true; break;
                    }
                }

                if (!foundReuse) {
                    if (!megaArena.Allocate(vCount, iCount, state.cmdIndex, state.vOffset, state.iOffset)) return;
                    state.maxV = vCount; state.maxI = iCount;
                }
            }

            memcpy(megaArena.mappedVertexData + state.vOffset, chunkMesh.vertices.data(), chunkMesh.vertices.size() * sizeof(Vertex));
            memcpy(megaArena.mappedIndexData + state.iOffset, chunkMesh.indices.data(), chunkMesh.indices.size() * sizeof(uint32_t));

            VkDrawIndexedIndirectCommand& cmd = masterCommands[state.cmdIndex];
            cmd.indexCount = static_cast<uint32_t>(chunkMesh.indices.size());
            cmd.instanceCount = 1; cmd.firstIndex = state.iOffset; cmd.vertexOffset = state.vOffset; cmd.firstInstance = 0;

            if (isNew) {
                state.activeHashIndex = static_cast<uint32_t>(activeChunkHashes.size()); activeChunkHashes.push_back(hash);
                std::lock_guard<std::mutex> lock(requestMutex); requestedChunks.insert(hash);
            }

            if (state.cullIndex == 0xFFFFFFFF) {
                state.cullIndex = static_cast<uint32_t>(cullArray.size());
                CullData cd; cd.hash = hash; cd.minX = cx * 16.0f; cd.minY = cy * 16.0f; cd.minZ = cz * 16.0f; cd.maxX = cd.minX + 16.0f; cd.maxY = cd.minY + 16.0f; cd.maxZ = cd.minZ + 16.0f; cd.cmdIndex = state.cmdIndex;
                cullArray.push_back(cd);
            } else { cullArray[state.cullIndex].cmdIndex = state.cmdIndex; }
        }

    private:
void AsyncChunkWorker() {
            int lastPcx = -999999; int lastPcz = -999999;

            while (workerRunning) {
                int pcx = static_cast<int>(std::floor(cameraPos.x)) >> 4;
                int pcz = static_cast<int>(std::floor(cameraPos.z)) >> 4;
                bool didWork = false;

                // 1. FÁZIS: MESH JOBOK KIVÉGZÉSE (Amit a főszál összekészített nekünk!)
                std::vector<MeshJob> localJobs;
                {
                    std::lock_guard<std::mutex> lock(jobMutex);
                    localJobs = std::move(pendingMeshJobs);
                    pendingMeshJobs.clear();
                }

                for (const auto& job : localJobs) {
                    auto mesh = NF::Render::BasicMesher::GenerateMesh(*job.center, job.cx, job.cy, job.cz, job.pX, job.nX, job.pY, job.nY, job.pZ, job.nZ);

                    std::lock_guard<std::mutex> lock(uploadMutex);
                    ChunkTask task; task.type = TaskType::MESH_READY; task.hash = job.hash; task.cx = job.cx; task.cy = job.cy; task.cz = job.cz;
                    task.meshData = std::move(mesh);
                    uploadQueue.push_back(std::move(task));
                    didWork = true;
                }

                // 2. FÁZIS: TEREPGENERÁLÁS (Csak a nyers blokk adatok, mesh nélkül!)
                if (pcx != lastPcx || pcz != lastPcz) {
                    lastPcx = pcx; lastPcz = pcz;
                    std::vector<uint64_t> toUnload;
                    {
                        std::lock_guard<std::mutex> lock(requestMutex);
                        for (auto it = requestedChunks.begin(); it != requestedChunks.end(); ) {
                            uint64_t hash = *it;
                            int cx = (hash >> 42) & 0x3FFFFF; int cz = hash & 0x3FFFFF;
                            if (cx & 0x200000) cx |= ~0x3FFFFF; if (cz & 0x200000) cz |= ~0x3FFFFF;
                            int dx = cx - pcx; int dz = cz - pcz;
                            if (dx*dx + dz*dz > (renderDistance + 2) * (renderDistance + 2)) {
                                toUnload.push_back(hash); it = requestedChunks.erase(it);
                            } else { ++it; }
                        }
                    }
                    if (!toUnload.empty()) {
                        std::lock_guard<std::mutex> lock(unloadMutex);
                        for (uint64_t h : toUnload) {
                            ChunkTask task; task.type = TaskType::UNLOAD; task.hash = h; unloadQueue.push_back(std::move(task));
                        }
                    }
                }

                for (int r = 0; r <= renderDistance; ++r) {
                    for (int x = -r; x <= r; ++x) {
                        for (int z = -r; z <= r; ++z) {
                            if (x*x + z*z > r*r) continue;

                            for (int cy = 0; cy < 4; ++cy) {
                                int cx = pcx + x; int cz = pcz + z;
                                uint64_t hash = NF::Core::GetChunkHash(cx, cy, cz);

                                bool needsGen = false;
                                {
                                    std::lock_guard<std::mutex> lock(requestMutex);
                                    if (requestedChunks.find(hash) == requestedChunks.end()) {
                                        requestedChunks.insert(hash); needsGen = true;
                                    }
                                }

                                if (needsGen) {
                                    NF::Core::WorldGenerator terrainGen;
                                    auto newChunk = std::make_unique<NF::Core::MacroChunk_Small>();
                                    terrainGen.Generate(*newChunk, cx, cy, cz);

                                    {
                                        std::lock_guard<std::mutex> lock(uploadMutex);
                                        ChunkTask task; task.type = TaskType::TERRAIN_GENERATED; task.hash = hash; task.cx = cx; task.cy = cy; task.cz = cz;
                                        task.newChunk = std::move(newChunk);
                                        uploadQueue.push_back(std::move(task));
                                    }
                                    didWork = true;

                                    while (workerRunning) {
                                        bool queueFull = false;
                                        { std::lock_guard<std::mutex> lock(uploadMutex); queueFull = uploadQueue.size() >= 2000; }
                                        if (!queueFull) break;
                                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                                    }
                                }
                            }
                        }
                    }
                }
                if (!didWork) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        void CheckAndDispatchMeshJob(int cx, int cy, int cz) {
            if (cy < 0 || cy >= 4) return;
            uint64_t hash = NF::Core::GetChunkHash(cx, cy, cz);

            if (NF::Core::realWorld.find(hash) == NF::Core::realWorld.end()) return;
            if (dispatchedMeshes.find(hash) != dispatchedMeshes.end()) return; // Már fut/kész

            const uint8_t* pX = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx+1, cy, cz)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx+1, cy, cz)]->tickAfter : nullptr;
            const uint8_t* nX = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx-1, cy, cz)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx-1, cy, cz)]->tickAfter : nullptr;
            const uint8_t* pY = (cy < 3 && NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy+1, cz))) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy+1, cz)]->tickAfter : nullptr;
            const uint8_t* nY = (cy > 0 && NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy-1, cz))) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy-1, cz)]->tickAfter : nullptr;
            const uint8_t* pZ = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy, cz+1)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy, cz+1)]->tickAfter : nullptr;
            const uint8_t* nZ = NF::Core::realWorld.count(NF::Core::GetChunkHash(cx, cy, cz-1)) ? NF::Core::realWorld[NF::Core::GetChunkHash(cx, cy, cz-1)]->tickAfter : nullptr;

            bool hasAllNeighbors = true;
            if (!pX || !nX || !pZ || !nZ) hasAllNeighbors = false;
            if (cy < 3 && !pY) hasAllNeighbors = false;
            if (cy > 0 && !nY) hasAllNeighbors = false;

            if (hasAllNeighbors) {
                MeshJob job;
                job.hash = hash; job.cx = cx; job.cy = cy; job.cz = cz;
                job.center = NF::Core::realWorld[hash].get();
                job.pX = pX; job.nX = nX; job.pY = pY; job.nY = nY; job.pZ = pZ; job.nZ = nZ;

                dispatchedMeshes.insert(hash);

                std::lock_guard<std::mutex> lock(jobMutex);
                pendingMeshJobs.push_back(job);
            }
        }

void ProcessTaskQueues() {
            std::vector<ChunkTask> localUnloads;
            {
                std::lock_guard<std::mutex> lock(unloadMutex);
                localUnloads = std::move(unloadQueue);
                unloadQueue.clear();
            }

            for (auto& task : localUnloads) {
                auto it = chunkRenderStates.find(task.hash);
                if (it != chunkRenderStates.end()) {
                    auto& state = it->second;
                    if (state.cmdIndex != 0xFFFFFFFF) freeVramSlots.push_back(state);
                    if (state.cullIndex != 0xFFFFFFFF && state.cullIndex < cullArray.size()) {
                        uint32_t cIdx = state.cullIndex; cullArray[cIdx] = cullArray.back(); chunkRenderStates[cullArray[cIdx].hash].cullIndex = cIdx; cullArray.pop_back();
                    }
                    if (state.activeHashIndex != 0xFFFFFFFF && state.activeHashIndex < activeChunkHashes.size()) {
                        uint32_t aIdx = state.activeHashIndex; uint64_t lastHash = activeChunkHashes.back(); activeChunkHashes[aIdx] = lastHash; chunkRenderStates[lastHash].activeHashIndex = aIdx; activeChunkHashes.pop_back();
                    }
                    chunkRenderStates.erase(it);
                }
                NF::Core::realWorld.erase(task.hash);
                dispatchedMeshes.erase(task.hash); // Ezt ki kell venni a pool-ból!
            }

            {
                std::lock_guard<std::mutex> lock(uploadMutex);
                for (auto& task : uploadQueue) mainThreadTaskQueue.push_back(std::move(task));
                uploadQueue.clear();
            }

            auto uploadStartTime = std::chrono::high_resolution_clock::now();

            while (!mainThreadTaskQueue.empty()) {
                auto task = std::move(mainThreadTaskQueue.front());
                mainThreadTaskQueue.pop_front();

                if (task.type == TaskType::UNLOAD) continue;

                bool isStillRequested = false;
                { std::lock_guard<std::mutex> lock(requestMutex); isStillRequested = (requestedChunks.find(task.hash) != requestedChunks.end()); }
                if (!isStillRequested) continue;

                // HA ÚJ BLOKK ADAT JÖTT, BERAKJUK ÉS MEGNÉZZÜK KINYÍLT-E AZ ÚT A MESHINGNEK!
                if (task.type == TaskType::TERRAIN_GENERATED) {
                    NF::Core::realWorld[task.hash] = std::move(task.newChunk);

                    CheckAndDispatchMeshJob(task.cx, task.cy, task.cz);
                    CheckAndDispatchMeshJob(task.cx+1, task.cy, task.cz);
                    CheckAndDispatchMeshJob(task.cx-1, task.cy, task.cz);
                    CheckAndDispatchMeshJob(task.cx, task.cy+1, task.cz);
                    CheckAndDispatchMeshJob(task.cx, task.cy-1, task.cz);
                    CheckAndDispatchMeshJob(task.cx, task.cy, task.cz+1);
                    CheckAndDispatchMeshJob(task.cx, task.cy, task.cz-1);
                    continue; // Ide nem kell VRAM allokáció, irány a kövi elem!
                }

                // HA KÉSZ A MESH, JÖHET AZ UPLOAD
                if (task.type == TaskType::MESH_READY) {
                    auto& state = chunkRenderStates[task.hash];
                    bool isNew = (state.cmdIndex == 0xFFFFFFFF);

                    if (task.meshData.vertices.empty()) {
                        state.isEmpty = true;
                        if (!isNew) {
                            if (state.cmdIndex != 0xFFFFFFFF) { freeVramSlots.push_back(state); state.cmdIndex = 0xFFFFFFFF; }
                            if (state.cullIndex != 0xFFFFFFFF && state.cullIndex < cullArray.size()) { uint32_t cIdx = state.cullIndex; cullArray[cIdx] = cullArray.back(); chunkRenderStates[cullArray[cIdx].hash].cullIndex = cIdx; cullArray.pop_back(); state.cullIndex = 0xFFFFFFFF; }
                        }
                        continue;
                    }

                    state.isEmpty = false;
                    bool needsNewAlloc = isNew || (task.meshData.vertices.size() > state.maxV) || (task.meshData.indices.size() > state.maxI);

                    if (needsNewAlloc) {
                        if (!isNew && state.cmdIndex != 0xFFFFFFFF) freeVramSlots.push_back(state);

                        uint32_t vCount = static_cast<uint32_t>(task.meshData.vertices.size() * 1.2f);
                        uint32_t iCount = static_cast<uint32_t>(task.meshData.indices.size() * 1.2f);

                        bool foundReuse = false;
                        for (size_t i = 0; i < freeVramSlots.size(); ++i) {
                            if (freeVramSlots[i].maxV >= vCount && freeVramSlots[i].maxI >= iCount) {
                                state.cmdIndex = freeVramSlots[i].cmdIndex; state.vOffset = freeVramSlots[i].vOffset; state.iOffset = freeVramSlots[i].iOffset; state.maxV = freeVramSlots[i].maxV; state.maxI = freeVramSlots[i].maxI;
                                freeVramSlots[i] = freeVramSlots.back(); freeVramSlots.pop_back(); foundReuse = true; break;
                            }
                        }

                        if (!foundReuse) { if (!megaArena.Allocate(vCount, iCount, state.cmdIndex, state.vOffset, state.iOffset)) continue; state.maxV = vCount; state.maxI = iCount; }
                    }

                    memcpy(megaArena.mappedVertexData + state.vOffset, task.meshData.vertices.data(), task.meshData.vertices.size() * sizeof(Vertex));
                    memcpy(megaArena.mappedIndexData + state.iOffset, task.meshData.indices.data(), task.meshData.indices.size() * sizeof(uint32_t));

                    VkDrawIndexedIndirectCommand& cmd = masterCommands[state.cmdIndex];
                    cmd.indexCount = static_cast<uint32_t>(task.meshData.indices.size());
                    cmd.instanceCount = 1; cmd.firstIndex = state.iOffset; cmd.vertexOffset = state.vOffset; cmd.firstInstance = 0;

                    if (isNew) { state.activeHashIndex = static_cast<uint32_t>(activeChunkHashes.size()); activeChunkHashes.push_back(task.hash); }

                    if (state.cullIndex == 0xFFFFFFFF) {
                        state.cullIndex = static_cast<uint32_t>(cullArray.size());
                        CullData cd; cd.hash = task.hash; cd.minX = task.cx * 16.0f; cd.minY = task.cy * 16.0f; cd.minZ = task.cz * 16.0f; cd.maxX = cd.minX + 16.0f; cd.maxY = cd.minY + 16.0f; cd.maxZ = cd.minZ + 16.0f; cd.cmdIndex = state.cmdIndex;
                        cullArray.push_back(cd);
                    } else { cullArray[state.cullIndex].cmdIndex = state.cmdIndex; }

                    auto now = std::chrono::high_resolution_clock::now();
                    float elapsedMs = std::chrono::duration<float, std::milli>(now - uploadStartTime).count();
                    if (elapsedMs > 4.0f) break;
                }
            }
        }

        void processInput() {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

            static bool f3PressedLastFrame = false; bool f3PressedNow = (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS);
            if (f3PressedNow && !f3PressedLastFrame) telemetryMenuOpen = !telemetryMenuOpen;
            f3PressedLastFrame = f3PressedNow;

            static bool fPressedLastFrame = false; bool fPressedNow = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
            if (fPressedNow && !fPressedLastFrame) {
                isMouseCaptured = !isMouseCaptured;
                if (isMouseCaptured) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                if (isMouseCaptured) firstMouse = true;
            }
            fPressedLastFrame = fPressedNow;

            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) selectedHotbarSlot = 0;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) selectedHotbarSlot = 1;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) selectedHotbarSlot = 2;

            if (isMouseCaptured) {
                double xpos, ypos; glfwGetCursorPos(window, &xpos, &ypos);
                if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
                float xoffset = xpos - lastX; float yoffset = lastY - ypos; lastX = xpos; lastY = ypos;
                float sensitivity = 0.1f; xoffset *= sensitivity; yoffset *= sensitivity;
                yaw += xoffset; pitch += yoffset;
                if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;

                Vec3 front; front.x = cos(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0)); front.y = sin(pitch * (M_PI/180.0)); front.z = sin(yaw * (M_PI/180.0)) * cos(pitch * (M_PI/180.0));
                cameraFront = front.normalize();
            }

            static bool spaceWasPressed = false; static float lastSpacePressTime = 0.0f;
            bool spaceIsPressed = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS); float currentTime = glfwGetTime();
            if (spaceIsPressed && !spaceWasPressed) { if (currentTime - lastSpacePressTime < 0.3f) player.isFlying = !player.isFlying; lastSpacePressTime = currentTime; }
            spaceWasPressed = spaceIsPressed;

            float baseForce = 50.0f;
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) baseForce *= 5.0f;

            float moveForce = baseForce * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && !player.isFlying) moveForce *= 0.5f;
            if (!player.isGrounded && !player.isFlying) moveForce *= 0.1f;

            Vec3 forwardFlat = {cameraFront.x, 0.0f, cameraFront.z}; forwardFlat = forwardFlat.normalize(); Vec3 rightFlat = forwardFlat.cross(cameraUp).normalize();
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { player.velocity.x += forwardFlat.x * moveForce; player.velocity.z += forwardFlat.z * moveForce; }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { player.velocity.x -= forwardFlat.x * moveForce; player.velocity.z -= forwardFlat.z * moveForce; }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { player.velocity.x -= rightFlat.x * moveForce; player.velocity.z -= rightFlat.z * moveForce; }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { player.velocity.x += rightFlat.x * moveForce; player.velocity.z += rightFlat.z * moveForce; }

            if (player.isFlying) {
                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) player.velocity.y += baseForce * deltaTime;
                if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) player.velocity.y -= baseForce * deltaTime;
            } else { if (spaceIsPressed && player.isGrounded) player.velocity.y = 10.0f; }

            NF::Core::Physics::UpdateEntityPhysics(player, deltaTime);

            if (player.pendingFallDamage > 0.0f) {
                int damage = static_cast<int>(player.pendingFallDamage * 2.0f); playerHP -= damage; player.pendingFallDamage = 0.0f;
                if (playerHP <= 0) { player.position = {256.0f, 80.0f, 256.0f}; player.velocity = {0.0f, 0.0f, 0.0f}; player.isFlying = false; playerHP = maxHP; }
            }
            cameraPos = { player.position.x, player.position.y + 1.6f, player.position.z };

            static bool leftMousePressed = false; static bool rightMousePressed = false;
            if (isMouseCaptured) {
                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                    if (!leftMousePressed) { auto hit = NF::Core::Physics::VoxelRaycast(cameraPos, cameraFront, 5.0f); if (hit.hit) BreakBlockAndRemesh(hit.hitX, hit.hitY, hit.hitZ); leftMousePressed = true; }
                } else { leftMousePressed = false; }

                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                    if (!rightMousePressed) {
                        auto hit = NF::Core::Physics::VoxelRaycast(cameraPos, cameraFront, 5.0f);
                        if (hit.hit) {
                            uint8_t selectedBlock = hotbarBlocks[selectedHotbarSlot];
                            if (selectedBlock != 0) {
                                int px = hit.hitX + hit.normalX; int py = hit.hitY + hit.normalY; int pz = hit.hitZ + hit.normalZ;
                                NF::Render::Vec3 pMin, pMax; player.GetBounds(player.position, pMin, pMax);
                                if (!((pMin.x < px + 1.0f && pMax.x > px) && (pMin.y < py + 1.0f && pMax.y > py) && (pMin.z < pz + 1.0f && pMax.z > pz))) PlaceBlockAndRemesh(px, py, pz, selectedBlock);
                            }
                        }
                        rightMousePressed = true;
                    }
                } else { rightMousePressed = false; }
            }
        }

        void updateTelemetryState(float dt) {
            totalFrames++; totalTimeElapsed += dt;
            if (totalFrames > 10) { if (dt < bestFrameTime) bestFrameTime = dt; if (dt > worstFrameTime) worstFrameTime = dt; }
            float currentTimestamp = totalTimeElapsed; frameHistory.push_back({currentTimestamp, dt});
            while (!frameHistory.empty() && (currentTimestamp - frameHistory.front().first > 10.0f)) frameHistory.erase(frameHistory.begin());

            float sum1s = 0.0f; float sum10s = 0.0f; size_t count1s = 0; size_t count10s = 0;
            for (const auto& record : frameHistory) { if (currentTimestamp - record.first <= 1.0f) { sum1s += record.second; count1s++; } if (currentTimestamp - record.first <= 10.0f) { sum10s += record.second; count10s++; } }
            avg1s = (count1s > 0) ? (sum1s / count1s) : dt; avg10s = (count10s > 0) ? (sum10s / count10s) : dt;

            if (telemetryMenuOpen) { menuOpenFrames++; menuOpenTimeElapsed += dt; }
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
            workerRunning = false; if (workerThread.joinable()) workerThread.join();
            vkDeviceWaitIdle(device); ImGui_ImplVulkan_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); megaArena.Cleanup();
            vkDestroyDescriptorPool(device, descriptorPool, nullptr); vkDestroySemaphore(device, renderFinishedSemaphore, nullptr); vkDestroySemaphore(device, imageAvailableSemaphore, nullptr); vkDestroyFence(device, inFlightFence, nullptr);
            vkDestroyImageView(device, depthImageView, nullptr); vkDestroyImage(device, depthImage, nullptr); vkFreeMemory(device, depthImageMemory, nullptr);
            vkDestroyPipeline(device, graphicsPipeline, nullptr); vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            for(auto fb : swapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr); for(auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
            vkDestroySwapchainKHR(device, swapChain, nullptr); vkDestroyCommandPool(device, commandPool, nullptr); vkDestroyRenderPass(device, renderPass, nullptr); vkDestroyDevice(device, nullptr); vkDestroyInstance(instance, nullptr); glfwDestroyWindow(window); glfwTerminate();
        }

        void drawFrame() {
            ProcessTaskQueues();

            vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX); vkResetFences(device, 1, &inFlightFence);
            uint32_t imageIndex; vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            ImGui_ImplVulkan_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

            float ramMb = 0.0f;
            #ifdef _WIN32
            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) ramMb = pmc.WorkingSetSize / (1024.0f * 1024.0f);
            #endif

            uint32_t activeVertices = megaArena.currentVertexOffset.load();
            uint32_t activeIndices = megaArena.currentIndexOffset.load();
            uint32_t activeCmds = megaArena.currentCommandCount.load();
            float activeVramMb = ((activeVertices * sizeof(Vertex)) + (activeIndices * sizeof(uint32_t)) + (activeCmds * sizeof(VkDrawIndexedIndirectCommand))) / (1024.0f * 1024.0f);
            float totalMegaArenaCapacity = ((megaArena.MAX_VERTICES * sizeof(Vertex)) + (megaArena.MAX_INDICES * sizeof(uint32_t)) + (megaArena.MAX_COMMANDS * sizeof(VkDrawIndexedIndirectCommand))) / (1024.0f * 1024.0f);

            int totalPendingUploads = mainThreadTaskQueue.size();
            bool isWorkerThrottledFlag = false;
            {
                std::lock_guard<std::mutex> lock(uploadMutex);
                totalPendingUploads += uploadQueue.size();
                isWorkerThrottledFlag = uploadQueue.size() >= 2000;
            }

            int totalPendingUnloads = 0;
            {
                std::lock_guard<std::mutex> lock(unloadMutex);
                totalPendingUnloads = unloadQueue.size();
            }

            HUDMetrics metrics = {
                playerHP, maxHP, selectedHotbarSlot, hotbarBlocks, telemetryMenuOpen,
                deltaTime, avg1s, avg10s, bestFrameTime, worstFrameTime,
                currentlyDrawnChunks, static_cast<int>(cullArray.size()),
                activeVertices, activeIndices,
                ramMb, activeVramMb, totalMegaArenaCapacity, static_cast<int>(freeVramSlots.size()),
                totalPendingUploads, totalPendingUnloads, isWorkerThrottledFlag,
                &renderDistance, isMouseCaptured,
                player.position.x, player.position.y, player.position.z,
                static_cast<int64_t>(std::floor(cameraPos.x / 16.0f)), static_cast<int64_t>(std::floor(cameraPos.y / 16.0f)), static_cast<int64_t>(std::floor(cameraPos.z / 16.0f)),
                player.velocity.x, player.velocity.y, player.velocity.z,
                cameraFront.x, cameraFront.y, cameraFront.z,
                player.isFlying, player.isGrounded,
                WIDTH, HEIGHT
            };

            TelemetryHUD::DrawInGameHUD(metrics);
            ImGui::Render();

            vkResetCommandBuffer(commandBuffer, 0); CommandManager::BeginCommandBuffer(commandBuffer);
            VkRenderPassBeginInfo renderPassInfo{}; renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; renderPassInfo.renderPass = renderPass; renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex]; renderPassInfo.renderArea.extent = swapChainExtent;
            std::array<VkClearValue, 2> clearValues{}; clearValues[0].color = VkClearColorValue{0.05f, 0.6f, 0.8f, 1.0f}; clearValues[1].depthStencil = VkClearDepthStencilValue{1.0f, 0};
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size()); renderPassInfo.pClearValues = clearValues.data();
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            // A drawFrame() függvény belsejében, a vkCmdBindPipeline után:
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            // --- EZ A SOR A KULCS! ---
            // A mainDescriptorSet-et kell ide átadnod, ami a TextureArray-t tartalmazza.
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &mainDescriptorSet, 0, nullptr);

            Mat4 view = Mat4::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
            Mat4 proj = Mat4::perspective(45.0f * (M_PI / 180.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f);
            Mat4 vp = proj * view;
            Frustum f = Frustum::extract(vp);

            visibleCommands.clear();
            currentlyDrawnChunks = 0;

            for (const auto& cull : cullArray) {
                if (f.isBoxVisible(cull.minX, cull.minY, cull.minZ, cull.maxX, cull.maxY, cull.maxZ)) {
                    visibleCommands.push_back(masterCommands[cull.cmdIndex]);
                    currentlyDrawnChunks++;
                }
            }

            if (!visibleCommands.empty()) {
                memcpy(megaArena.mappedIndirectData, visibleCommands.data(), visibleCommands.size() * sizeof(VkDrawIndexedIndirectCommand));

                VkBuffer vertexBuffers[] = {megaArena.vertexBuffer}; VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); vkCmdBindIndexBuffer(commandBuffer, megaArena.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Mat4), &vp);
                vkCmdDrawIndexedIndirect(commandBuffer, megaArena.indirectBuffer, 0, visibleCommands.size(), sizeof(VkDrawIndexedIndirectCommand));
            }

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
            vkCmdEndRenderPass(commandBuffer); CommandManager::EndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[] = {imageAvailableSemaphore}; VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1; submitInfo.pWaitSemaphores = waitSemaphores; submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &commandBuffer;
            VkSemaphore signalSemaphores[] = {renderFinishedSemaphore}; submitInfo.signalSemaphoreCount = 1; submitInfo.pSignalSemaphores = signalSemaphores;

            vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
            VkPresentInfoKHR presentInfo{}; presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR; presentInfo.waitSemaphoreCount = 1; presentInfo.pWaitSemaphores = signalSemaphores; presentInfo.swapchainCount = 1; presentInfo.pSwapchains = &swapChain; presentInfo.pImageIndices = &imageIndex;
            vkQueuePresentKHR(graphicsQueue, &presentInfo);
        }
    };
}