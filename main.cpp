//
// Fájl: main.cpp
// Készítette: NexusForge Engine (Rick & Gem)
//
#include <cstdint>
#include <vector>
#include <iostream>
#include <array>
#include <atomic>
#include <utility>
#include <chrono>
#include <random>
#include <iomanip>
#include <bit>
#include <thread>
#include <cmath>

#include "Core/MacroChunk.hpp"
#include "Core/BlockPlacement.hpp"
#include "Core/MutationPipeline.hpp"
#include "Render/BasicMesher.hpp"
#include "Render/VulkanCore.hpp"

using namespace NF::Core;

constexpr uint32_t FLAG_REJECTED        = 1U << 31;
constexpr uint32_t FLAG_OVERWRITE_SOLID = 1U << 30;
constexpr uint8_t  AIR_PALETTE_INDEX_8  = 0;

struct alignas(64) LightMap { uint16_t data[4096]; };

class GlobalLightPool {
public:
    static inline std::vector<LightMap> pool;
    static void Initialize() {
        pool.resize(10000);
        for (size_t i = 0; i < 4096; ++i) pool[1].data[i] = 0xF000;
    }
};

std::vector<MacroChunk_Small> realWorld(200);

// Globális mutató a VulkanCore-ra a remesheléshez
NF::Render::VulkanCore* globalVulkanApp = nullptr;

// --- BLOKK KIÜTÉS ---
void BreakBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz) {
    int cx = gx >> 4; int cy = gy >> 4; int cz = gz >> 4;
    if (cx < 0 || cx >= 8 || cy < 0 || cy >= 2 || cz < 0 || cz >= 8) return;

    uint32_t chunkIndex = cx + (cz * 8) + (cy * 64);
    int lx = gx & 15; int ly = gy & 15; int lz = gz & 15;
    uint32_t localIndex = lx + (ly * 16) + (lz * 256);

    realWorld[chunkIndex].tickNow[localIndex] = AIR_PALETTE_INDEX_8;
    realWorld[chunkIndex].tickAfter[localIndex] = AIR_PALETTE_INDEX_8;

    std::cout << "[Fizika] Blokk Kitorve! Koordinata: X:" << gx << " Y:" << gy << " Z:" << gz << "\n";
    // MDI varázslat: Csak ezt a chunkot küldjük át a Mega-Bufferbe!
    if (globalVulkanApp) globalVulkanApp->UpdateSingleChunk(chunkIndex, cx, cy, cz);
}

void PlaceBlockAndRemesh(int32_t gx, int32_t gy, int32_t gz, uint8_t blockID) {
    int cx = gx >> 4; int cy = gy >> 4; int cz = gz >> 4;
    if (cx < 0 || cx >= 8 || cy < 0 || cy >= 2 || cz < 0 || cz >= 8) return;

    uint32_t chunkIndex = cx + (cz * 8) + (cy * 64);
    int lx = gx & 15; int ly = gy & 15; int lz = gz & 15;
    uint32_t localIndex = lx + (ly * 16) + (lz * 256);

    realWorld[chunkIndex].tickNow[localIndex] = blockID;
    realWorld[chunkIndex].tickAfter[localIndex] = blockID;

    std::cout << "[Fizika] Blokk Lerakva! ID: " << (int)blockID << " | Koordinata: X:" << gx << " Y:" << gy << " Z:" << gz << "\n";
    // Csak 1 chunk újrameshelése
    if (globalVulkanApp) globalVulkanApp->UpdateSingleChunk(chunkIndex, cx, cy, cz);
}

class O1_PhysicsArbiter {
public:
    static void ResolveConflictsParallel(std::vector<MoveCommand_Block>& sorted_cmds, size_t count, int num_threads) {}
};

class PhysicsEngine {
public:
    static void CommitTargetsParallel(std::vector<MoveCommand_Block>& commands, size_t count, int num_threads) {
        if (count == 0) return;
        std::vector<std::thread> threads;
        size_t chunk_size = count / num_threads;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&commands, count, chunk_size, num_threads, t]() {
                size_t start = t * chunk_size;
                size_t end = (t == num_threads - 1) ? count : (t + 1) * chunk_size;
                for (size_t i = start; i < end; ++i) {
                    auto& cmd = commands[i];
                    if ((cmd.flags & FLAG_REJECTED) == 0 && cmd.targetGridID > 0) {
                        uint32_t chunkIndex = cmd.tgtChunkX + (cmd.tgtChunkZ * 8) + (cmd.tgtChunkY * 64);
                        auto& tgtChunk = realWorld[chunkIndex];
                        bool isAir = (tgtChunk.tickAfter[cmd.tgtLocalIndex] == AIR_PALETTE_INDEX_8);
                        bool canOverwrite = (cmd.flags & FLAG_OVERWRITE_SOLID);
                        if (isAir || canOverwrite) {
                            SetBlockInChunk_Small(tgtChunk, cmd.tgtLocalIndex, cmd.targetPaletteIndex);
                        }
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
    }
};

int main() {
    std::cout << "====================================================\n";
    std::cout << " NEXUSFORGE TITAN ENGINE LAUNCHER (8x8x2 WORLD)\n";
    std::cout << "====================================================\n";

    GlobalLightPool::Initialize();
    MutationPipeline engine;
    uint32_t active_threads = std::min<uint32_t>(8, std::max<uint32_t>(2, std::thread::hardware_concurrency() / 2));

    std::vector<MoveCommand_Block> blk_src, blk_dst;
    std::vector<SortItem> items, aux;

    blk_src.reserve(1000000);
    items.reserve(1000000);

    uint64_t tick_global_or = 0;
    uint64_t tick_global_and = ~0ULL;

    std::cout << "[PHASE 1] 128 Chunk generalasa... ";

    constexpr uint32_t currentNexusRegionID = 1;

    for (int cx = 0; cx < 8; ++cx) {
        for (int cz = 0; cz < 8; ++cz) {
            for (int cy = 0; cy < 2; ++cy) {

                uint32_t chunkIndex = cx + (cz * 8) + (cy * 64);

                for (int z = 0; z < 16; ++z) {
                    for (int x = 0; x < 16; ++x) {
                        int globalX = cx * 16 + x;
                        int globalZ = cz * 16 + z;

                        int height = 8 + static_cast<int>(std::sin(globalX * 0.1f) * 4.0f + std::cos(globalZ * 0.1f) * 4.0f);

                        for (int y = 0; y < 16; ++y) {
                            uint32_t blockID = 0;

                            if (cy == 0) {
                                blockID = 3;
                            } else {
                                if (y == height) blockID = 2;
                                else if (y < height && y >= height - 3) blockID = 1;
                                else if (y < height - 3) blockID = 3;
                            }

                            if (blockID != 0) {
                                MoveCommand_Block cmd{};
                                cmd.targetGridID = currentNexusRegionID;
                                cmd.tgtChunkX = cx;
                                cmd.tgtChunkY = cy;
                                cmd.tgtChunkZ = cz;
                                cmd.tgtLocalIndex = x + (y * 16) + (z * 256);
                                cmd.targetPaletteIndex = blockID;
                                cmd.force = 10;
                                cmd.flags = FLAG_OVERWRITE_SOLID;

                                blk_src.push_back(cmd);
                                SortItem si; si.key = ExtractSortKey(cmd); si.index = static_cast<uint32_t>(blk_src.size() - 1);
                                tick_global_or |= si.key; tick_global_and &= si.key; items.push_back(si);
                            }
                        }
                    }
                }
            }
        }
    }

    size_t tick_blocks = blk_src.size();
    std::cout << "Kesz. (" << tick_blocks << " aktiv blokk a memoriaban!)\n";

    blk_dst.resize(tick_blocks);
    aux.resize(tick_blocks);

    uint64_t varying_bits = tick_global_or ^ tick_global_and;

    engine.ExecutePhase(items, aux, blk_src, blk_dst, tick_blocks, varying_bits);
    O1_PhysicsArbiter::ResolveConflictsParallel(blk_dst, tick_blocks, active_threads);
    PhysicsEngine::CommitTargetsParallel(blk_dst, tick_blocks, active_threads);

    NF::Render::VulkanCore app;
    globalVulkanApp = &app; // Rákötjük a globális mutatót

    std::cout << "====================================================\n";
    std::cout << " GPU RENDER INDITASA\n";
    std::cout << "====================================================\n";

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "CRASH: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}