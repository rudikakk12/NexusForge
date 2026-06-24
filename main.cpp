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

#include "Core/MacroChunk.hpp"
#include "Core/BlockPlacement.hpp"
#include "Core/MutationPipeline.hpp"
#include "Render/BasicMesher.hpp"
#include "Render/VulkanCore.hpp" // ÚJ!

using namespace NF::Core;

constexpr uint32_t FLAG_REJECTED        = 1U << 31;
constexpr uint32_t FLAG_OVERWRITE_SOLID = 1U << 30;
constexpr uint8_t  AIR_PALETTE_INDEX_8  = 0;
constexpr size_t MAX_SUPPORTED_THREADS  = 16;

struct alignas(64) LightMap { uint16_t data[4096]; };

class GlobalLightPool {
private:
    static inline std::atomic<uint32_t> nextFreeIndex{2};
public:
    static inline std::vector<LightMap> pool;
    static void Initialize() {
        pool.resize(10000);
        for (size_t i = 0; i < 4096; ++i) pool[1].data[i] = 0xF000;
    }
};

static std::vector<MacroChunk_Small> realWorld(101);

// --- PÁRHUZAMOS O(1) ARBITER (X, Y, Z Térbeli Védelmmel!) ---
class O1_PhysicsArbiter {
public:
    static void ResolveConflictsParallel(std::vector<MoveCommand_Block>& sorted_cmds, size_t count, int num_threads) {
        if (count < 2) return;
        std::vector<std::thread> threads;
        size_t chunk_size = count / num_threads;

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&sorted_cmds, count, chunk_size, num_threads, t]() {
                size_t start = t * chunk_size;
                size_t end = (t == num_threads - 1) ? count : (t + 1) * chunk_size;

                // Szegmens határvédelem (Soha nem vágunk szét egyező X,Y,Z blokkokat)
                if (start > 0) {
                    while (start < count &&
                           sorted_cmds[start].targetGridID == sorted_cmds[start - 1].targetGridID &&
                           sorted_cmds[start].tgtChunkX == sorted_cmds[start - 1].tgtChunkX &&
                           sorted_cmds[start].tgtChunkY == sorted_cmds[start - 1].tgtChunkY &&
                           sorted_cmds[start].tgtChunkZ == sorted_cmds[start - 1].tgtChunkZ &&
                           sorted_cmds[start].tgtLocalIndex == sorted_cmds[start - 1].tgtLocalIndex) {
                        start++;
                    }
                }
                if (t != num_threads - 1) {
                    while (end < count &&
                           sorted_cmds[end].targetGridID == sorted_cmds[end - 1].targetGridID &&
                           sorted_cmds[end].tgtChunkX == sorted_cmds[end - 1].tgtChunkX &&
                           sorted_cmds[end].tgtChunkY == sorted_cmds[end - 1].tgtChunkY &&
                           sorted_cmds[end].tgtChunkZ == sorted_cmds[end - 1].tgtChunkZ &&
                           sorted_cmds[end].tgtLocalIndex == sorted_cmds[end - 1].tgtLocalIndex) {
                        end++;
                    }
                }

                if (start >= end || start >= count) return;

                size_t current_winner = start;
                for (size_t i = start + 1; i < end; ++i) {
                    auto& king = sorted_cmds[current_winner];
                    auto& challenger = sorted_cmds[i];

                    // TÖKÉLETES TÉRBELI EGYEZÉS
                    if (king.targetGridID == challenger.targetGridID &&
                        king.tgtChunkX == challenger.tgtChunkX && king.tgtChunkY == challenger.tgtChunkY && king.tgtChunkZ == challenger.tgtChunkZ &&
                        king.tgtLocalIndex == challenger.tgtLocalIndex) {
                        if (king.flags & FLAG_REJECTED) {
                            current_winner = i; continue;
                        }
                        if (king.force > challenger.force) challenger.flags |= FLAG_REJECTED;
                        else if (challenger.force > king.force) { king.flags |= FLAG_REJECTED; current_winner = i; }
                        else { king.flags |= FLAG_REJECTED; challenger.flags |= FLAG_REJECTED; }
                    } else {
                        current_winner = i;
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
    }
};

// --- PÁRHUZAMOS AVX2 COMMIT ---
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

                // CHUNK (Grid+XYZ) Alapú szegmentálás a Lock-Free íráshoz!
                if (start > 0) {
                    while (start < count &&
                           commands[start].targetGridID == commands[start - 1].targetGridID &&
                           commands[start].tgtChunkX == commands[start - 1].tgtChunkX &&
                           commands[start].tgtChunkY == commands[start - 1].tgtChunkY &&
                           commands[start].tgtChunkZ == commands[start - 1].tgtChunkZ) {
                        start++;
                    }
                }
                if (t != num_threads - 1) {
                    while (end < count &&
                           commands[end].targetGridID == commands[end - 1].targetGridID &&
                           commands[end].tgtChunkX == commands[end - 1].tgtChunkX &&
                           commands[end].tgtChunkY == commands[end - 1].tgtChunkY &&
                           commands[end].tgtChunkZ == commands[end - 1].tgtChunkZ) {
                        end++;
                    }
                }

                if (start >= end || start >= count) return;

                for (size_t i = start; i < end; ++i) {
                    auto& cmd = commands[i];
                    if ((cmd.flags & FLAG_REJECTED) == 0 && cmd.targetGridID > 0 && cmd.targetGridID <= 100) {
                        auto& tgtChunk = realWorld[cmd.targetGridID]; // Teszt miatt egyenlőre a GridID-ba írunk
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

/*int main() {
    std::cout << "====================================================\n";
    std::cout << " NEXUSFORGE VARYING-BITS RADIX PIPELINE (500K)\n";
    std::cout << "====================================================\n";

    GlobalLightPool::Initialize();
    MutationPipeline engine;

    constexpr size_t TEST_SIZE = 500000;
    uint32_t active_threads = std::min<uint32_t>(8, std::max<uint32_t>(2, std::thread::hardware_concurrency() / 2));

    std::vector<MoveCommand_Block> blk_src, blk_dst;
    std::vector<SortItem> items, aux;
    blk_src.reserve(TEST_SIZE); blk_dst.resize(TEST_SIZE);
    items.reserve(TEST_SIZE);   aux.resize(TEST_SIZE);

    std::mt19937 gen(1337);
    std::uniform_int_distribution<uint32_t> distGrid(1, 100);
    std::uniform_int_distribution<int16_t> distChunk(0, 15);     // Egy pici régióban építkezünk
    std::uniform_int_distribution<uint16_t> distLocal(0, 4095);
    std::uniform_int_distribution<uint32_t> distForce(1, 50);
    std::uniform_int_distribution<uint32_t> distGlobalBlockID(1, 200);

    // AZ ÉVSZÁZAD TRÜKKJE: Kigyűjtjük a biteket!
    uint64_t tick_global_or = 0;
    uint64_t tick_global_and = ~0ULL;

    std::cout << "[PHASE 1] 500,000 block esemeny generalasa... ";
    for(size_t i = 0; i < TEST_SIZE; ++i) {
        MoveCommand_Block cmd{};
        cmd.targetGridID = distGrid(gen);
        cmd.tgtChunkX = distChunk(gen);
        cmd.tgtChunkY = distChunk(gen);
        cmd.tgtChunkZ = distChunk(gen);
        cmd.tgtLocalIndex = distLocal(gen);
        cmd.targetPaletteIndex = distGlobalBlockID(gen);
        cmd.force = distForce(gen);
        cmd.flags = FLAG_OVERWRITE_SOLID;
        blk_src.push_back(cmd);

        SortItem si; si.key = ExtractSortKey(cmd); si.index = i;

        tick_global_or |= si.key;
        tick_global_and &= si.key;

        items.push_back(si);
    }
    std::cout << "Kesz.\n\n";

    uint64_t varying_bits = tick_global_or ^ tick_global_and;
    size_t tick_blocks = blk_src.size();

    // --- PHASE 2: TITAN PIPELINE ---
    auto t_titan_start = std::chrono::high_resolution_clock::now();
    engine.ExecutePhase(items, aux, blk_src, blk_dst, tick_blocks, varying_bits);
    auto t_titan_end = std::chrono::high_resolution_clock::now();

    // --- PHASE 3: PÁRHUZAMOS O(1) ARBITER ---
    auto t_arbiter_start = std::chrono::high_resolution_clock::now();
    O1_PhysicsArbiter::ResolveConflictsParallel(blk_dst, tick_blocks, active_threads);
    auto t_arbiter_end = std::chrono::high_resolution_clock::now();

    // --- PHASE 4: PÁRHUZAMOS AVX2 COMMIT ---
    auto t_commit_start = std::chrono::high_resolution_clock::now();
    PhysicsEngine::CommitTargetsParallel(blk_dst, tick_blocks, active_threads);
    auto t_commit_end = std::chrono::high_resolution_clock::now();

    double titan_ms = std::chrono::duration<double, std::milli>(t_titan_end - t_titan_start).count();
    double arbiter_ms = std::chrono::duration<double, std::milli>(t_arbiter_end - t_arbiter_start).count();
    double commit_ms = std::chrono::duration<double, std::milli>(t_commit_end - t_commit_start).count();
    double total_ms = titan_ms + arbiter_ms + commit_ms;

    std::cout << "====================================================\n";
    std::cout << " TELJESITMENY JELENTES (500K Blokk / 1 Tick)\n";
    std::cout << "====================================================\n";
    std::cout << std::fixed << std::setprecision(4);
    std::cout << " [Phase 2] Titan Rendezés+Gather : " << std::setw(8) << titan_ms << " ms\n";
    std::cout << " [Phase 3] Multi-Core Arbiter    : " << std::setw(8) << arbiter_ms << " ms\n";
    std::cout << " [Phase 4] Multi-Core AVX Commit : " << std::setw(8) << commit_ms << " ms\n";
    std::cout << "----------------------------------------------------\n";
    std::cout << " OSSZESITETT TICK IDO            : " << std::setw(8) << total_ms << " ms  (BUDGET: 31.25 ms)\n\n";
    std::cout << "====================================================\n";

    // ====================================================
    // MESHING TESZT (Phase 5 Szimuláció)
    // ====================================================
    auto t_mesh_start = std::chrono::high_resolution_clock::now();

    // Generáljuk le az 1-es GridID-jú MacroChunk mesh-ét
    NF::Render::MeshData chunk1_mesh = NF::Render::BasicMesher::GenerateMesh(realWorld[1]);
    //NF::Render::BasicMesher::ExportToOBJ(chunk1_mesh, "NexusForge_Chunk_1.obj");
    auto t_mesh_end = std::chrono::high_resolution_clock::now();
    double meshing_ms = std::chrono::duration<double, std::milli>(t_mesh_end - t_mesh_start).count();

    std::cout << " MESHING TESZT (GridID 1):\n";
    std::cout << " -> Legeneralt Vertexek szama : " << chunk1_mesh.vertices.size() << " db\n";
    std::cout << " -> Legeneralt Haromszogek    : " << (chunk1_mesh.indices.size() / 3) << " db\n";
    std::cout << " -> MESHING IDO               : " << std::fixed << std::setprecision(4) << meshing_ms << " ms\n";
    std::cout << "====================================================\n";

    return 0;
}*/
int main() {
    std::cout << "====================================================\n";
    std::cout << " NEXUSFORGE VULKAN LAUNCHER\n";
    std::cout << "====================================================\n";

    // 1. Lefoglaljuk a memóriákat és a Fény-medencét, mint eddig
    GlobalLightPool::Initialize();

    // 2. Létrehozzuk a Vulkan Appot
    NF::Render::VulkanCore app;

    try {
        // Ez megnyitja az ablakot, és pörgeti a while() ciklust,
        // amíg be nem zárod az X-szel!
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}