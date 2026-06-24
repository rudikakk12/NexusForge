#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>

namespace NF::Render {

    class VulkanBuffer {
    public:
        // Megkeresi a videókártyán a megfelelő típusú memóriát (pl. olyat, amit a CPU is lát)
        static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }
            throw std::runtime_error("[Vulkan] Nem talalhato megfelelo memoria tipus a GPU-n!");
        }

        // Létrehoz egy buffert, és memóriát is rendel hozzá
        static void CreateBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, 
                                 VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
                                 VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
            
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Nem sikerult a buffer letrehozasa!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Nem sikerult memoriat foglalni a buffernek!");
            }

            // Összekötjük a buffert a lefoglalt memóriával
            vkBindBufferMemory(device, buffer, bufferMemory, 0);
        }
    };
}