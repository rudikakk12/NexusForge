//
// Fájl: Render/VulkanMegaArena.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Architektúra: Multi-Draw Indirect (MDI) Mega-Bufferek
//
#pragma once
#include <vulkan/vulkan.h>
#include <atomic>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <cstring>
#include "VulkanBuffer.hpp"
#include "BasicMesher.hpp"

namespace NF::Render {

    class VulkanMegaArena {
    private:
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    public:
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        Vertex* mappedVertexData = nullptr;

        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        uint32_t* mappedIndexData = nullptr;

        VkBuffer indirectBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indirectMemory = VK_NULL_HANDLE;
        VkDrawIndexedIndirectCommand* mappedIndirectData = nullptr;

        std::vector<VkDrawIndexedIndirectCommand> shadowIndirectData;

        std::atomic<uint32_t> currentVertexOffset{0};
        std::atomic<uint32_t> currentIndexOffset{0};
        std::atomic<uint32_t> currentCommandCount{0};

        // ==============================================================
        // BIZTONSÁGOS HOST_VISIBLE SKÁLÁZÁS (ReBAR limitekhez igazítva)
        // Összes lefoglalt VRAM: ~3.4 GB (Minden kártyán garantáltan lefut)
        // ==============================================================
        const uint32_t MAX_VERTICES = 100000000;  // 100 millió vertex (~2.8 GB VRAM)
        const uint32_t MAX_INDICES  = 150000000;  // 150 millió index (~600 MB VRAM)
        const uint32_t MAX_COMMANDS = 250000;     // 250 ezer command (~5 MB VRAM)
        void Initialize(VkDevice dev, VkPhysicalDevice pDev) {
            device = dev;
            physicalDevice = pDev;

            shadowIndirectData.resize(MAX_COMMANDS);

            // VERTEX BUFFER ALLOKÁCIÓ (7 GB)
            VkDeviceSize vSize = MAX_VERTICES * sizeof(Vertex);
            VulkanBuffer::CreateBuffer(device, physicalDevice, vSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexBuffer, vertexMemory);
            vkMapMemory(device, vertexMemory, 0, vSize, 0, (void**)&mappedVertexData);

            // INDEX BUFFER ALLOKÁCIÓ (1.4 GB)
            VkDeviceSize iSize = MAX_INDICES * sizeof(uint32_t);
            VulkanBuffer::CreateBuffer(device, physicalDevice, iSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                indexBuffer, indexMemory);
            vkMapMemory(device, indexMemory, 0, iSize, 0, (void**)&mappedIndexData);

            // INDIRECT BUFFER ALLOKÁCIÓ (5 MB)
            VkDeviceSize cmdSize = MAX_COMMANDS * sizeof(VkDrawIndexedIndirectCommand);
            VulkanBuffer::CreateBuffer(device, physicalDevice, cmdSize,
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                indirectBuffer, indirectMemory);
            vkMapMemory(device, indirectMemory, 0, cmdSize, 0, (void**)&mappedIndirectData);

            std::cout << "[VulkanMegaArena] TITAN MDI Bufferek felkeszitve RTX 3060 Profilra (8.4 GB)!" << std::endl;
        }

        bool Allocate(uint32_t vCount, uint32_t iCount, uint32_t& outCmdIdx, uint32_t& outVOffset, uint32_t& outIOffset) {
            uint32_t predictedV = currentVertexOffset.load(std::memory_order_relaxed) + vCount;
            uint32_t predictedI = currentIndexOffset.load(std::memory_order_relaxed) + iCount;
            uint32_t predictedCmd = currentCommandCount.load(std::memory_order_relaxed) + 1;

            // Biztonsági VRAM sapka!
            if (predictedV > MAX_VERTICES || predictedI > MAX_INDICES || predictedCmd >= MAX_COMMANDS) {
                return false;
            }

            outVOffset = currentVertexOffset.fetch_add(vCount, std::memory_order_relaxed);
            outIOffset = currentIndexOffset.fetch_add(iCount, std::memory_order_relaxed);
            outCmdIdx = currentCommandCount.fetch_add(1, std::memory_order_relaxed);

            return true;
        }

        void Cleanup() {
            if (device == VK_NULL_HANDLE) return;
            vkUnmapMemory(device, vertexMemory); vkDestroyBuffer(device, vertexBuffer, nullptr); vkFreeMemory(device, vertexMemory, nullptr);
            vkUnmapMemory(device, indexMemory);  vkDestroyBuffer(device, indexBuffer, nullptr);  vkFreeMemory(device, indexMemory, nullptr);
            vkUnmapMemory(device, indirectMemory);vkDestroyBuffer(device, indirectBuffer, nullptr);vkFreeMemory(device, indirectMemory, nullptr);
        }
    };
}