//
// Fájl: Core/MutationPipeline.hpp
// Készítette: NexusForge Engine (Rick & Gem)
// Cél: Multi-Payload AVX2/SSE2 Radix Pipeline Perzisztens Szálakkal.
//
#pragma once

#include <vector>
#include <thread>
#include <barrier>
#include <atomic>
#include <algorithm>
#include <functional>
#include <immintrin.h> 
#include <emmintrin.h> 

#if defined(_MSC_VER)
    #define RESTRICT __restrict
#else
    #define RESTRICT __restrict__
#endif

namespace NF::Core {

    // --- 1. PAYLOAD STRUKTÚRÁK (Szigorú L1 Cache Igazítás) ---

    // 16 bájt - SSE2
    #pragma pack(push, 2)
    struct alignas(16) MoveCommand_Deletion {
        uint16_t localIndex; int16_t chunkX; int16_t chunkY; int16_t chunkZ; uint32_t gridID;
        uint32_t flags;
    };
    #pragma pack(pop)

    // 32 bájt - AVX2 (Fél)
    #pragma pack(push, 4)
    struct alignas(32) MoveCommand_Liquid {
        uint16_t localIndex; int16_t chunkX; int16_t chunkY; int16_t chunkZ; uint32_t gridID;
        uint16_t liquidType; uint16_t pressure; uint32_t flowDirection;
        uint32_t flags; uint64_t _pad; 
    };
    #pragma pack(pop)

    // 64 bájt - AVX2 (Teljes)
    #pragma pack(push, 4)
    struct alignas(64) MoveCommand_Block {
        uint16_t srcLocalIndex; int16_t srcChunkX; int16_t srcChunkY; int16_t srcChunkZ; uint32_t sourceGridID;
        uint16_t tgtLocalIndex; int16_t tgtChunkX; int16_t tgtChunkY; int16_t tgtChunkZ; uint32_t targetGridID;
        uint32_t sourcePaletteIndex; uint32_t targetPaletteIndex; uint32_t force; uint32_t flags;
        union {
            uint8_t rawModData[24];
            struct { float dirX, dirY, dirZ; uint32_t damage; uint64_t entityUUID; } mod_physics;
        };
    };
    #pragma pack(pop)

    struct alignas(16) SortItem {
        uint64_t key; uint32_t index; uint32_t _pad;
    };

    // Univerzális kulcs kinyerő (Compile-Time elágazással)
    template <typename T>
    inline uint64_t ExtractSortKey(const T& cmd) {
        uint64_t key = 0;
        if constexpr (std::is_same_v<T, MoveCommand_Deletion> || std::is_same_v<T, MoveCommand_Liquid>) {
            key |= (static_cast<uint64_t>(cmd.gridID) & 0xFFFFFF) << 40;
            key |= (static_cast<uint64_t>(cmd.chunkX) & 0xFF) << 32;
            key |= (static_cast<uint64_t>(cmd.chunkY) & 0xFF) << 24;
            key |= (static_cast<uint64_t>(cmd.chunkZ) & 0xFF) << 16;
            key |= (static_cast<uint64_t>(cmd.localIndex) & 0xFFFF);
        } else if constexpr (std::is_same_v<T, MoveCommand_Block>) {
            key |= (static_cast<uint64_t>(cmd.targetGridID) & 0xFFFFFF) << 40;
            key |= (static_cast<uint64_t>(cmd.tgtChunkX) & 0xFF) << 32;
            key |= (static_cast<uint64_t>(cmd.tgtChunkY) & 0xFF) << 24;
            key |= (static_cast<uint64_t>(cmd.tgtChunkZ) & 0xFF) << 16;
            key |= (static_cast<uint64_t>(cmd.tgtLocalIndex) & 0xFFFF);
        }
        return key;
    }

    // --- 2. A TITAN PIPELINE (Job System) ---
    class MutationPipeline {
    private:
        enum class JobPhase { SLEEP, SORT_DEL, SORT_LIQ, SORT_BLK, SHUTDOWN };

        uint32_t num_threads;
        std::vector<std::thread> workers;
        std::barrier<std::function<void()>>* sync_point;

        std::atomic<JobPhase> current_phase{JobPhase::SLEEP};
        std::atomic<size_t> next_hist{0}, next_scatter{0}, next_gather{0};

        SortItem* RESTRICT p_in; SortItem* RESTRICT p_out;
        void* RESTRICT p_src; void* RESTRICT p_dst;
        
        size_t job_n;
        size_t NUM_CHUNKS;
        size_t chunk_size;

        std::vector<std::vector<uint32_t>> chunk_hist;

        template <typename PayloadType>
        inline void ExecuteGather(int t_id) {
            if (t_id == 0) next_gather.store(0, std::memory_order_relaxed);
            sync_point->arrive_and_wait();

            const PayloadType* src = static_cast<const PayloadType*>(p_src);
            PayloadType* dst = static_cast<PayloadType*>(p_dst);

            while (true) {
                size_t c = next_gather.fetch_add(1, std::memory_order_relaxed);
                if (c >= NUM_CHUNKS) break;

                size_t start = c * chunk_size;
                size_t end = std::min(start + chunk_size, job_n);

                for (size_t i = start; i < end; ++i) {
                    if (i + 2 < end) {
                        uint32_t nextIdx = p_in[i + 2].index;
                        _mm_prefetch(reinterpret_cast<const char*>(&src[nextIdx]), _MM_HINT_T0);
                    }
                    uint32_t idx = p_in[i].index;

                    // Compile-Time Típusválasztó a hardveres utasításokhoz
                    if constexpr (sizeof(PayloadType) == 64) {
                        const __m256i* s = reinterpret_cast<const __m256i*>(&src[idx]);
                        __m256i* d       = reinterpret_cast<__m256i*>(&dst[i]);
                        _mm256_stream_si256(d, _mm256_loadu_si256(s));
                        _mm256_stream_si256(d + 1, _mm256_loadu_si256(s + 1));
                    } else if constexpr (sizeof(PayloadType) == 32) {
                        const __m256i* s = reinterpret_cast<const __m256i*>(&src[idx]);
                        __m256i* d       = reinterpret_cast<__m256i*>(&dst[i]);
                        _mm256_stream_si256(d, _mm256_loadu_si256(s));
                    } else if constexpr (sizeof(PayloadType) == 16) {
                        const __m128i* s = reinterpret_cast<const __m128i*>(&src[idx]);
                        __m128i* d       = reinterpret_cast<__m128i*>(&dst[i]);
                        _mm_stream_si128(d, _mm_loadu_si128(s));
                    }
                }
            }
        }

        void WorkerLoop(int t_id) {
            while (true) {
                sync_point->arrive_and_wait(); 
                JobPhase phase = current_phase.load(std::memory_order_acquire);
                
                if (phase == JobPhase::SHUTDOWN) break;
                if (phase == JobPhase::SLEEP) continue;

                // 1. Radix Indexelés (Mindhárom típusra ugyanaz)
                for (int pass = 0; pass < 6; ++pass) {
                    int shift = pass * 11;
                    uint32_t mask = (pass == 5) ? 0x1FF : 0x7FF;

                    if (t_id == 0) {
                        next_hist.store(0, std::memory_order_relaxed);
                        next_scatter.store(0, std::memory_order_relaxed);
                    }
                    sync_point->arrive_and_wait();

                    while (true) {
                        size_t c = next_hist.fetch_add(1, std::memory_order_relaxed);
                        if (c >= NUM_CHUNKS) break;
                        size_t start = c * chunk_size; size_t end = std::min(start + chunk_size, job_n);
                        uint32_t h0[2048] = {0}; uint32_t h1[2048] = {0}; uint32_t h2[2048] = {0}; uint32_t h3[2048] = {0};
                        size_t i = start; size_t end_unrolled = start + ((end - start) & ~3);
                        
                        for (; i < end_unrolled; i += 4) {
                            __builtin_prefetch(&p_in[i + 32], 0, 0);
                            h0[(p_in[i+0].key >> shift) & mask]++; h1[(p_in[i+1].key >> shift) & mask]++;
                            h2[(p_in[i+2].key >> shift) & mask]++; h3[(p_in[i+3].key >> shift) & mask]++;
                        }
                        for (; i < end; ++i) h0[(p_in[i].key >> shift) & mask]++;
                        for (int b = 0; b < 2048; ++b) chunk_hist[c][b] = h0[b] + h1[b] + h2[b] + h3[b];
                    }
                    sync_point->arrive_and_wait();

                    if (t_id == 0) {
                        uint32_t offset = 0;
                        for (int bin = 0; bin < 2048; ++bin) {
                            for (size_t c = 0; c < NUM_CHUNKS; ++c) {
                                uint32_t count = chunk_hist[c][bin]; chunk_hist[c][bin] = offset; offset += count;
                            }
                        }
                    }
                    sync_point->arrive_and_wait();

                    while (true) {
                        size_t c = next_scatter.fetch_add(1, std::memory_order_relaxed);
                        if (c >= NUM_CHUNKS) break;
                        size_t start = c * chunk_size; size_t end = std::min(start + chunk_size, job_n);
                        for (size_t i = start; i < end; ++i) {
                            __builtin_prefetch(&p_in[i + 16], 0, 0);
                            p_out[chunk_hist[c][(p_in[i].key >> shift) & mask]++] = p_in[i];
                        }
                    }
                    sync_point->arrive_and_wait();
                    if (t_id == 0) std::swap(p_in, p_out);
                    sync_point->arrive_and_wait();
                }

                // 2. Szétágazó Fizikai Másolás (Gather)
                if (phase == JobPhase::SORT_DEL) ExecuteGather<MoveCommand_Deletion>(t_id);
                else if (phase == JobPhase::SORT_LIQ) ExecuteGather<MoveCommand_Liquid>(t_id);
                else if (phase == JobPhase::SORT_BLK) ExecuteGather<MoveCommand_Block>(t_id);
                
                sync_point->arrive_and_wait();
            }
        }

    public:
        MutationPipeline() {
            uint32_t hw_threads = std::thread::hardware_concurrency();
            num_threads = std::min<uint32_t>(6, std::max<uint32_t>(2, hw_threads / 2)); // Magok fele, de max 6.
            
            chunk_hist.resize(num_threads * 16, std::vector<uint32_t>(2048, 0));
            auto on_completion = []() noexcept {}; 
            sync_point = new std::barrier<std::function<void()>>(num_threads + 1, on_completion); // +1 a Vezérlőnek

            for (uint32_t i = 0; i < num_threads; ++i) {
                workers.emplace_back(&MutationPipeline::WorkerLoop, this, i);
            }
        }

        ~MutationPipeline() {
            current_phase.store(JobPhase::SHUTDOWN, std::memory_order_release);
            sync_point->arrive_and_wait();
            for (auto& t : workers) t.join();
            delete sync_point;
        }

        template <typename T>
        void ExecutePhase(std::vector<SortItem>& items, std::vector<SortItem>& aux, const std::vector<T>& src, std::vector<T>& dst, size_t valid_count) {
            job_n = valid_count;
            if (job_n == 0) return;

            p_in = items.data(); p_out = aux.data();
            p_src = (void*)src.data(); p_dst = (void*)dst.data();
            
            NUM_CHUNKS = std::min<size_t>(num_threads * 16, std::max<size_t>(num_threads, job_n / 40000));
            chunk_size = (job_n + NUM_CHUNKS - 1) / NUM_CHUNKS;

            if constexpr (std::is_same_v<T, MoveCommand_Deletion>) current_phase.store(JobPhase::SORT_DEL, std::order_release);
            else if constexpr (std::is_same_v<T, MoveCommand_Liquid>) current_phase.store(JobPhase::SORT_LIQ, std::order_release);
            else if constexpr (std::is_same_v<T, MoveCommand_Block>) current_phase.store(JobPhase::SORT_BLK, std::order_release);

            sync_point->arrive_and_wait(); // 1. Munkások felébresztése
            sync_point->arrive_and_wait(); // 2. Várakozás amíg végeznek
            
            _mm_sfence(); // Biztonsági zár (Non-Temporal Flush)
            current_phase.store(JobPhase::SLEEP, std::memory_order_release);
        }
    };
}
