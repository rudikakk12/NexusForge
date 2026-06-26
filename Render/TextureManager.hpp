// Koncepció: Render/TextureManager.hpp
#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace NF::Render {

    class TextureArrayManager {
    public:
        VkImage textureArrayImage;
        VkDeviceMemory textureArrayMemory;
        VkImageView textureArrayView;
        VkSampler textureSampler;

        // Ezzel töltünk be egy mappát a GPU-ra
        void LoadBlockTextures(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& folderPath) {

            int texWidth = 16, texHeight = 16, texChannels = 4;
            uint32_t layerCount = 256; // Max 256 féle blokk egyelőre

            // 1. VkImage létrehozása VK_IMAGE_CREATE_INFO segítségével,
            // ahol az arrayLayers = layerCount.
            // 2. Egy nagy Staging Buffer létrehozása.
            // 3. For ciklus 1-től layerCount-ig, ami az stb_image-dzsel beolvassa
            //    a fájlokat (pl. folderPath + "/" + std::to_string(id) + ".png")
            //    és bemásolja a Staging Buffer megfelelő eltolásához (offset).
            // 4. vkCmdCopyBufferToImage utasítással áttoljuk a VRAM-ba a teljes tömböt.
            // 5. VK_IMAGE_VIEW_TYPE_2D_ARRAY típusú ImageView létrehozása.

            // A Shader-ben pedig ez ennyi lesz:
            // layout(binding = 1) uniform sampler2DArray blockTextures;
            // vec4 color = texture(blockTextures, vec3(uv.x, uv.y, inGlobalBlockID));
        }
    };
}