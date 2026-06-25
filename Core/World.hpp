//
// Fájl: Core/World.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Architektúra: Infinite Spatial Hash Map
//
#pragma once
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cmath>
#include "MacroChunk.hpp"

namespace NF::Core {

    // 64-bites egyedi azonosító generálása a 3D koordinátákból!
    inline uint64_t GetChunkHash(int32_t cx, int32_t cy, int32_t cz) {
        uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(cx) & 0x3FFFFF);
        uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(cy) & 0xFFFFF);
        uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(cz) & 0x3FFFFF);
        return (ux << 42) | (uy << 22) | uz;
    }

    // A VÉGTELEN VILÁGUNK TÁROLÓJA!
    inline std::unordered_map<uint64_t, std::unique_ptr<MacroChunk_Small>> realWorld;

    // A Procedurális Generátor (Áthozva a main.cpp-ből)
    inline void GenerateTerrain(MacroChunk_Small& chunk, int cx, int cy, int cz) {
        // Alap paletta beállítása a shaderhez
        chunk.PaletteGlobalBlockStateIDs[1] = 1;
        chunk.PaletteGlobalBlockStateIDs[2] = 2;
        chunk.PaletteGlobalBlockStateIDs[3] = 3;

        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                int globalX = cx * 16 + x;
                int globalZ = cz * 16 + z;
                int surfaceHeight = 24 + static_cast<int>(std::sin(globalX * 0.02f) * 10.0f + std::cos(globalZ * 0.02f) * 10.0f);

                for (int y = 0; y < 16; ++y) {
                    int globalY = cy * 16 + y;
                    uint8_t blockID = 0;

                    if (globalY < surfaceHeight - 3) blockID = 3;
                    else if (globalY < surfaceHeight) blockID = 1;
                    else if (globalY == surfaceHeight) blockID = 2;

                    if (blockID != 0) {
                        uint32_t localIndex = x + (y * 16) + (z * 256);
                        chunk.tickNow[localIndex] = blockID;
                        chunk.tickAfter[localIndex] = blockID;
                    }
                }
            }
        }
    }
}