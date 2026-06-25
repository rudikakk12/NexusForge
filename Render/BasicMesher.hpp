//
// Fájl: Render/BasicMesher.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Architektúra: Perfect 3D Greedy Meshing
//
#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <cstdint>
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
        static inline uint8_t GetVoxelSafe(const uint8_t* voxels, int x, int y, int z) {
            if (x < 0 || x >= 16 || y < 0 || y >= 16 || z < 0 || z >= 16) return 0;
            return voxels[x + (y * 16) + (z * 256)];
        }

        // Tökéletesített Vektor-alapú Quad Generátor
        static void EmitQuad(MeshData& mesh, int* quad_x, int d, int u, int v,
                             int width, int height, bool isBackFace, uint32_t id,
                             int ox, int oy, int oz)
        {
            // Globális eltolás beállítása
            float gx = quad_x[0] + (ox * 16.0f);
            float gy = quad_x[1] + (oy * 16.0f);
            float gz = quad_x[2] + (oz * 16.0f);

            // Irányvektorok a szélességhez (u) és magassághoz (v)
            float du[3] = {0.0f, 0.0f, 0.0f};
            du[u] = static_cast<float>(width);

            float dv[3] = {0.0f, 0.0f, 0.0f};
            dv[v] = static_cast<float>(height);

            // Normálvektor kiszámítása a d (axis) alapján
            float nx = 0, ny = 0, nz = 0;
            if (d == 0) nx = isBackFace ? -1.0f : 1.0f;
            if (d == 1) ny = isBackFace ? -1.0f : 1.0f;
            if (d == 2) nz = isBackFace ? -1.0f : 1.0f;

            uint32_t v_idx = static_cast<uint32_t>(mesh.vertices.size());

            // A Quad 4 sarka vektormatekkal (Garantáltan nem torzul el!)
            mesh.vertices.push_back({ gx, gy, gz, nx, ny, nz, id });
            mesh.vertices.push_back({ gx + du[0], gy + du[1], gz + du[2], nx, ny, nz, id });
            mesh.vertices.push_back({ gx + du[0] + dv[0], gy + du[1] + dv[1], gz + du[2] + dv[2], nx, ny, nz, id });
            mesh.vertices.push_back({ gx + dv[0], gy + dv[1], gz + dv[2], nx, ny, nz, id });

            // Vulkan CCW (Counter-Clockwise) Winding Order
            if (!isBackFace) {
                mesh.indices.insert(mesh.indices.end(), {v_idx, v_idx+1, v_idx+2, v_idx+2, v_idx+3, v_idx});
            } else {
                mesh.indices.insert(mesh.indices.end(), {v_idx, v_idx+3, v_idx+2, v_idx+2, v_idx+1, v_idx});
            }
        }

    public:
        static MeshData GenerateMesh(NF::Core::MacroChunk_Small& chunk, int offsetX = 0, int offsetY = 0, int offsetZ = 0) {
            MeshData mesh;
            mesh.vertices.reserve(1024);
            mesh.indices.reserve(1536);
            const uint8_t* voxels = chunk.tickAfter;

            // d = 0 (X tengely), d = 1 (Y tengely), d = 2 (Z tengely)
            for (int d = 0; d < 3; ++d) {
                // Vektormatek: XxY=Z, YxZ=X, ZxX=Y (Garantálja a jó cullingot)
                int u = (d == 0) ? 1 : (d == 1) ? 2 : 0;
                int v = (d == 0) ? 2 : (d == 1) ? 0 : 1;

                int x[3] = {0, 0, 0};
                int q[3] = {0, 0, 0};
                q[d] = 1;

                // -1-től megyünk, hogy a levegő/chunk határ látszódjon
                for (x[d] = -1; x[d] < 16; ++x[d]) {
                    uint32_t maskPos[256] = {0};
                    uint32_t maskNeg[256] = {0};

                    for (x[v] = 0; x[v] < 16; ++x[v]) {
                        for (x[u] = 0; x[u] < 16; ++x[u]) {

                            // A C++ tömb-varázslat: az x[0,1,2] automatikusan beáll a jó tengelyre!
                            uint8_t b1 = (x[d] >= 0) ? GetVoxelSafe(voxels, x[0], x[1], x[2]) : 0;
                            uint8_t b2 = (x[d] < 15) ? GetVoxelSafe(voxels, x[0]+q[0], x[1]+q[1], x[2]+q[2]) : 0;

                            bool solid1 = (b1 != 0);
                            bool solid2 = (b2 != 0);

                            if (solid1 && !solid2) {
                                maskPos[x[u] + x[v] * 16] = chunk.PaletteGlobalBlockStateIDs[b1];
                            } else if (!solid1 && solid2) {
                                maskNeg[x[u] + x[v] * 16] = chunk.PaletteGlobalBlockStateIDs[b2];
                            }
                        }
                    }

                    // A belső felfaló rutin (A "Mohó" rész)
                    auto SweepMask = [&](uint32_t* mask, bool isBackFace) {
                        for (int j = 0; j < 16; ++j) {
                            for (int i = 0; i < 16; ) {
                                uint32_t id = mask[i + j * 16];
                                if (id != 0) {

                                    // 1. Horizontális szélesség
                                    int width = 1;
                                    while (i + width < 16 && mask[(i + width) + j * 16] == id) {
                                        width++;
                                    }

                                    // 2. Vertikális magasság
                                    int height = 1;
                                    bool done = false;
                                    while (j + height < 16) {
                                        for (int k = 0; k < width; ++k) {
                                            if (mask[(i + k) + (j + height) * 16] != id) {
                                                done = true; break;
                                            }
                                        }
                                        if (done) break;
                                        height++;
                                    }

                                    // A Quad pontos helyének kiszámítása a térben
                                    int quad_x[3] = {0, 0, 0};
                                    quad_x[d] = x[d] + 1; // A lap mindig a határra esik!
                                    quad_x[u] = i;
                                    quad_x[v] = j;

                                    EmitQuad(mesh, quad_x, d, u, v, width, height, isBackFace, id, offsetX, offsetY, offsetZ);

                                    // 3. Maszk tisztítása
                                    for (int l = 0; l < height; ++l) {
                                        for (int k = 0; k < width; ++k) {
                                            mask[(i + k) + (j + l) * 16] = 0;
                                        }
                                    }
                                    i += width;
                                } else {
                                    i++;
                                }
                            }
                        }
                    };

                    SweepMask(maskPos, false);
                    SweepMask(maskNeg, true);
                }
            }
            return mesh;
        }
    };
}