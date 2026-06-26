//
// Fájl: Core/WorldGeneration.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Architektúra: Procedurális, zaj-alapú terepgenerátor modul
//
#pragma once

#include <cmath>
#include <cstdint>
#include "MacroChunk.hpp"

namespace NF::Core {

    // Ezt a struktúrát fogjuk később konfig fájlokból (pl. JSON/Mod Lua) betölteni
    struct TerrainConfig {
        int seaLevel = 30;              // Alapmagasság
        float globalScale = 0.005f;     // Mennyire legyenek "szélesek" a hegyek (kisebb érték = lankásabb)
        float heightMultiplier = 25.0f; // Milyen magasak legyenek a hegyek
        int octaves = 3;                // Zaj részletessége (Több oktáv = rücskösebb hegyek)
    };

    class WorldGenerator {
    private:
        TerrainConfig config;

        // Gyors, belső pszeudo-random Hash függvény 2D koordinátákhoz
        // JAVÍTOTT: Gyors Bitwise DOD Hash (nincs std::sin lebegőpontos lassulás!)
        float Random2D(float x, float y) const {
            uint32_t a = static_cast<uint32_t>(static_cast<int32_t>(x));
            uint32_t b = static_cast<uint32_t>(static_cast<int32_t>(y));
            a *= 3284157443u; b ^= a << 16 | a >> 16;
            b *= 1911520717u; a ^= b << 16 | b >> 16;
            a *= 2048419325u;
            return static_cast<float>(a) / 4294967295.0f; // 0.0 és 1.0 között
        }

        // JAVÍTOTT: Tökéletes bilineáris interpoláció
        float SmoothNoise(float x, float y) const {
            float ix = std::floor(x); float iy = std::floor(y);
            float fx = x - ix; float fy = y - iy;

            float a = Random2D(ix, iy);
            float b = Random2D(ix + 1.0f, iy);
            float c = Random2D(ix, iy + 1.0f);
            float d = Random2D(ix + 1.0f, iy + 1.0f);

            float ux = fx * fx * (3.0f - 2.0f * fx);
            float uy = fy * fy * (3.0f - 2.0f * fy);

            // A MATEK HELYRE RAKVA!
            return a + ux * (b - a) + uy * (c - a) + ux * uy * (a - b - c + d);
        }

        // Fraktál Brown-mozgás (fBm) az igazi hegyvonulatokhoz
        float FractalNoise(float x, float z) const {
            float total = 0.0f;
            float frequency = config.globalScale;
            float amplitude = 1.0f;
            float maxValue = 0.0f;

            for (int i = 0; i < config.octaves; ++i) {
                total += SmoothNoise(x * frequency, z * frequency) * amplitude;
                maxValue += amplitude;
                amplitude *= 0.5f; // Minden oktáv fele olyan magas
                frequency *= 2.0f; // Minden oktáv kétszer sűrűbb
            }
            return total / maxValue; // Normalizálás 0.0 és 1.0 közé
        }

    public:
        WorldGenerator(TerrainConfig cfg = {}) : config(cfg) {}

        void SetConfig(const TerrainConfig& cfg) { config = cfg; }

void Generate(MacroChunk_Small& chunk, int cx, int cy, int cz) const {
            // Paletta a shadernek - HIVATALOS GLOBAL ID-K BEKÖTÉSE!
            chunk.PaletteGlobalBlockStateIDs[0] = 0; // Levegő
            chunk.PaletteGlobalBlockStateIDs[1] = 1; // Föld (barna)
            chunk.PaletteGlobalBlockStateIDs[2] = 2; // Fű (zöld tetejű)
            chunk.PaletteGlobalBlockStateIDs[3] = 3; // Kő (sima)
            chunk.PaletteGlobalBlockStateIDs[4] = 4; // Zúzottkő
            chunk.PaletteGlobalBlockStateIDs[5] = 5; // Obszidián
            chunk.activePaletteSize = 6; // Hivatalosan regisztráltuk mind a 6-ot

            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    int globalX = cx * 16 + x;
                    int globalZ = cz * 16 + z;

                    // 1. Fraktál zaj kiszámítása a (X, Z) oszlopra
                    float noiseVal = FractalNoise(static_cast<float>(globalX), static_cast<float>(globalZ));

                    // 2. Felszín magasságának kiszámítása
                    int surfaceHeight = config.seaLevel + static_cast<int>(noiseVal * config.heightMultiplier);

                    for (int y = 0; y < 16; ++y) {
                        int globalY = cy * 16 + y;
                        uint8_t blockID = 0;

                        if (globalY > surfaceHeight) {
                            blockID = 0; // Levegő
                        } else if (globalY == surfaceHeight) {
                            blockID = 2; // FŰ (A 2-es a fű!)
                        } else if (globalY >= surfaceHeight - 3) {
                            blockID = 1; // FÖLD (Az 1-es a föld!)
                        } else {
                            blockID = 3; // KŐ
                        }

                        if (blockID != 0) {
                            uint32_t localIndex = x + (y * 16) + (z * 256);
                            chunk.tickNow[localIndex] = blockID;
                            chunk.tickAfter[localIndex] = blockID;
                        }
                    }
                }
            }
        }
    };
}