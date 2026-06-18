//
// Fájl: BlockPlacement.hpp
// Készítette: Rick, Dátum: 2026. 06. 15..
//
#pragma once

#include <cstdint>
#include <iostream>
#include <ostream>
#include <immintrin.h>
#include "MacroChunk.hpp"

namespace NF::Core {
    enum SetBlockResult {Success, Full, OutOfBounds};

    uint32_t FindFreePaletteIndex_Small(MacroChunk_Small & chunk) {

        // A PaletteMask pontosan 4 db 64-bites szám, azaz 256 bit (32 bájt).
        // EGYETLEN AVX töltéssel a teljes maszk bent van a processzorban! Nincs szükség ciklusra.
        __m256i val = _mm256_load_si256(reinterpret_cast<const __m256i*>(chunk.PaletteMask.data()));

        // Összehasonlítjuk egy csupa 1-esekből álló vektorral (Minden bit 1-es = tele van a paletta)
        __m256i full_mask = _mm256_set1_epi32(-1); // Elegánsabb módja a csupa 1-es generálásának
        __m256i cmp = _mm256_cmpeq_epi64(val, full_mask);

        // Visszakapunk egy 32-bites maszkot (minden bájt eredménye 1 bit)
        uint32_t mask = _mm256_movemask_epi8(cmp);

        // Ha a mask 0xFFFFFFFF, az azt jelenti, hogy minden bit 1-es, azaz a paletta teljesen tele van.
        if (mask != 0xFFFFFFFF) {
            // Ha idáig eljutottunk, BIZTOSAN van legalább egy szabad bitünk!
            // Most már csak végignézzük a 4 darab 64-bites szót hagyományosan, ami szupergyors.
            for (size_t j = 0; j < 4; ++j) {
                uint64_t scalar_val = chunk.PaletteMask[j];
                if (scalar_val != 0xFFFFFFFFFFFFFFFFULL) {
                    // Megkeressük az első 0 bitet (negáljuk a val-t, így a 0-kból 1-es lesz, és countr_zero)
                    return static_cast<uint32_t>((j * 64) + std::countr_zero(~scalar_val));
                }
            }
        }

        return static_cast<uint32_t>(-1); // Hibakód: A paletta menthetetlenül tele van.
    }

    SetBlockResult SetBlockInChunk_Small(MacroChunk_Small & chunk, uint16_t LocalIndex, uint32_t GlobalBlockStateID) {

        uint16_t PaletteIndex = 0;
        uint32_t IsInPalette = 0;
        const __m256i* ptr = reinterpret_cast<const __m256i*>(GlobalBlockStateID);
        uint32_t FirstFreeIndex;

        __m256i target = _mm256_set1_epi32(GlobalBlockStateID);
        __m256i const * base_ptr = (__m256i const*)chunk.PaletteGlobalBlockStateIDs.data();
        uint64_t FinalMask;
        for (uint32_t i = 0; i < 32; i = i + 8){

            __m256i d0 = _mm256_loadu_si256(base_ptr + i + 0);
            __m256i c0 = _mm256_cmpeq_epi32(target, d0);

            __m256i d1 = _mm256_loadu_si256(base_ptr + i + 1);
            __m256i c1 = _mm256_cmpeq_epi32(target, d1);
            uint8_t m0 = _mm256_movemask_ps(_mm256_castsi256_ps(c0));

            __m256i d2 = _mm256_loadu_si256(base_ptr + i + 2);
            __m256i c2 = _mm256_cmpeq_epi32(target, d2);
            uint8_t m1 = _mm256_movemask_ps(_mm256_castsi256_ps(c1));

            __m256i d3 = _mm256_loadu_si256(base_ptr + i + 3);
            __m256i c3 = _mm256_cmpeq_epi32(target, d3);
            uint8_t m2 = _mm256_movemask_ps(_mm256_castsi256_ps(c2));

            __m256i d4 = _mm256_loadu_si256(base_ptr + i + 4);
            __m256i c4 = _mm256_cmpeq_epi32(target, d4);
            uint8_t m3 = _mm256_movemask_ps(_mm256_castsi256_ps(c3));

            __m256i d5 = _mm256_loadu_si256(base_ptr + i + 5);
            __m256i c5 = _mm256_cmpeq_epi32(target, d5);
            uint8_t m4 = _mm256_movemask_ps(_mm256_castsi256_ps(c4));

            __m256i d6 = _mm256_loadu_si256(base_ptr + i + 6);
            __m256i c6 = _mm256_cmpeq_epi32(target, d6);
            uint8_t m5 = _mm256_movemask_ps(_mm256_castsi256_ps(c5));

            __m256i d7 = _mm256_loadu_si256(base_ptr + i + 7);
            __m256i c7 = _mm256_cmpeq_epi32(target, d7);
            uint8_t m6 = _mm256_movemask_ps(_mm256_castsi256_ps(c6));

            uint8_t m7 = _mm256_movemask_ps(_mm256_castsi256_ps(c7));


            FinalMask =      static_cast<uint64_t>(m0) |
                            (static_cast<uint64_t>(m1) << 8) |
                            (static_cast<uint64_t>(m2) << 16) |
                            (static_cast<uint64_t>(m3) << 24) |
                            (static_cast<uint64_t>(m4) << 32) |
                            (static_cast<uint64_t>(m5) << 40) |
                            (static_cast<uint64_t>(m6) << 48) |
                            (static_cast<uint64_t>(m7) << 56);
            if(FinalMask != 0){
                IsInPalette = 1;
                PaletteIndex = (i * 8) + std::countr_zero(FinalMask);
                break;
            }
        }


        if (IsInPalette == 0) {
            if (chunk.activePaletteSize == 256) {
                std::cout << "Palette Overflow!" << std::endl;
                if (CheckMacroChunkPalette_Small(chunk) == false){return SetBlockResult::Full;}
                else{
                    FirstFreeIndex = FindFreePaletteIndex_Small(chunk);
                    PaletteIndex = FirstFreeIndex;
                }
            }
            chunk.PaletteMask[PaletteIndex >> 6] |= (1ULL << (FirstFreeIndex & 63));
            chunk.PaletteGlobalBlockStateIDs[PaletteIndex] = GlobalBlockStateID;

            chunk.tickAfter[LocalIndex] = PaletteIndex;
            return SetBlockResult::Success;
        }



    }
}