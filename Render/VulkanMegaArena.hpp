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
#include "VulkanBuffer.hpp"
#include "BasicMesher.hpp" // A Vertex struktúra miatt

namespace NF::Render {

    class VulkanMegaArena {
    private:
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    public:
        // --- 1. VERTEX MEGA-BUFFER (~160 MB) ---
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
        Vertex* mappedVertexData = nullptr;

        // --- 2. INDEX MEGA-BUFFER (~40 MB) ---
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        uint32_t* mappedIndexData = nullptr;

        // --- 3. INDIRECT COMMAND BUFFER (~2 MB) ---
        // Ez a Vulkán szabványos 20 bájtos struktúrája. A GPU ezt olvassa végig!
        VkBuffer indirectBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indirectMemory = VK_NULL_HANDLE;
        VkDrawIndexedIndirectCommand* mappedIndirectData = nullptr;

        // --- ATOMI MUTATÓK AZ ALLOKÁCIÓHOZ (Lock-Free) ---
        std::atomic<uint32_t> currentVertexOffset{0};
        std::atomic<uint32_t> currentIndexOffset{0};
        std::atomic<uint32_t> currentCommandCount{0};

        // Skálázott kapacitás a tesztvilághoz (128+ chunk bőségesen elfér)
        const uint32_t MAX_VERTICES = 5000000;  // 5 millió vertex
        const uint32_t MAX_INDICES  = 10000000; // 10 millió index
        const uint32_t MAX_COMMANDS = 100000;   // 100 ezer draw command

        void Initialize(VkDevice dev, VkPhysicalDevice pDev) {
            device = dev;
            physicalDevice = pDev;

            // Vertex Buffer létrehozása
            VkDeviceSize vSize = MAX_VERTICES * sizeof(Vertex);
            VulkanBuffer::CreateBuffer(device, physicalDevice, vSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexBuffer, vertexMemory);
            vkMapMemory(device, vertexMemory, 0, vSize, 0, (void**)&mappedVertexData);

            // Index Buffer létrehozása
            VkDeviceSize iSize = MAX_INDICES * sizeof(uint32_t);
            VulkanBuffer::CreateBuffer(device, physicalDevice, iSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                indexBuffer, indexMemory);
            vkMapMemory(device, indexMemory, 0, iSize, 0, (void**)&mappedIndexData);

            // Indirect Command Buffer létrehozása
            VkDeviceSize cmdSize = MAX_COMMANDS * sizeof(VkDrawIndexedIndirectCommand);
            VulkanBuffer::CreateBuffer(device, physicalDevice, cmdSize,
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                indirectBuffer, indirectMemory);
            vkMapMemory(device, indirectMemory, 0, cmdSize, 0, (void**)&mappedIndirectData);

            std::cout << "[VulkanMegaArena] MDI Mega-Bufferek lefoglalva a GPU-n!" << std::endl;
        }

        // LOCK-FREE ALLOKÁTOR A MESHER SZÁLAKNAK
        bool Allocate(uint32_t vCount, uint32_t iCount, uint32_t& outCmdIdx, uint32_t& outVOffset, uint32_t& outIOffset) {
            outVOffset = currentVertexOffset.fetch_add(vCount, std::memory_order_relaxed);
            outIOffset = currentIndexOffset.fetch_add(iCount, std::memory_order_relaxed);
            outCmdIdx = currentCommandCount.fetch_add(1, std::memory_order_relaxed);

            if (outVOffset + vCount > MAX_VERTICES || outIOffset + iCount > MAX_INDICES || outCmdIdx >= MAX_COMMANDS) {
                std::cerr << "[MDI] CRITICAL: A Mega-Buffer megtelt! Nincs tobb VRAM hely." << std::endl;
                return false;
            }
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