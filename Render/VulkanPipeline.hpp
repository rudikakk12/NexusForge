#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>

namespace NF::Render {

    class VulkanPipeline {
    public:
        static VkRenderPass CreateRenderPass(VkDevice device, VkFormat swapChainImageFormat) {
            // A "vászon" (attachment) leírása: töröljük ki minden frame elején
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = swapChainImageFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    // Törlés
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Megtartás a megjelenítéshez
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            // Al-renderpass (a modern GPU-k így optimalizálnak)
            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            // RenderPass létrehozása
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;

            VkRenderPass renderPass;
            if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                throw std::runtime_error("[Vulkan] RenderPass letrehozasa sikertelen!");
            }
            return renderPass;
        }
    };
}