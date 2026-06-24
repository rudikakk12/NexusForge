#pragma once

#include <vector>
#include <cstdint>
#include <fstream>
#include <string>
#include "../Core/MacroChunk.hpp"

namespace NF::Render {

    // A Vertex struktúra pontos definíciója
    struct Vertex {
        float x, y, z;       // Pozíció
        float nx, ny, nz;    // Normálvektor
        uint32_t paletteID;  // Szín / Textúra azonosító
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    class BasicMesher {
    private:
        static inline int GetIndex(int x, int y, int z) {
            return x + (y * 16) + (z * 256);
        }

        static inline bool IsTransparent(const uint8_t* voxels, int x, int y, int z) {
            if (x < 0 || x >= 16 || y < 0 || y >= 16 || z < 0 || z >= 16) return true;
            return voxels[GetIndex(x, y, z)] == 0;
        }

        static void AddFace(MeshData& mesh, int x, int y, int z, int faceDir, uint8_t paletteID) {
            uint32_t v_idx = static_cast<uint32_t>(mesh.vertices.size());

            float nx = 0, ny = 0, nz = 0;
            float v[4][3];

            switch(faceDir) {
                case 0: ny = 1.0f; v[0][0]=0;v[0][1]=1;v[0][2]=0; v[1][0]=1;v[1][1]=1;v[1][2]=0; v[2][0]=1;v[2][1]=1;v[2][2]=1; v[3][0]=0;v[3][1]=1;v[3][2]=1; break;
                case 1: ny = -1.0f; v[0][0]=0;v[0][1]=0;v[0][2]=1; v[1][0]=1;v[1][1]=0;v[1][2]=1; v[2][0]=1;v[2][1]=0;v[2][2]=0; v[3][0]=0;v[3][1]=0;v[3][2]=0; break;
                case 2: nx = 1.0f; v[0][0]=1;v[0][1]=0;v[0][2]=0; v[1][0]=1;v[1][1]=1;v[1][2]=0; v[2][0]=1;v[2][1]=1;v[2][2]=1; v[3][0]=1;v[3][1]=0;v[3][2]=1; break;
                case 3: nx = -1.0f; v[0][0]=0;v[0][1]=0;v[0][2]=1; v[1][0]=0;v[1][1]=1;v[1][2]=1; v[2][0]=0;v[2][1]=1;v[2][2]=0; v[3][0]=0;v[3][1]=0;v[3][2]=0; break;
                case 4: nz = 1.0f; v[0][0]=1;v[0][1]=0;v[0][2]=1; v[1][0]=1;v[1][1]=1;v[1][2]=1; v[2][0]=0;v[2][1]=1;v[2][2]=1; v[3][0]=0;v[3][1]=0;v[3][2]=1; break;
                case 5: nz = -1.0f; v[0][0]=0;v[0][1]=0;v[0][2]=0; v[1][0]=0;v[1][1]=1;v[1][2]=0; v[2][0]=1;v[2][1]=1;v[2][2]=0; v[3][0]=1;v[3][1]=0;v[3][2]=0; break;
            }

            for(int i = 0; i < 4; ++i) {
                mesh.vertices.push_back({
                    (float)x + v[i][0], (float)y + v[i][1], (float)z + v[i][2],
                    nx, ny, nz,
                    paletteID
                });
            }

            mesh.indices.push_back(v_idx + 0); mesh.indices.push_back(v_idx + 1); mesh.indices.push_back(v_idx + 2);
            mesh.indices.push_back(v_idx + 2); mesh.indices.push_back(v_idx + 3); mesh.indices.push_back(v_idx + 0);
        }

    public:
        static MeshData GenerateMesh(NF::Core::MacroChunk_Small& chunk) {
            MeshData mesh;
            mesh.vertices.reserve(4096);
            mesh.indices.reserve(6144);
            const uint8_t* voxels = chunk.tickAfter;

            for (int z = 0; z < 16; ++z) {
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 16; ++x) {
                        uint8_t voxelID = voxels[GetIndex(x, y, z)];
                        if (voxelID == 0) continue;
                        if (IsTransparent(voxels, x, y + 1, z)) AddFace(mesh, x, y, z, 0, voxelID);
                        if (IsTransparent(voxels, x, y - 1, z)) AddFace(mesh, x, y, z, 1, voxelID);
                        if (IsTransparent(voxels, x + 1, y, z)) AddFace(mesh, x, y, z, 2, voxelID);
                        if (IsTransparent(voxels, x - 1, y, z)) AddFace(mesh, x, y, z, 3, voxelID);
                        if (IsTransparent(voxels, x, y, z + 1)) AddFace(mesh, x, y, z, 4, voxelID);
                        if (IsTransparent(voxels, x, y, z - 1)) AddFace(mesh, x, y, z, 5, voxelID);
                    }
                }
            }
            return mesh;
        }

        static void ExportToOBJ(const MeshData& mesh, const std::string& filename) {
            std::ofstream out(filename);
            if (!out.is_open()) return;
            for (const auto& v : mesh.vertices) out << "v " << v.x << " " << v.y << " " << v.z << "\n";
            for (const auto& v : mesh.vertices) out << "vn " << v.nx << " " << v.ny << " " << v.nz << "\n";
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                uint32_t i1 = mesh.indices[i] + 1;
                uint32_t i2 = mesh.indices[i+1] + 1;
                uint32_t i3 = mesh.indices[i+2] + 1;
                out << "f " << i1 << "//" << i1 << " " << i2 << "//" << i2 << " " << i3 << "//" << i3 << "\n";
            }
            out.close();
        }
    };
}