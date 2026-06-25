//
// Fájl: Core/Physics.hpp
// Készítette: NexusForge Engine (Rick & Gem)
//
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include "MacroChunk.hpp"
#include "../Render/CameraMath.hpp"

extern std::vector<NF::Core::MacroChunk_Small> realWorld;

namespace NF::Core::Physics {

    inline bool IsVoxelSolid(float worldX, float worldY, float worldZ) {
        int32_t gx = static_cast<int32_t>(std::floor(worldX));
        int32_t gy = static_cast<int32_t>(std::floor(worldY));
        int32_t gz = static_cast<int32_t>(std::floor(worldZ));

        int32_t cx = gx >> 4;
        int32_t cy = gy >> 4;
        int32_t cz = gz >> 4;

        if (cy >= 2) return false;
        if (cy < 0) return true;
        if (cx < 0 || cx >= 8 || cz < 0 || cz >= 8) return true;

        uint32_t chunkIndex = cx + (cz * 8) + (cy * 64);
        int32_t lx = gx & 15;
        int32_t ly = gy & 15;
        int32_t lz = gz & 15;
        uint32_t localIndex = lx + (ly * 16) + (lz * 256);

        return realWorld[chunkIndex].tickAfter[localIndex] != 0;
    }

    struct RaycastResult {
        bool hit;
        int32_t hitX, hitY, hitZ;
        int32_t normalX, normalY, normalZ;
    };

    inline RaycastResult VoxelRaycast(NF::Render::Vec3 origin, NF::Render::Vec3 direction, float maxDistance) {
        int32_t x = static_cast<int32_t>(std::floor(origin.x));
        int32_t y = static_cast<int32_t>(std::floor(origin.y));
        int32_t z = static_cast<int32_t>(std::floor(origin.z));

        int32_t stepX = (direction.x > 0) ? 1 : ((direction.x < 0) ? -1 : 0);
        int32_t stepY = (direction.y > 0) ? 1 : ((direction.y < 0) ? -1 : 0);
        int32_t stepZ = (direction.z > 0) ? 1 : ((direction.z < 0) ? -1 : 0);

        float tDeltaX = (stepX != 0) ? std::abs(1.0f / direction.x) : INFINITY;
        float tDeltaY = (stepY != 0) ? std::abs(1.0f / direction.y) : INFINITY;
        float tDeltaZ = (stepZ != 0) ? std::abs(1.0f / direction.z) : INFINITY;

        float tMaxX = (stepX > 0) ? (std::floor(origin.x) + 1.0f - origin.x) * tDeltaX : (origin.x - std::floor(origin.x)) * tDeltaX;
        float tMaxY = (stepY > 0) ? (std::floor(origin.y) + 1.0f - origin.y) * tDeltaY : (origin.y - std::floor(origin.y)) * tDeltaY;
        float tMaxZ = (stepZ > 0) ? (std::floor(origin.z) + 1.0f - origin.z) * tDeltaZ : (origin.z - std::floor(origin.z)) * tDeltaZ;

        if (std::isnan(tMaxX) || std::isinf(tMaxX)) tMaxX = INFINITY;
        if (std::isnan(tMaxY) || std::isinf(tMaxY)) tMaxY = INFINITY;
        if (std::isnan(tMaxZ) || std::isinf(tMaxZ)) tMaxZ = INFINITY;

        int32_t steppedAxis = -1;
        float distance = 0.0f;

        while (distance <= maxDistance) {
            if (IsVoxelSolid(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))) {
                return {
                    true, x, y, z,
                    (steppedAxis == 0) ? -stepX : 0,
                    (steppedAxis == 1) ? -stepY : 0,
                    (steppedAxis == 2) ? -stepZ : 0
                };
            }
            if (tMaxX < tMaxY) {
                if (tMaxX < tMaxZ) { distance = tMaxX; tMaxX += tDeltaX; x += stepX; steppedAxis = 0; }
                else               { distance = tMaxZ; tMaxZ += tDeltaZ; z += stepZ; steppedAxis = 2; }
            } else {
                if (tMaxY < tMaxZ) { distance = tMaxY; tMaxY += tDeltaY; y += stepY; steppedAxis = 1; }
                else               { distance = tMaxZ; tMaxZ += tDeltaZ; z += stepZ; steppedAxis = 2; }
            }
        }
        return {false, 0, 0, 0, 0, 0, 0};
    }

    struct AABB {
        float width, height, depth;
    };

    struct Entity {
        NF::Render::Vec3 position;
        NF::Render::Vec3 velocity;
        AABB hitbox;
        bool isGrounded = false;

        // --- ÚJ ATTRIBÚTUMOK A GAMEPLAY-HEZ ---
        bool isFlying = false;
        float pendingFallDamage = 0.0f; // Kinetikus sokk tárolója

        void GetBounds(NF::Render::Vec3 pos, NF::Render::Vec3& minOut, NF::Render::Vec3& maxOut) const {
            minOut = { pos.x - hitbox.width * 0.5f, pos.y, pos.z - hitbox.depth * 0.5f };
            maxOut = { pos.x + hitbox.width * 0.5f, pos.y + hitbox.height, pos.z + hitbox.depth * 0.5f };
        }
    };

    inline bool CheckAABBCollision(const Entity& ent, NF::Render::Vec3 testPos) {
        NF::Render::Vec3 minB, maxB;
        ent.GetBounds(testPos, minB, maxB);

        int32_t minX = static_cast<int32_t>(std::floor(minB.x));
        int32_t minY = static_cast<int32_t>(std::floor(minB.y));
        int32_t minZ = static_cast<int32_t>(std::floor(minB.z));
        int32_t maxX = static_cast<int32_t>(std::floor(maxB.x));
        int32_t maxY = static_cast<int32_t>(std::floor(maxB.y));
        int32_t maxZ = static_cast<int32_t>(std::floor(maxB.z));

        for (int32_t y = minY; y <= maxY; ++y) {
            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    if (IsVoxelSolid(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z))) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    inline void UpdateEntityPhysics(Entity& ent, float deltaTime) {

        // --- 1. GRAVITÁCIÓ ÉS LÉGELLENÁLLÁS ---
        if (ent.isFlying) {
            // Nincs gravitáció! Súrlódás (légellenállás) kiterjesztve minden tengelyre.
            ent.velocity.x *= std::pow(0.0001f, deltaTime);
            ent.velocity.y *= std::pow(0.0001f, deltaTime);
            ent.velocity.z *= std::pow(0.0001f, deltaTime);
        } else {
            // Normál fizika (Zuhanás)
            ent.velocity.y -= 25.0f * deltaTime;
            ent.velocity.x *= std::pow(0.001f, deltaTime);
            ent.velocity.z *= std::pow(0.001f, deltaTime);
        }

        NF::Render::Vec3 moveDelta = {
            ent.velocity.x * deltaTime,
            ent.velocity.y * deltaTime,
            ent.velocity.z * deltaTime
        };

        ent.isGrounded = false;

        // X TENGELY ÜTKÖZÉS
        if (moveDelta.x != 0.0f) {
            NF::Render::Vec3 testPos = ent.position;
            testPos.x += moveDelta.x;
            if (CheckAABBCollision(ent, testPos)) ent.velocity.x = 0.0f;
            else ent.position.x = testPos.x;
        }

        // Y TENGELY ÜTKÖZÉS (Gravitáció / Ugrás / Fall Damage)
        if (moveDelta.y != 0.0f) {
            NF::Render::Vec3 testPos = ent.position;
            testPos.y += moveDelta.y;

            if (CheckAABBCollision(ent, testPos)) {
                // Ha LEFELÉ mozogtunk és betont fogtunk:
                if (ent.velocity.y < 0.0f) {
                    ent.isGrounded = true;

                    // --- KINETIKUS SEBZÉS (FALL DAMAGE) KISZÁMÍTÁSA ---
                    // Ha nem repültünk, és a sebesség kritikus volt (-14 m/s az kb 3 blokk)
                    if (!ent.isFlying && ent.velocity.y < -14.0f) {
                        ent.pendingFallDamage = std::abs(ent.velocity.y) - 14.0f;
                    }
                }
                ent.velocity.y = 0.0f; // Ütközéskor a sebesség lenullázódik
            } else {
                ent.position.y = testPos.y;
            }
        }

        // Z TENGELY ÜTKÖZÉS
        if (moveDelta.z != 0.0f) {
            NF::Render::Vec3 testPos = ent.position;
            testPos.z += moveDelta.z;
            if (CheckAABBCollision(ent, testPos)) ent.velocity.z = 0.0f;
            else ent.position.z = testPos.z;
        }
    }
}