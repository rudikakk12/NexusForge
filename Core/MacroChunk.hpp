//
// Fájl: Macrochunk.hpp
// Készítette: Rick, Dátum: 2026. 06. 15..
//
#pragma once
#include <array>
#include <cstdint>
#include <immintrin.h>



namespace NF::Core {


    constexpr uint32_t NEEDS_COMPRESSION_FLAG_SHIFT         = 31;


    struct MacroChunkCoordinates {
        union {
            uint64_t GridID : 24,
                     X : 8,
                     Y : 8,
                     Z : 8,
                     LocalIndex : 16;
        };

    };

    constexpr size_t MAX_BITPLANES = 8;
    constexpr size_t MAX_COARSE_GRIDS = 4;

    struct alignas(64) MacroChunk_Small {

        // I. A FEJLÉC (Fix 64 bájt, optimalizálva az L1 Cache-hez és a memóriahatárokhoz)
        // JAVÍTVA: A sorrend úgy lett optimalizálva, hogy pontosan a memóriahatárokra essen
        // és a hasznos adat pontosan 32 bájtot tegyen ki rejtett lyukak nélkül.
        uint32_t flags               = 0; // 4 bájt (Offset 0. Bit 0 = activeBuffer, 31.Bit = )
        uint32_t activePaletteSize   = 0; // 4 bájt (Offset 4)
        uint32_t dirtyPlanesMask     = 0; // 4 bájt (Offset 8)

        uint32_t Grid_ID             = 0; // 4 bájt (Offset 12 -> TÖKÉLETES 4 bájtos alignment!)
        uint32_t lastUpdateTick      = 0; // 4 bájt (Offset 16. Mikor nyúltunk hozzá utoljára?)
        uint32_t lightMapIndex       = 0; // 4 bájt (Offset 20. Mutató a GlobalLightPool-ba)

        uint8_t  macroChunkX         = 0; // 1 bájt (Offset 24. Világ koordináták)
        uint8_t  macroChunkY         = 0; // 1 bájt (Offset 26.)
        uint8_t  macroChunkZ         = 0; // 1 bájt (Offset 28.)
        int8_t padding2;                  // 1 bájt


        uint8_t  chunkState          = 0; // 1 bájt (Offset 30. Enum: 0=Empty, 1=Generating, 2=Ready)
        uint8_t  requiresLightUpdate = 0; // 1 bájt (Offset 31. Scheduler "Kuka" szűrő flag)

        int8_t padding3[2];               // 2bájt

        // Összesen PONTOSAN 32 bájt a hasznos adat! Nincs többé implicit padding csúszás.
        uint32_t GridID_TOP          = 0;  // 4 bájt
        uint32_t GridID_BOTTOM       = 0;  // 4 bájt
        uint32_t GridID_NORTH        = 0;  // 4 bájt
        uint32_t GridID_EAST         = 0;  // 4 bájt
        uint32_t GridID_SOUTH        = 0;  // 4 bájt
        uint32_t GridID_WEST         = 0;  // 4 bájt
        // 64 - 32 = 32 bájt padding a tökéletes L1 Cache igazításhoz! (29-ről 32-re javítva)
        uint8_t  _padding[8]        = {};

        alignas(32) std::array<uint64_t, 4> PaletteMask         = {};

        // II. FORRÓ AVX2 ZÓNA (Fix eltolások, maximális sűrűség)

        // FIGYELEM: A 0. elemnek (index 0) mindkét tömbben a levegőnek kell lennie! (Globális ID: 0)
        // 0: IS_SOLID, 1:IS_REPLACEABLE, 2:IS_LIQUID, 3:IS_HAZARD, 4:NEEDS_RANDOM_TICK, 5:FACE_CULLING_MERGE, 6-9:OPACITY, 10-15: RESERVED
        alignas(32) std::array<uint16_t, 256> PaletteProperties = {};
        alignas(32) std::array<uint32_t, 256> PaletteGlobalBlockStateIDs = {};

        alignas(32) std::array<uint8_t, 4096> data_BufferA = {};
        alignas(32) std::array<uint8_t, 4096> data_BufferB = {};


        alignas(32) std::array<std::array<uint64_t, 64>, MAX_BITPLANES> bitPlanes = {};
        alignas(32) std::array<std::array<uint8_t, 64>, MAX_COARSE_GRIDS> coarseGrids = {};

        // III. HIDEG ZÓNA (A struktúra legvégén)
        uint8_t* tickNow   = nullptr;
        uint8_t* tickAfter = nullptr;

        MacroChunk_Small() {
            tickNow   = data_BufferA.data();
            tickAfter = data_BufferB.data();

            // --- EZZEL A KÉT SORRAL MEGJAVUL A VILÁG ---
            PaletteMask[0] = 1ULL; // A 0. bitet (Levegő) fixen lefoglaljuk!
            activePaletteSize = 1; // 1 elem már a palettán van.
        }

        void SwapTickBuffers() { std::swap(tickNow, tickAfter); }
    };

     struct alignas(64) MacroChunk_Big {

        // I. A FEJLÉC (Fix 64 bájt, optimalizálva az L1 Cache-hez és a memóriahatárokhoz)
        // JAVÍTVA: A sorrend úgy lett optimalizálva, hogy pontosan a memóriahatárokra essen
        // és a hasznos adat pontosan 32 bájtot tegyen ki rejtett lyukak nélkül.
        uint32_t flags               = 0; // 2 bájt (Offset 0. Bit 0 = activeBuffer, 31.Bit = )
        uint32_t activePaletteSize   = 0; // 4 bájt (Offset 4)
        uint32_t dirtyPlanesMask     = 0; // 4 bájt (Offset 8)

        uint32_t Grid_ID             = 0; // 4 bájt (Offset 12 -> TÖKÉLETES 4 bájtos alignment!)
        uint32_t lastUpdateTick      = 0; // 4 bájt (Offset 16. Mikor nyúltunk hozzá utoljára?)
        uint32_t lightMapIndex       = 0; // 4 bájt (Offset 20. Mutató a GlobalLightPool-ba)

        uint8_t  macroChunkX         = 0; // 2 bájt (Offset 24. Világ koordináták)
        uint8_t  macroChunkY         = 0; // 2 bájt (Offset 26.)
        uint8_t  macroChunkZ         = 0; // 2 bájt (Offset 28.)
        int8_t padding2;


        uint8_t  chunkState          = 0; // 1 bájt (Offset 30. Enum: 0=Empty, 1=Generating, 2=Ready)
        uint8_t  requiresLightUpdate = 0; // 1 bájt (Offset 31. Scheduler "Kuka" szűrő flag)
        int8_t padding3[2];              // 2 bájt

        // Összesen PONTOSAN 32 bájt a hasznos adat! Nincs többé implicit padding csúszás.
        uint32_t GridID_TOP          = 0;  // 4 bájt
        uint32_t GridID_BOTTOM       = 0;  // 4 bájt
        uint32_t GridID_NORTH        = 0;  // 4 bájt
        uint32_t GridID_EAST         = 0;  // 4 bájt
        uint32_t GridID_SOUTH        = 0;  // 4 bájt
        uint32_t GridID_WEST         = 0;  // 4 bájt
        // 64 - 32 = 32 bájt padding a tökéletes L1 Cache igazításhoz! (29-ről 32-re javítva)
        uint8_t  _padding[8]        = {};

        alignas(32) std::array<uint64_t, 66> PaletteMask         = {};

        // II. FORRÓ AVX2 ZÓNA (Fix eltolások, maximális sűrűség)

        // FIGYELEM: A 0. elemnek (index 0) mindkét tömbben a levegőnek kell lennie! (Globális ID: 0)
        // 0: IS_SOLID, 1:IS_REPLACEABLE, 2:IS_LIQUID, 3:IS_HAZARD, 4:NEEDS_RANDOM_TICK, 5:FACE_CULLING_MERGE, 6-9:OPACITY, 10-15: RESERVED
        alignas(32) std::array<uint16_t, 4224> PaletteProperties = {};
        alignas(32) std::array<uint32_t, 4224> PaletteGlobalBlockStateIDs = {};

        alignas(32) std::array<uint16_t, 4096> data_BufferA = {};
        alignas(32) std::array<uint16_t, 4096> data_BufferB = {};


        alignas(32) std::array<std::array<uint64_t, 64>, MAX_BITPLANES> bitPlanes = {};
        alignas(32) std::array<std::array<uint8_t, 64>, MAX_COARSE_GRIDS> coarseGrids = {};

        // III. HIDEG ZÓNA (A struktúra legvégén)
        uint16_t* tickNow   = nullptr;
        uint16_t* tickAfter = nullptr;

        MacroChunk_Big() {
            tickNow   = data_BufferA.data();
            tickAfter = data_BufferB.data();

            // --- EZZEL A KÉT SORRAL MEGJAVUL A VILÁG ---
            PaletteMask[0] = 1ULL; // A 0. bitet (Levegő) fixen lefoglaljuk!
            activePaletteSize = 1; // 1 elem már a palettán van.
        }

        void SwapTickBuffers() { std::swap(tickNow, tickAfter); }
    };

    bool UpgradeChunkPalette() {return true;}

    bool CheckMacroChunkPalette_Small(MacroChunk_Small & chunk) {

        constexpr size_t MaskArraySize = 4;
        uint64_t TempPaletteMask[MaskArraySize] = {0};

        const uint8_t* Voxel = chunk.tickNow;

        for (uint32_t i = 0; i < 4096; ++i) {
            uint8_t TempPaletteIndex = Voxel[i];

            TempPaletteMask[TempPaletteIndex >> 6] |= (1ULL << (TempPaletteIndex & 63));
            }

        uint8_t NewPaletteSize = 0;
        uint32_t NeedsCompression = 0;

        for (uint32_t i = 0; i < MaskArraySize; ++i) {

            uint64_t OldMask = chunk.PaletteMask[i];

            chunk.PaletteMask[i] &= TempPaletteMask[i];
            NewPaletteSize += std::popcount(chunk.PaletteMask[i]);

            NeedsCompression |= (OldMask != chunk.PaletteMask[i]);
        }
        chunk.flags |= NeedsCompression << NEEDS_COMPRESSION_FLAG_SHIFT;

        chunk.activePaletteSize = NeedsCompression ? NewPaletteSize : chunk.activePaletteSize;
    return true;
    }



    }

