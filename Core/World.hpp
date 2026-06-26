//
// Fájl: Core/World.hpp
//
#pragma once

#include <unordered_map>
#include <memory>
#include <cstdint>
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
}