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

// Beemeljük a saját Core headereidet
#include "Core/MacroChunk.hpp"
#include "Core/BlockPlacement.hpp"

// AZ ÚJ TITAN PIPELINE (Ezt a fájlt adtam az előző üzenetben!)
#include "Core/MutationPipeline.hpp"

using namespace NF::Core;

// --- KONSTANSOK ---
constexpr uint32_t FLAG_REJECTED        = 1U << 31;
constexpr uint32_t FLAG_OVERWRITE_SOLID = 1U << 30;

constexpr uint8_t  AIR_PALETTE_INDEX_8  = 0;
constexpr uint16_t AIR_PALETTE_INDEX_16 = 0;
constexpr size_t MAX_SUPPORTED_THREADS  = 16;

// --- A GLOBÁLIS FÉNY-MEDENCE (LIGHT POOL) ---
struct alignas(64) LightMap {
    uint16_t data[4096];
};

class GlobalLightPool {
private:
    static inline std::atomic<uint32_t> nextFreeIndex{2};
public:
    static inline std::vector<LightMap> pool;

    static void Initialize() {
        pool.resize(10000);
        // 0. INDEX: "DUMMY" KOROMSÖTÉT (Minden nulla)
        // 1. INDEX: "DUMMY" NAPFÉNY
        for (size_t i = 0; i < 4096; ++i) {
            pool[1].data[i] = 0xF000; 
        }
    }

    static uint32_t AllocateNewLightMap() {
        uint32_t index = nextFreeIndex.fetch_add(1, std::memory_order_relaxed);
        return (index < pool.size()) ? index : 0;
    }
};

// --- CHUNK TIER ÉS MEMORY MANAGER MOCK ---
enum class ChunkTier : uint8_t { UNLOADED = 0, WILDERNESS_8BIT, NORMAL_16BIT, HEAVY_16BIT };

struct ChunkBufferInfo {
    void* bufferTickAfter; 
    ChunkTier tier;
};

static uint8_t  TRASH_BIN_8BIT[MAX_SUPPORTED_THREADS]  = {0};
static uint16_t TRASH_BIN_16BIT[MAX_SUPPORTED_THREADS] = {0};

class MemoryManager {
public:
    static ChunkBufferInfo GetChunkTickAfterBuffer(uint32_t gridID, int16_t cx, int16_t cy, int16_t cz) {
        // Valós implementáció jön ide
        if (cx == 0) return { nullptr, ChunkTier::UNLOADED };
        if (cx == 1) return { nullptr, ChunkTier::WILDERNESS_8BIT };
        return { nullptr, ChunkTier::NORMAL_16BIT };
    }
};

// --- ÚJ: O(1) DETERMINISZTIKUS KÖLCSÖNÖS MEGSEMMISÜLÉS ARBITER ---
class O1_PhysicsArbiter {
public:
    // MEGJEGYZÉS: Itt a MoveCommand_Block-ot használjuk a Titan Pipeline-ból!
    static void ResolveConflicts(std::vector<MoveCommand_Block>& sorted_cmds, size_t count) {
        if (count < 2) return;
        
        for (size_t i = 0; i < count - 1; ++i) {
            auto& a = sorted_cmds[i];
            auto& b = sorted_cmds[i + 1];

            if (a.flags & FLAG_REJECTED) continue; 

            if (a.targetGridID == b.targetGridID && a.tgtLocalIndex == b.tgtLocalIndex) {
                if (a.force > b.force) {
                    b.flags |= FLAG_REJECTED;
                } else if (a.force < b.force) {
                    a.flags |= FLAG_REJECTED;
                } else {
                    // DÖNTETLEN ESETÉN: Mindkét blokk/jármű megsemmisül! Kőkemény fizika!
                    a.flags |= FLAG_REJECTED;
                    b.flags |= FLAG_REJECTED;
                }
            }
        }
    }
};

// --- COMMIT FÁZIS (Rögzítés a memóriába) ---
class PhysicsEngine {
public:
    static void CommitSources(std::vector<MoveCommand_Block>& commands, size_t count, int threadID) {
        uint8_t* const TRASH_PTR_8  = &TRASH_BIN_8BIT[threadID % MAX_SUPPORTED_THREADS];
        uint16_t* const TRASH_PTR_16 = &TRASH_BIN_16BIT[threadID % MAX_SUPPORTED_THREADS];

        for (size_t i = 0; i < count; ++i) {
            auto& cmd = commands[i];
            ChunkBufferInfo srcInfo = MemoryManager::GetChunkTickAfterBuffer(cmd.sourceGridID, cmd.srcChunkX, cmd.srcChunkY, cmd.srcChunkZ);

            uint32_t isLoaded = (srcInfo.tier != ChunkTier::UNLOADED);
            uint32_t isAccepted = ((cmd.flags & FLAG_REJECTED) == 0);
            uint32_t execute = isAccepted & isLoaded;

            if (srcInfo.tier == ChunkTier::WILDERNESS_8BIT) {
                uint8_t* rawBuf = static_cast<uint8_t*>(srcInfo.bufferTickAfter);
                uint32_t safeIndex = isLoaded ? cmd.srcLocalIndex : 0;
                uint8_t* safeBuf = isLoaded ? rawBuf : TRASH_PTR_8;
                uint8_t* finalSourcePtr = execute ? &safeBuf[safeIndex] : TRASH_PTR_8;
                *finalSourcePtr = AIR_PALETTE_INDEX_8;
            }
            else if (isLoaded) {
                uint16_t* rawBuf = static_cast<uint16_t*>(srcInfo.bufferTickAfter);
                uint32_t safeIndex = isLoaded ? cmd.srcLocalIndex : 0;
                uint16_t* safeBuf = isLoaded ? rawBuf : TRASH_PTR_16;
                uint16_t* finalSourcePtr = execute ? &safeBuf[safeIndex] : TRASH_PTR_16;
                *finalSourcePtr = AIR_PALETTE_INDEX_16;
            }
        }
    }

    static void CommitTargets(std::vector<MoveCommand_Block>& commands, size_t count, int threadID) {
        uint8_t* const TRASH_PTR_8  = &TRASH_BIN_8BIT[threadID % MAX_SUPPORTED_THREADS];
        uint16_t* const TRASH_PTR_16 = &TRASH_BIN_16BIT[threadID % MAX_SUPPORTED_THREADS];

        for (size_t i = 0; i < count; ++i) {
            auto& cmd = commands[i];
            ChunkBufferInfo tgtInfo = MemoryManager::GetChunkTickAfterBuffer(cmd.targetGridID, cmd.tgtChunkX, cmd.tgtChunkY, cmd.tgtChunkZ);

            uint32_t isLoaded = (tgtInfo.tier != ChunkTier::UNLOADED);
            uint32_t isAccepted = ((cmd.flags & FLAG_REJECTED) == 0);
            uint32_t canOverwrite = ((cmd.flags & FLAG_OVERWRITE_SOLID) != 0);

            if (tgtInfo.tier == ChunkTier::WILDERNESS_8BIT) {
                uint8_t* rawBuf = static_cast<uint8_t*>(tgtInfo.bufferTickAfter);
                uint32_t safeIndex = isLoaded ? cmd.tgtLocalIndex : 0;
                uint8_t* safeBuf = isLoaded ? rawBuf : TRASH_PTR_8;

                uint32_t isAir = (safeBuf[safeIndex] == AIR_PALETTE_INDEX_8);
                uint32_t execute = isAccepted & isLoaded & (isAir | canOverwrite);

                uint8_t* finalTargetPtr = execute ? &safeBuf[safeIndex] : TRASH_PTR_8;
                *finalTargetPtr = static_cast<uint8_t>(cmd.targetPaletteIndex);
            }
            else if (isLoaded) {
                uint16_t* rawBuf = static_cast<uint16_t*>(tgtInfo.bufferTickAfter);
                uint32_t safeIndex = isLoaded ? cmd.tgtLocalIndex : 0;
                uint16_t* safeBuf = isLoaded ? rawBuf : TRASH_PTR_16;

                uint32_t isAir = (safeBuf[safeIndex] == AIR_PALETTE_INDEX_16);
                uint32_t execute = isAccepted & isLoaded & (isAir | canOverwrite);

                uint16_t* finalTargetPtr = execute ? &safeBuf[safeIndex] : TRASH_PTR_16;
                *finalTargetPtr = static_cast<uint16_t>(cmd.targetPaletteIndex);
            }
        }
    }
};

// --- FÉNY MOTOR ---
class LightEngine {
public:
    template <typename IndexType, size_t MaxPaletteSize>
    static void ProcessLightJob(MacroChunk<IndexType, MaxPaletteSize>* chunk) {
        if (chunk->lightMapIndex <= 1) {
            chunk->lightMapIndex = GlobalLightPool::AllocateNewLightMap();
        }
        if (chunk->lightMapIndex == 0) return;
        uint16_t* myLightData = GlobalLightPool::pool[chunk->lightMapIndex].data;
        // Flood fill ...
        chunk->requiresLightUpdate = 0;
    }
};

// ========================================================================
// A NEXUSFORGE "START" GOMB (A fő játékhurok)
// ========================================================================
int main() {
    std::cout << "====================================================\n";
    std::cout << " NEXUSFORGE TITAN ENGINE - INITIALIZATION\n";
    std::cout << "====================================================\n";

    GlobalLightPool::Initialize();
    
    // 1. A Titan háttérszálak felébrednek
    MutationPipeline engine;
    
    // 2. Globális Tick Pufferek (Kőkemény memória előfoglalás)
    constexpr size_t MAX_TICK_EVENTS = 250000; 
    
    std::vector<MoveCommand_Deletion> del_src, del_dst;
    std::vector<MoveCommand_Liquid> liq_src, liq_dst;
    std::vector<MoveCommand_Block> blk_src, blk_dst;
    std::vector<SortItem> items, aux;
    
    del_src.reserve(MAX_TICK_EVENTS); del_dst.resize(MAX_TICK_EVENTS);
    liq_src.reserve(MAX_TICK_EVENTS); liq_dst.resize(MAX_TICK_EVENTS);
    blk_src.reserve(MAX_TICK_EVENTS); blk_dst.resize(MAX_TICK_EVENTS);
    items.resize(MAX_TICK_EVENTS);    aux.resize(MAX_TICK_EVENTS);

    std::cout << "[SYSTEM] LightPool és Memoria Pufferek lefoglalva.\n";
    std::cout << "[SYSTEM] O(1) Mutual Destruction Arbiter aktiv.\n\n";

    // --- TE TESZTED ---
    /*
    MacroChunk_Small asd;
    bool siker = SetBlockInChunk_Small(asd, 100, 100);
    SetBlockInChunk_Small(asd, 101, 101);
    SetBlockInChunk_Small(asd, 102, 102);
    asd.SwapTickBuffers();
    std::cout << "Teszt kimenet: " << (int)siker << " | " << asd.activePaletteSize << std::endl;
    */

    // --- A GAME LOOP ---
    bool isServerRunning = true;
    uint64_t tickCounter = 0;

    while (isServerRunning) {
        
        // PHASE 1: EVENTEK (Hálózat, SubGrid adatok, Játékos építés)
        size_t tick_deletions = del_src.size();
        size_t tick_liquids   = liq_src.size();
        size_t tick_blocks    = blk_src.size();

        // PHASE 2 & 3: STRUKTURÁLIS MUTÁCIÓ & ARBITER
        if (tick_deletions > 0) {
            engine.ExecutePhase(items, aux, del_src, del_dst, tick_deletions);
        }
        
        if (tick_blocks > 0) {
            // Szortírozzuk és Memóriába Gatherezzük a 64-bájtos adatokat
            engine.ExecutePhase(items, aux, blk_src, blk_dst, tick_blocks);
            
            // O(1) Döntéshozatal: A szortírozott listán azonnal kiderül, ki ütközik
            O1_PhysicsArbiter::ResolveConflicts(blk_dst, tick_blocks);
            
            // PHASE 4: COMMIT (Beégetjük az adatot a Chunkok bufferébe)
            PhysicsEngine::CommitSources(blk_dst, tick_blocks, 0); // Főszálon hívjuk (0. szál id)
            PhysicsEngine::CommitTargets(blk_dst, tick_blocks, 0);
        }

        if (tick_liquids > 0) {
            engine.ExecutePhase(items, aux, liq_src, liq_dst, tick_liquids);
        }

        // PHASE 5: WIDE EXECUTION & RENDER PREP
        // Itt pörög a LightEngine és az Entity AI a lock-free világon!

        tickCounter++;
        // Egyetlen teszt-tick után kilép
        if (tickCounter >= 1) isServerRunning = false; 
    }

    std::cout << "Engine Shutdown tiszta kilepessel.\n";
    return 0;
}
