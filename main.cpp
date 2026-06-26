#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb_image.h" // Vagy ahogy pontosan nálad van
#undef STB_IMAGE_IMPLEMENTATION // <--- EZ AZ A SOR AMI MEGOLDJA A REDEFINITION HIBÁKAT!

#include <cstdint>
#include <vector>
#include <iostream>
#include <thread>
#include <cmath>

#include "Core/MacroChunk.hpp"
#include "Core/World.hpp"
#include "Render/VulkanCore.hpp"

// Globális mutató a VulkanCore-ra a remesheléshez
NF::Render::VulkanCore* globalVulkanApp = nullptr;

void BreakBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz) {
    int cx = gx >> 4; int cy = gy >> 4; int cz = gz >> 4;
    uint64_t hash = NF::Core::GetChunkHash(cx, cy, cz);

    auto it = NF::Core::realWorld.find(hash);
    if (it == NF::Core::realWorld.end()) return;

    int lx = gx & 15; int ly = gy & 15; int lz = gz & 15;
    uint32_t localIndex = lx + (ly * 16) + (lz * 256);

    it->second->tickNow[localIndex] = 0;
    it->second->tickAfter[localIndex] = 0;

    globalVulkanApp->UpdateSingleChunk(NF::Core::GetChunkHash(cx, cy, cz), cx, cy, cz);


    if (lx == 0)  globalVulkanApp->UpdateSingleChunk(NF::Core::GetChunkHash(cx-1, cy, cz), cx-1, cy, cz);
    if (lx == 15) globalVulkanApp->UpdateSingleChunk(NF::Core::GetChunkHash(cx+1, cy, cz), cx+1, cy, cz);
    if (ly == 0)  globalVulkanApp->UpdateSingleChunk(NF::Core::GetChunkHash(cx, cy-1, cz), cx, cy-1, cz);
    if (ly == 15) globalVulkanApp->UpdateSingleChunk(NF::Core::GetChunkHash(cx, cy+1, cz), cx, cy+1, cz);
    if (lz == 0) globalVulkanApp->UpdateSingleChunk(NF::Core::GetChunkHash(cx, cy, cz-1), cx, cy, cz-1);
    if (lz == 15) globalVulkanApp->UpdateSingleChunk(NF::Core::GetChunkHash(cx, cy, cz+1), cx, cy, cz+1);

    std::cout << "[Fizika] Blokk Kitorve! Koordinata: X:" << gx << " Y:" << gy << " Z:" << gz << "\n";
    if (globalVulkanApp) globalVulkanApp->UpdateSingleChunk(hash, cx, cy, cz);
}

void PlaceBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz, uint8_t blockID) {
    int cx = gx >> 4; int cy = gy >> 4; int cz = gz >> 4;
    uint64_t hash = NF::Core::GetChunkHash(cx, cy, cz);

    auto it = NF::Core::realWorld.find(hash);
    if (it == NF::Core::realWorld.end()) return;

    int lx = gx & 15; int ly = gy & 15; int lz = gz & 15;
    uint32_t localIndex = lx + (ly * 16) + (lz * 256);

    it->second->tickNow[localIndex] = blockID;
    it->second->tickAfter[localIndex] = blockID;

    std::cout << "[Fizika] Blokk Lerakva! ID: " << (int)blockID << " | Koordinata: X:" << gx << " Y:" << gy << " Z:" << gz << "\n";
    if (globalVulkanApp) globalVulkanApp->UpdateSingleChunk(hash, cx, cy, cz);
}

int main() {
    std::cout << "====================================================\n";
    std::cout << " NEXUSFORGE TITAN ENGINE - INFINITE WORLD\n";
    std::cout << "====================================================\n";

    NF::Render::VulkanCore app;
    globalVulkanApp = &app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "CRASH: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}