#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

namespace NF::Render {

    struct SwapChainData {
        VkSwapchainKHR swapChain;
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
        VkExtent2D extent;
    };

    class VulkanSwapchain {
    public:
        static SwapChainData Create(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
            // Itt a Swapchain létrehozásának a váza
            // Valós motorban itt ellenőrizzük a Surface képességeket (Capabilities)

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;
            createInfo.minImageCount = 2; // Double buffering
            createInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
            createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            createInfo.imageExtent = {width, height};
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
            createInfo.clipped = VK_TRUE;

            SwapChainData data;
            data.extent = {width, height};

            if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &data.swapChain) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] Hiba: A Swapchain letrehozasa meghiusult!");
            }

            // Képek lekérése
            uint32_t imageCount;
            vkGetSwapchainImagesKHR(device, data.swapChain, &imageCount, nullptr);
            data.images.resize(imageCount);
            vkGetSwapchainImagesKHR(device, data.swapChain, &imageCount, data.images.data());

            // Image View-k létrehozása (ezek kellenek a megjelenítéshez)
            data.imageViews.resize(imageCount);
            for (size_t i = 0; i < imageCount; i++) {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = data.images[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
                viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_VIEW_CREATE_INFO; // Itt a flagek finomhangolása kell
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;

                if (vkCreateImageView(device, &viewInfo, nullptr, &data.imageViews[i]) != VK_SUCCESS) {
                    throw std::runtime_error("[Vulkan] Hiba: Nem sikerult a ImageView letrehozasa!");
                }
            }

            return data;
        }
    };
}