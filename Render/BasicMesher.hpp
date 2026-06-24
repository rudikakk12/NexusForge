#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <cstdint>
#include <fstream>
#include <string>
#include "../Core/MacroChunk.hpp"

namespace NF::Render {

    struct Vertex {
        float x, y, z;
        float nx, ny, nz;
        uint32_t paletteID;

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
            attributeDescriptions[0].binding = 0; attributeDescriptions[0].location = 0; attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; attributeDescriptions[0].offset = offsetof(Vertex, x);
            attributeDescriptions[1].binding = 0; attributeDescriptions[1].location = 1; attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; attributeDescriptions[1].offset = offsetof(Vertex, nx);
            attributeDescriptions[2].binding = 0; attributeDescriptions[2].location = 2; attributeDescriptions[2].format = VK_FORMAT_R32_UINT; attributeDescriptions[2].offset = offsetof(Vertex, paletteID);
            return attributeDescriptions;
        }
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    class BasicMesher {
    private:
        static inline int GetIndex(int x, int y, int z) { return x + (y * 16) + (z * 256); }
        static inline bool IsTransparent(const uint8_t* voxels, int x, int y, int z) {
            if (x < 0 || x >= 16 || y < 0 || y >= 16 || z < 0 || z >= 16) return true;
            return voxels[GetIndex(x, y, z)] == 0;
        }

        // JAVÍTVA: A paletteID típusa uint8_t-ről uint32_t-re cserélve (Globális ID), hogy a Vertex struktúrának és a Shadernek is megfeleljen
        static void AddFace(MeshData& mesh, int x, int y, int z, int faceDir, uint32_t globalPaletteID, int offsetX, int offsetY, int offsetZ) {
            uint32_t v_idx = static_cast<uint32_t>(mesh.vertices.size());

            float nx = 0, ny = 0, nz = 0; float v[4][3];

            switch(faceDir) {
                case 0: ny = 1.0f; v[0][0]=0;v[0][1]=1;v[0][2]=1; v[1][0]=1;v[1][1]=1;v[1][2]=1; v[2][0]=1;v[2][1]=1;v[2][2]=0; v[3][0]=0;v[3][1]=1;v[3][2]=0; break;
                case 1: ny = -1.0f; v[0][0]=0;v[0][1]=0;v[0][2]=0; v[1][0]=1;v[1][1]=0;v[1][2]=0; v[2][0]=1;v[2][1]=0;v[2][2]=1; v[3][0]=0;v[3][1]=0;v[3][2]=1; break;
                case 2: nx = 1.0f; v[0][0]=1;v[0][1]=0;v[0][2]=0; v[1][0]=1;v[1][1]=1;v[1][2]=0; v[2][0]=1;v[2][1]=1;v[2][2]=1; v[3][0]=1;v[3][1]=0;v[3][2]=1; break;
                case 3: nx = -1.0f; v[0][0]=0;v[0][1]=0;v[0][2]=1; v[1][0]=0;v[1][1]=1;v[1][2]=1; v[2][0]=0;v[2][1]=1;v[2][2]=0; v[3][0]=0;v[3][1]=0;v[3][2]=0; break;
                case 4: nz = 1.0f; v[0][0]=1;v[0][1]=0;v[0][2]=1; v[1][0]=1;v[1][1]=1;v[1][2]=1; v[2][0]=0;v[2][1]=1;v[2][2]=1; v[3][0]=0;v[3][1]=0;v[3][2]=1; break;
                case 5: nz = -1.0f; v[0][0]=0;v[0][1]=0;v[0][2]=0; v[1][0]=0;v[1][1]=1;v[1][2]=0; v[2][0]=1;v[2][1]=1;v[2][2]=0; v[3][0]=1;v[3][1]=0;v[3][2]=0; break;
            }

            // Kiszámoljuk a Globális pozíciót
            float gx = (float)x + (offsetX * 16.0f);
            float gy = (float)y + (offsetY * 16.0f);
            float gz = (float)z + (offsetZ * 16.0f);

            for(int i = 0; i < 4; ++i) {
                mesh.vertices.push_back({ gx + v[i][0], gy + v[i][1], gz + v[i][2], nx, ny, nz, globalPaletteID });
            }

            mesh.indices.push_back(v_idx + 0); mesh.indices.push_back(v_idx + 1); mesh.indices.push_back(v_idx + 2);
            mesh.indices.push_back(v_idx + 2); mesh.indices.push_back(v_idx + 3); mesh.indices.push_back(v_idx + 0);
        }

    public:
        // Fogadja a Chunk eltolását!
        static MeshData GenerateMesh(NF::Core::MacroChunk_Small& chunk, int offsetX = 0, int offsetY = 0, int offsetZ = 0) {
            MeshData mesh;
            mesh.vertices.reserve(4096);
            mesh.indices.reserve(6144);
            const uint8_t* voxels = chunk.tickAfter;

            for (int z = 0; z < 16; ++z) {
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 16; ++x) {
                        uint8_t localVoxelID = voxels[GetIndex(x, y, z)];
                        if (localVoxelID == 0) continue;

                        // JAVÍTÁS: A voxels[] a chunk LOKÁLIS paletta indexét tartalmazza.
                        // A Shadernek viszont a GLOBÁLIS BlockStateID-ra van szüksége a helyes színezéshez.
                        // Ezért kikeressük a globális ID-t a chunk palettájából a lokális azonosító alapján!
                        uint32_t globalID = chunk.PaletteGlobalBlockStateIDs[localVoxelID];

                        if (IsTransparent(voxels, x, y + 1, z)) AddFace(mesh, x, y, z, 0, globalID, offsetX, offsetY, offsetZ);
                        if (IsTransparent(voxels, x, y - 1, z)) AddFace(mesh, x, y, z, 1, globalID, offsetX, offsetY, offsetZ);
                        if (IsTransparent(voxels, x + 1, y, z)) AddFace(mesh, x, y, z, 2, globalID, offsetX, offsetY, offsetZ);
                        if (IsTransparent(voxels, x - 1, y, z)) AddFace(mesh, x, y, z, 3, globalID, offsetX, offsetY, offsetZ);
                        if (IsTransparent(voxels, x, y, z + 1)) AddFace(mesh, x, y, z, 4, globalID, offsetX, offsetY, offsetZ);
                        if (IsTransparent(voxels, x, y, z - 1)) AddFace(mesh, x, y, z, 5, globalID, offsetX, offsetY, offsetZ);
                    }
                }
            }
            return mesh;
        }
    };
}