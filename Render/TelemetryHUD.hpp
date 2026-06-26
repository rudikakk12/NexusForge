//
// Fájl: Render/TelemetryHUD.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Architektúra: Különválasztott, Mély-Analitikai UI réteg
//
#pragma once

#include <imgui.h>
#include <cstdint>
#include <cmath>

namespace NF::Render {

    // KIBŐVÍTETT STRUKTÚRA A TELJES MOTOR-ANALÍZISHEZ
    struct HUDMetrics {
        // Player & Inventory
        int playerHP;
        int maxHP;
        int selectedHotbarSlot;
        uint8_t* hotbarBlocks;

        // UI State
        bool telemetryMenuOpen;

        // Performance
        float deltaTime;
        float avg1s;
        float avg10s;
        float bestFrameTime;
        float worstFrameTime;

        // Culling & MDI
        int currentlyDrawnChunks;
        int totalCullArraySize;
        uint32_t activeVertices;
        uint32_t activeIndices;

        // Memory & VRAM
        float ramMb;
        float activeVramMb;
        float totalMegaArenaCapacity;
        int freeVramSlotsSize;

        // Asynchronous Queues
        int totalPendingUploads;
        int totalPendingUnloads;
        bool isWorkerThrottled; // ÚJ: Pihen-e a bányász szál a backpressure miatt?

        // Engine Controls
        int* renderDistance;
        bool isMouseCaptured;

        // Physics & World
        float px, py, pz; // Pontos lebegőpontos pozíció
        int64_t gcx, gcy, gcz; // Chunk koordináták
        float vx, vy, vz; // Sebesség vektor
        float lookX, lookY, lookZ; // Kamera nézési vektor
        bool isFlying;
        bool isGrounded;

        uint32_t WIDTH, HEIGHT;
    };

    class TelemetryHUD {
    public:
        static void DrawInGameHUD(HUDMetrics& m) {
            // ==========================================
            // FIX IN-GAME OVERLAY (Célkereszt + Hotbar)
            // ==========================================
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(m.WIDTH, m.HEIGHT));
            ImGui::Begin("InGameOverlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 center(m.WIDTH * 0.5f, m.HEIGHT * 0.5f);

            // Célkereszt
            drawList->AddLine(ImVec2(center.x - 8, center.y), ImVec2(center.x + 8, center.y), IM_COL32(255, 255, 255, 200), 2.0f);
            drawList->AddLine(ImVec2(center.x, center.y - 8), ImVec2(center.x, center.y + 8), IM_COL32(255, 255, 255, 200), 2.0f);

            // Életerő
            ImGui::SetCursorPos(ImVec2(20, m.HEIGHT - 70));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(220, 50, 50, 255));
            ImGui::ProgressBar((float)m.playerHP / m.maxHP, ImVec2(200, 20), "");
            ImGui::PopStyleColor();
            ImGui::SetCursorPos(ImVec2(25, m.HEIGHT - 70));
            ImGui::Text("HP: %d / %d", m.playerHP, m.maxHP);

            // Hotbar
            float hotbarWidth = 9 * 50.0f; float startX = (m.WIDTH - hotbarWidth) * 0.5f; float startY = m.HEIGHT - 65.0f;
            for (int i = 0; i < 9; ++i) {
                ImVec2 pos(startX + i * 50.0f, startY);
                ImU32 bgColor = (i == m.selectedHotbarSlot) ? IM_COL32(200, 200, 50, 180) : IM_COL32(40, 40, 40, 180);
                drawList->AddRectFilled(pos, ImVec2(pos.x + 45, pos.y + 45), bgColor, 4.0f);
                drawList->AddRect(pos, ImVec2(pos.x + 45, pos.y + 45), IM_COL32(0, 0, 0, 255), 3.0f, 0, 1.0f);
                char text[16]; snprintf(text, sizeof(text), "%d", i + 1);
                drawList->AddText(ImVec2(pos.x + 5, pos.y + 5), IM_COL32(255, 255, 255, 255), text);
                if (m.hotbarBlocks[i] != 0) {
                    char blkText[16]; snprintf(blkText, sizeof(blkText), "ID:%d", m.hotbarBlocks[i]);
                    drawList->AddText(ImVec2(pos.x + 5, pos.y + 25), IM_COL32(150, 255, 150, 255), blkText);
                }
            }
            ImGui::End();

            // ==========================================
            // KITERJESZTETT TELEMETRIA (F3)
            // ==========================================
            if (m.telemetryMenuOpen) {
                // Átlátszó háttér, hogy láss a pályán
                ImGui::SetNextWindowBgAlpha(0.65f);
                // Jobb oldalra dokkolt lebegő ablak
                ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(400, 680), ImGuiCond_FirstUseEver);

                ImGui::Begin("NEXUSFORGE TITAN ANALYTICS", &m.telemetryMenuOpen, ImGuiWindowFlags_NoSavedSettings);

                // FPS ÉS KERETIDŐK
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "FPS: %d", static_cast<int>(1.0f / (m.deltaTime > 0.0f ? m.deltaTime : 0.001f)));
                ImGui::Text("FrameTime: %.2f ms", m.deltaTime * 1000.0f);
                ImGui::TextDisabled("Avg 1s: %.2f ms | Avg 10s: %.2f ms", m.avg1s * 1000.0f, m.avg10s * 1000.0f);

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                // 1. ENGINE CONTROLS (Irányítás és Látótáv)
                if (ImGui::CollapsingHeader("Engine Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderInt("Render Distance", m.renderDistance, 2, 128, "%d Chunks");
                    ImGui::Text("Mouse Status: "); ImGui::SameLine();
                    if (m.isMouseCaptured) ImGui::TextColored(ImVec4(1, 0, 0, 1), "[CAPTURED] (Press 'F')");
                    else ImGui::TextColored(ImVec4(0, 1, 0, 1), "[FREE] (Press 'F')");
                }

                // 2. HARDWARE & MEMORY (RAM / VRAM)
                if (ImGui::CollapsingHeader("Hardware & Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("System RAM (Process): %.2f MB", m.ramMb);

                    float vramPercent = (m.activeVramMb / m.totalMegaArenaCapacity) * 100.0f;
                    ImGui::Text("VRAM (MDI Buffers): %.2f MB / %.1f MB", m.activeVramMb, m.totalMegaArenaCapacity);
                    ImGui::ProgressBar(vramPercent / 100.0f, ImVec2(-1, 14), "");

                    ImGui::Text("Free VRAM Slots (Reused): %d", m.freeVramSlotsSize);
                }

                // 3. ASYNC JOB SYSTEM (Multithreading Queues)
                if (ImGui::CollapsingHeader("Async Job System", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Pending UPLOADS (RAM->VRAM): %d", m.totalPendingUploads);
                    ImGui::Text("Pending UNLOADS (VRAM GC): %d", m.totalPendingUnloads);

                    ImGui::Text("Worker Thread Status: "); ImGui::SameLine();
                    if (m.isWorkerThrottled) ImGui::TextColored(ImVec4(1, 1, 0, 1), "THROTTLED (Backpressure)");
                    else ImGui::TextColored(ImVec4(0, 1, 0, 1), "RUNNING (Generating)");
                }

                // 4. RENDERING & MDI PIPELINE
                if (ImGui::CollapsingHeader("Graphics Pipeline", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Text("Active Vertices: %u", m.activeVertices);
                    ImGui::Text("Active Indices: %u", m.activeIndices);

                    int culledChunks = m.totalCullArraySize - m.currentlyDrawnChunks;
                    float cullEfficiency = m.totalCullArraySize > 0 ? ((float)culledChunks / m.totalCullArraySize) * 100.0f : 0.0f;

                    ImGui::Text("Frustum Culling: %.1f%% efficiency", cullEfficiency);
                    ImGui::Text("Chunks Drawn: %d / %d", m.currentlyDrawnChunks, m.totalCullArraySize);
                }

                // 5. PLAYER PHYSICS & WORLD
                if (ImGui::CollapsingHeader("Player & Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 1.0f, 1.0f), "Absolute Pos:");
                    ImGui::Text("X: %.3f | Y: %.3f | Z: %.3f", m.px, m.py, m.pz);

                    ImGui::TextColored(ImVec4(0.2f, 1.0f, 1.0f, 1.0f), "Chunk Pos (Grid):");
                    ImGui::Text("cX: %lld | cY: %lld | cZ: %lld", (long long)m.gcx, (long long)m.gcy, (long long)m.gcz);

                    float speed = std::sqrt(m.vx*m.vx + m.vy*m.vy + m.vz*m.vz);
                    ImGui::Text("Velocity: %.2f m/s", speed);
                    ImGui::TextDisabled("(Vx: %.1f, Vy: %.1f, Vz: %.1f)", m.vx, m.vy, m.vz);

                    ImGui::Text("Look Vector: [%.2f, %.2f, %.2f]", m.lookX, m.lookY, m.lookZ);

                    ImGui::Text("State: "); ImGui::SameLine();
                    if (m.isFlying) ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "FLYING");
                    else if (m.isGrounded) ImGui::TextColored(ImVec4(0, 1, 0, 1), "GROUNDED");
                    else ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1), "FALLING");
                }

                ImGui::End();
            }
        }
    };
}