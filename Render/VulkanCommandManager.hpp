#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

namespace NF::Render {

    class CommandManager {
    public:
        // 1. Létrehozzuk a CommandPool-t, ami a buffereket menedzseli
        static VkCommandPool CreateCommandPool(VkDevice device, uint32_t queueFamilyIndex) {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Engedélyezi az újrahasznosítást
            poolInfo.queueFamilyIndex = queueFamilyIndex;

            VkCommandPool commandPool;
            if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Nem sikerült létrehozni a CommandPool-t!");
            }
            return commandPool;
        }

        // 2. Lefoglaljuk a parancsbuffert
        static VkCommandBuffer CreateCommandBuffer(VkDevice device, VkCommandPool commandPool) {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Nem sikerült lefoglalni a CommandBuffer-t!");
            }
            return commandBuffer;
        }

        // 3. A parancsok rögzítésének indítása
        static void BeginCommandBuffer(VkCommandBuffer commandBuffer) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Nem sikerült elkezdeni a parancsrögzítést!");
            }
        }

        // 4. A parancsok lezárása
        static void EndCommandBuffer(VkCommandBuffer commandBuffer) {
            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Nem sikerült lezárni a parancsbuffert!");
            }
        }
    };
}