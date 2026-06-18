#include <cstdint>
#include <vector>
#include <iostream>
#include <array>
#include <atomic>
#include <utility> // ÚJ: std::swap miatt szükséges
#include "Core/BlockPlacement.hpp"
#include "Core/MacroChunk.hpp"

// --- KONSTANSOK ---
constexpr uint32_t FLAG_REJECTED                        = 1U << 31;
constexpr uint32_t FLAG_OVERWRITE_SOLID                 = 1U << 30;
// Paletta indexek (Szigorú szabály: a 0-s index MINDIG a levegő a chunk palettájában)
// Ez garantálja, hogy a C++ zero-initialization (vagy egy memset(0)) azonnal levegővel tölti fel a teret.
constexpr uint8_t  AIR_PALETTE_INDEX_8                  = 0;
constexpr uint16_t AIR_PALETTE_INDEX_16                 = 0;

// Maximum támogatott szálak száma a Trash Bin bufferhez
constexpr size_t MAX_SUPPORTED_THREADS                  = 16;

// --- ÚJ: A GLOBÁLIS FÉNY-MEDENCE (LIGHT POOL) ---
// Ez spórolja meg a 6.5 GB RAM-ot. Csak az aktív fények kapnak 64KB-ot.
struct alignas(64) LightMap {
    uint16_t data[4096];
};

class GlobalLightPool {
private:
    // Lock-free foglaláshoz atomi számláló.
    static inline std::atomic<uint32_t> nextFreeIndex{2};

public:
    // Előre lefoglalt memória a töredezettség elkerülésére
    static inline std::vector<LightMap> pool;

    static void Initialize() {
        // Lefoglalunk helyet mondjuk 10,000 aktív fényes chunk-nak (640 MB RAM)
        pool.resize(10000);

        // 0. INDEX: "DUMMY" KOROMSÖTÉT
        // Mivel zero-initialized a vector, ez alapból csupa 0. Senki nem írhat bele.

        // 1. INDEX: "DUMMY" NAPFÉNY
        // A felszíni, üres chunkok fognak ide mutatni (branchless olvasás a renderelőnek).
        for (size_t i = 0; i < 4096; ++i) {
            pool[1].data[i] = 0xF000; // 4 bit Sun (Max), RGB (0)
        }
    }

    // Villámgyors aszinkron foglalás a fény-motornak
    static uint32_t AllocateNewLightMap() {
        // std::memory_order_relaxed elég, mert csak egy ID-t kérünk ki.
        // Később ide jöhet egy Free-List a törölt chunkok újrahasznosítására.
        uint32_t index = nextFreeIndex.fetch_add(1, std::memory_order_relaxed);
        // Biztonsági öv - ha kifutunk a memóriából, inkább sötét marad, de nem fagy ki.
        return (index < pool.size()) ? index : 0;
    }
};

// --- 1. ADATSTRUKTÚRÁK ---
#pragma pack(push, 4) // Kényszerítjük, hogy ne legyenek "lukak" az adatok között
struct alignas(64) GlobalMoveCommand {
    // 12 bájt: Forrás helye
    uint16_t srcLocalIndex;
    int16_t srcChunkX; int16_t srcChunkY; int16_t srcChunkZ;
    uint32_t sourceGridID;

    // 12 bájt: Cél helye
    uint16_t tgtLocalIndex;
    int16_t tgtChunkX; int16_t tgtChunkY; int16_t tgtChunkZ;
    uint32_t targetGridID;

    // 16 bájt: Fizikai payload
    // Paletta indexeket használunk, hogy beleférjen a 8/16 bites bufferbe!
    uint32_t sourcePaletteIndex;
    uint32_t targetPaletteIndex;
    uint32_t force;
    uint32_t flags;

    // 24 bájt: Modder játszótér
    union {
        uint8_t rawModData[24];
        struct { float dirX, dirY, dirZ; uint32_t damage; uint64_t entityUUID; } mod_physics;
    };
};
#pragma pack(pop)

; // ~238 KB

// Chunk típusok azonosítása futásidőben
enum class ChunkTier : uint8_t { UNLOADED = 0, WILDERNESS_8BIT, NORMAL_16BIT, HEAVY_16BIT };

// Adatstruktúra a MemoryManager számára
struct ChunkBufferInfo {
    void* bufferTickAfter; // Nyers mutató a data_BufferB-re (Type Erasure)
    ChunkTier tier;
};

// Két különálló "Mérgező" Trash Bin a csonkolás elkerülésére (A méret megnövelve 64-re a szál-túlcsordulás miatt)
static uint8_t  TRASH_BIN_8BIT[MAX_SUPPORTED_THREADS]  = {0};
static uint16_t TRASH_BIN_16BIT[MAX_SUPPORTED_THREADS] = {0};

class MemoryManager {
public:
    static ChunkBufferInfo GetChunkTickAfterBuffer(uint32_t gridID, int16_t cx, int16_t cy, int16_t cz) {
        if (cx == 0) return { nullptr, ChunkTier::UNLOADED };
        if (cx == 1) return { nullptr, ChunkTier::WILDERNESS_8BIT };
        return { nullptr, ChunkTier::NORMAL_16BIT };
    }
};

// --- 4. SZERVEZŐ (SCHEDULER) ---
struct ArbiterJob { size_t startIndex; size_t endIndex; };

// --- AZ ARBITER TÉRBELI HASH TÁBLÁJA ---
constexpr uint32_t HASH_SIZE_MASK = 0xFFFFF;
constexpr uint32_t HASH_TABLE_SIZE = 1048576;
constexpr uint32_t EMPTY_SLOT = 0xFFFFFFFF;

class PhysicsArbiter {
private:
    static inline std::array<std::atomic<uint32_t>, HASH_TABLE_SIZE> spatialGrid;

    static uint32_t HashCoords(uint32_t gridID, uint16_t localIndex) {
        uint32_t hash = 2166136261u;
        hash ^= gridID;
        hash *= 16777619u;
        hash ^= localIndex;
        hash *= 16777619u;
        return hash & HASH_SIZE_MASK;
    }

public:
    static void ResetGrid() {
        for (uint32_t i = 0; i < HASH_TABLE_SIZE; ++i) {
            spatialGrid[i].store(EMPTY_SLOT, std::memory_order_relaxed);
        }
    }

    static void ResolveTargetConflicts(GlobalMoveCommand* commands, ArbiterJob job) {
        for (size_t i = job.startIndex; i < job.endIndex; ++i) {
            auto& myCmd = commands[i];
            std::atomic_ref<uint32_t> myFlags(myCmd.flags);

            if (myFlags.load(std::memory_order_relaxed) & FLAG_REJECTED) continue;

            uint32_t hash = HashCoords(myCmd.targetGridID, myCmd.tgtLocalIndex);
            uint32_t currentWinnerIndex = spatialGrid[hash].load(std::memory_order_relaxed);
            bool resolved = false;

            while (!resolved) {
                if (currentWinnerIndex == EMPTY_SLOT) {
                    if (spatialGrid[hash].compare_exchange_weak(
                            currentWinnerIndex, i,
                            std::memory_order_release,
                            std::memory_order_relaxed
                        ))
                    {
                        resolved = true;
                    }
                } else {
                    GlobalMoveCommand& competingCmd = commands[currentWinnerIndex];
                    std::atomic_ref<uint32_t> competingFlags(competingCmd.flags);

                    if (myCmd.targetGridID == competingCmd.targetGridID &&
                        myCmd.tgtLocalIndex == competingCmd.tgtLocalIndex)
                    {
                        if (myCmd.force > competingCmd.force) {
                            if (spatialGrid[hash].compare_exchange_weak(
                                    currentWinnerIndex, i,
                                    std::memory_order_release,
                                    std::memory_order_relaxed))
                            {
                                competingFlags.fetch_or(FLAG_REJECTED, std::memory_order_relaxed);
                                resolved = true;
                            }
                        } else {
                            myFlags.fetch_or(FLAG_REJECTED, std::memory_order_relaxed);
                            resolved = true;
                        }
                    } else {
                        hash = (hash + 1) & HASH_SIZE_MASK;
                        currentWinnerIndex = spatialGrid[hash].load(std::memory_order_relaxed);
                    }
                }
            }
        }
    }
};

// --- 5. COMMIT FÁZIS (3-Tier Támogatással) ---
class PhysicsEngine {
public:
    static void CommitSources(GlobalMoveCommand* commands, ArbiterJob job, int threadID) {
        // Biztonsági rács a TRASH_PTR túlcsordulása ellen
        uint8_t* const TRASH_PTR_8  = &TRASH_BIN_8BIT[threadID % MAX_SUPPORTED_THREADS];
        uint16_t* const TRASH_PTR_16 = &TRASH_BIN_16BIT[threadID % MAX_SUPPORTED_THREADS];

        for (size_t i = job.startIndex; i < job.endIndex; ++i) {
            auto& cmd = commands[i];

            ChunkBufferInfo srcInfo = MemoryManager::GetChunkTickAfterBuffer(cmd.sourceGridID, cmd.srcChunkX, cmd.srcChunkY, cmd.srcChunkZ);

            uint32_t isLoaded = (srcInfo.tier != ChunkTier::UNLOADED);
            uint32_t isAccepted = ((cmd.flags & FLAG_REJECTED) == 0);
            uint32_t execute = isAccepted & isLoaded;

            if (srcInfo.tier == ChunkTier::WILDERNESS_8BIT) {
                uint8_t* rawBuf = static_cast<uint8_t*>(srcInfo.bufferTickAfter);
                // JAVÍTVA UB: Ha a chunk nincs betöltve, a rawBuf nullptr. Ha ehhez adjuk hozzá az indexet,
                // az C++ Undefined Behavior, még akkor is, ha a ternary operátorral elkerüljük az írást!
                // A safeBuf és safeIndex garantálja, hogy a memóriacím mindig érvényes, elágazás nélkül.
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

    static void CommitTargets(GlobalMoveCommand* commands, ArbiterJob job, int threadID) {
        uint8_t* const TRASH_PTR_8  = &TRASH_BIN_8BIT[threadID % MAX_SUPPORTED_THREADS];
        uint16_t* const TRASH_PTR_16 = &TRASH_BIN_16BIT[threadID % MAX_SUPPORTED_THREADS];

        for (size_t i = job.startIndex; i < job.endIndex; ++i) {
            auto& cmd = commands[i];

            ChunkBufferInfo tgtInfo = MemoryManager::GetChunkTickAfterBuffer(cmd.targetGridID, cmd.tgtChunkX, cmd.tgtChunkY, cmd.tgtChunkZ);

            uint32_t isLoaded = (tgtInfo.tier != ChunkTier::UNLOADED);
            uint32_t isAccepted = ((cmd.flags & FLAG_REJECTED) == 0);
            uint32_t canOverwrite = ((cmd.flags & FLAG_OVERWRITE_SOLID) != 0);

            if (tgtInfo.tier == ChunkTier::WILDERNESS_8BIT) {
                uint8_t* rawBuf = static_cast<uint8_t*>(tgtInfo.bufferTickAfter);

                // JAVÍTVA UB: Itt garantáljuk, hogy sose olvassunk memóriaszemetet az isAir kiszámításánál,
                // ha a chunk nincs betöltve. A safeBuf[0]-ra fallbackel, ami mindig valid a TRASH_BIN miatt.
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

// --- ÚJ: 7. FÉNY MOTOR (ASZINKRON JOB) ---
class LightEngine {
public:
    // A Job Scheduler CSAK azokat a chunkokat adja át, ahol chunk->requiresLightUpdate != 0!
    // Így az inaktív területeket (a chunkok 90%-át) a CPU meg sem próbálja beolvasni.
    template <typename IndexType, size_t MaxPaletteSize>
    static void ProcessLightJob(NF::Core::MacroChunk<IndexType, MaxPaletteSize>* chunk) {

        // 1. Ha a chunk eddig a "Dummy" blokkokra mutatott, lekérünk egy valódi, írható memóriát.
        if (chunk->lightMapIndex <= 1) {
            chunk->lightMapIndex = GlobalLightPool::AllocateNewLightMap();
        }

        // Biztonsági vonal: Ha kifogytunk a medencéből (Index 0-t kaptunk), kiszállunk.
        if (chunk->lightMapIndex == 0) return;

        // 2. Kőkemény Branchless Hot Path az AVX2-nek
        uint16_t* myLightData = GlobalLightPool::pool[chunk->lightMapIndex].data;

        // ... IDE JÖN A FÉNYTERJEDÉS (Flood fill algoritmus) ...
        // myLightData[i] = ...

        // 3. Kész vagyunk! Kinyomjuk a flaget, így a Scheduler legközelebb
        // azonnal a "Kukába rakja" a frissítési kérelmet, amíg nem történik új blokk-lerakás.
        chunk->requiresLightUpdate = 0;
    }
};

// --- 8. A "START" GOMB ---
int main() {
    // Inicializáljuk a lock-free rácsot a start előtt, különben véletlen memóriaszeméttel próbálna ütközést vizsgálni!
    PhysicsArbiter::ResetGrid();

    GlobalLightPool::Initialize();
    std::cout << "Data-Oriented Voxel Engine (LightPool) inicializalva..." << std::endl;

    return 0;
}