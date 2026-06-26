//
// Fájl: Render/TextureManager.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Leírás: Vulkan 2D Texture Array betöltő a Lock-Free Palettához (32x32)
//
#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include "../libs/stb_image.h"
#include "VulkanBuffer.hpp"
#include "VulkanCommandManager.hpp"

namespace NF::Render {

    class TextureManager {
    public:
        VkImage textureArrayImage = VK_NULL_HANDLE;
        VkDeviceMemory textureArrayMemory = VK_NULL_HANDLE;
        VkImageView textureArrayView = VK_NULL_HANDLE;
        VkSampler textureSampler = VK_NULL_HANDLE;

        void LoadBlockTextures(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& folderPath) {
            // BEÁLLÍTVA 32x32-RE
            const int texWidth = 32;
            const int texHeight = 32;
            const uint32_t layerCount = 256;
            const VkDeviceSize layerSize = texWidth * texHeight * 4; // 32 * 32 * 4 (RGBA)
            const VkDeviceSize imageSize = layerSize * layerCount;

            // 1. STAGING BUFFER: Ebbe töltjük be a CPU-n a képeket
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;

            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = imageSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

            VkMemoryRequirements memReqs;
            vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);
            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memReqs.size;
            allocInfo.memoryTypeIndex = VulkanBuffer::FindMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);
            vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

            // 2. KÉPEK BEOLVASÁSA ÉS MÁSOLÁSA A STAGING BUFFERBE
            void* data;
            vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);

            for (uint32_t i = 0; i < layerCount; ++i) {
                std::string filePath = folderPath + "/" + std::to_string(i) + ".png";
                int width, height, channels;

                stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

                VkDeviceSize offset = i * layerSize;

                if (!pixels) {
                    // 32x32-es Missing Texture (Lila-fekete sakktábla) generálása a VRAM-ban
                    std::vector<uint8_t> fallback(layerSize, 0);
                    for(int y = 0; y < texHeight; y++) {
                        for(int x = 0; x < texWidth; x++) {
                            bool isMagenta = ((x / 16) + (y / 16)) % 2 == 0;
                            int pOffset = (y * texWidth + x) * 4;
                            fallback[pOffset + 0] = isMagenta ? 255 : 0;   // R
                            fallback[pOffset + 1] = 0;                     // G
                            fallback[pOffset + 2] = isMagenta ? 255 : 0;   // B
                            fallback[pOffset + 3] = i == 0 ? 0 : 255;      // A
                        }
                    }
                    memcpy(static_cast<uint8_t*>(data) + offset, fallback.data(), layerSize);
                } else {
                    if (width != texWidth || height != texHeight) {
                        std::cerr << "[Textura] Hiba: A " << filePath << " nem " << texWidth << "x" << texHeight << " felbontasu!\n";
                    } else {
                        memcpy(static_cast<uint8_t*>(data) + offset, pixels, layerSize);
                    }
                    stbi_image_free(pixels);
                }
            }
            vkUnmapMemory(device, stagingBufferMemory);

            // 3. VULKAN IMAGE (TEXTURE ARRAY) LÉTREHOZÁSA A GPU-N
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = texWidth;
            imageInfo.extent.height = texHeight;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = layerCount;
            imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vkCreateImage(device, &imageInfo, nullptr, &textureArrayImage);

            vkGetImageMemoryRequirements(device, textureArrayImage, &memReqs);
            allocInfo.allocationSize = memReqs.size;
            allocInfo.memoryTypeIndex = VulkanBuffer::FindMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(device, &allocInfo, nullptr, &textureArrayMemory);
            vkBindImageMemory(device, textureArrayImage, textureArrayMemory, 0);

            // 4. MÁSOLÁS A STAGING BUFFERBŐL A GPU VRAM-BA
            VkCommandBuffer cmd = CommandManager::CreateCommandBuffer(device, commandPool);
            CommandManager::BeginCommandBuffer(cmd);

            TransitionImageLayout(cmd, textureArrayImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layerCount);

            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = layerCount;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {(uint32_t)texWidth, (uint32_t)texHeight, 1};
            vkCmdCopyBufferToImage(cmd, stagingBuffer, textureArrayImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            TransitionImageLayout(cmd, textureArrayImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, layerCount);

            CommandManager::EndCommandBuffer(cmd);
            VkSubmitInfo submitInfo{}; submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &cmd;
            vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(graphicsQueue);
            vkFreeCommandBuffers(device, commandPool, 1, &cmd);

            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);

            // 5. IMAGE VIEW ÉS SAMPLER LÉTREHOZÁSA
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = textureArrayImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = layerCount;
            vkCreateImageView(device, &viewInfo, nullptr, &textureArrayView);

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);

            std::cout << "[Textura] Texture Array sikeresen lefoglalva es feltoltve a VRAM-ba!\n";
        }

        void Cleanup(VkDevice device) {
            vkDestroySampler(device, textureSampler, nullptr);
            vkDestroyImageView(device, textureArrayView, nullptr);
            vkDestroyImage(device, textureArrayImage, nullptr);
            vkFreeMemory(device, textureArrayMemory, nullptr);
        }

    private:
        void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = layerCount;

            VkPipelineStageFlags sourceStage;
            VkPipelineStageFlags destinationStage;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            } else {
                throw std::invalid_argument("[Vulkan] Nem tamogatott layout valtas!");
            }

            vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    };
}