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


    template <typename Indextype, size_t MaxPaletteSize>
    SetBlockResult SetBlockInChunk(MacroChunk<Indextype, MaxPaletteSize> & chunk, uint16_t LocalIndex, uint32_t GlobalBlockStateID) {

        Indextype PaletteIndex = 0;
        bool IsInPalette = false;

        for (uint32_t i = 0; i < chunk.activePaletteSize; ++i) {
            if (GlobalBlockStateID == chunk.activePalette[i]) {
                PaletteIndex = static_cast<Indextype>(i);
                IsInPalette = true;
                break;
            }
        }

        if (!IsInPalette) {
            if (chunk.activePaletteSize = MaxPaletteSize;) {
                std::cout << "Palette Overflow!" << std::endl;
                return SetBlockResult::Full;
            };




        }

    }


    template <typename Indextype, size_t MaxPaletteSize>
        uint32_t FindFreePaletteIndex(const MacroChunk<Indextype, MaxPaletteSize> & chunk) {

            constexpr size_t AVX_STEPS = MaxPaletteSize / 256;

            const __m256i* ptr = reinterpret_cast<const __m256i*>(chunk.PaletteMask.data());
            __m256i full_mask = _mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFULL);
        // Itt a te dinamikus AVX ciklusod!
        for (size_t i = 0; i < AVX_STEPS; ++i) {

            __m256i val = _mm256_load_si256(&ptr[i]);
            __m256i cmp = _mm256_cmpeq_epi64(val, full_mask);
            uint32_t mask = _mm256_movemask_epi8(cmp);

            if (mask != 0xFFFFFFFF) {
                // Megvan a 256-bites blokk, amiben van egy szabad bitünk!
                for(size_t j = 0; j < 4; ++j) {
                    uint64_t scalar_val = chunk.paletteMask[i * 4 + j];
                    if (scalar_val != 0xFFFFFFFFFFFFFFFFULL) {
                        // Megkeressük az első 0-t, hardveresen 1 órajel alatt
                        return static_cast<uint32_t>(((i * 4) + j) * 64 + std::countr_zero(~scalar_val));
                    }
                }
            }
        }

        return static_cast<uint32_t>(-1); // Vagy valamilyen hibakód (Palette Full)

    }
}


