//
// Fájl: Core/MutationPipeline.hpp
// Készítette: NexusForge Engine (Rick & Gem)
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

    #pragma pack(push, 2)
    struct alignas(16) MoveCommand_Deletion {
        uint16_t localIndex; int16_t chunkX; int16_t chunkY; int16_t chunkZ; uint32_t gridID;
        uint32_t flags;
    };
    #pragma pack(pop)

    #pragma pack(push, 4)
    struct alignas(32) MoveCommand_Liquid {
        uint16_t localIndex; int16_t chunkX; int16_t chunkY; int16_t chunkZ; uint32_t gridID;
        uint16_t liquidType; uint16_t pressure; uint32_t flowDirection;
        uint32_t flags; uint64_t _pad;
    };
    #pragma pack(pop)

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

    class MutationPipeline {
    private:
        enum class JobPhase { SLEEP, SORT_DEL, SORT_LIQ, SORT_BLK, SHUTDOWN };

        uint32_t num_threads;
        std::vector<std::thread> workers;
        std::barrier<std::function<void()>>* worker_sync;

        std::atomic<uint32_t> current_job_id{0};
        std::atomic<uint32_t> workers_finished{0};
        std::atomic<JobPhase> current_phase{JobPhase::SLEEP};
        std::atomic<uint64_t> current_varying_bits{0};

        SortItem* RESTRICT p_in; SortItem* RESTRICT p_out;
        void* RESTRICT p_src; void* RESTRICT p_dst;
        size_t job_n;
        std::vector<std::vector<uint32_t>> chunk_hist; // L2 Cache Histograms!

        template <typename PayloadType>
        inline void ExecuteGatherStatic(int t_id, size_t start, size_t end) {
            const PayloadType* src = static_cast<const PayloadType*>(p_src);
            PayloadType* dst = static_cast<PayloadType*>(p_dst);

            for (size_t i = start; i < end; ++i) {
                if (i + 2 < end) {
                    _mm_prefetch(reinterpret_cast<const char*>(&src[p_in[i + 2].index]), _MM_HINT_T0);
                }
                uint32_t idx = p_in[i].index;

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

        void WorkerLoop(int t_id) {
            uint32_t last_job = 0;
            while (true) {
                uint32_t job = current_job_id.load(std::memory_order_acquire);
                while (job == last_job) {
                    current_job_id.wait(last_job, std::memory_order_relaxed);
                    job = current_job_id.load(std::memory_order_acquire);
                }

                JobPhase phase = current_phase.load(std::memory_order_relaxed);
                if (phase == JobPhase::SHUTDOWN) break;

                size_t chunk_size = job_n / num_threads;
                size_t start = t_id * chunk_size;
                size_t end = (t_id == num_threads - 1) ? job_n : (t_id + 1) * chunk_size;

                uint64_t varying = current_varying_bits.load(std::memory_order_relaxed);

                // ==============================================================
                // AZ ÚJ 16-BITES L2 CACHE RADIX (Csak 4 menet a 6 helyett!)
                // ==============================================================
                for (int pass = 0; pass < 4; ++pass) {
                    int shift = pass * 16;
                    uint32_t mask = 0xFFFF; // 16 bit = 65536 bin (256 KB)

                    // A Varying Bits trükk továbbra is benne marad bónusznak!
                    if (((varying >> shift) & mask) == 0) {
                        continue;
                    }

                    // Histogram takarítás
                    std::fill(chunk_hist[t_id].begin(), chunk_hist[t_id].end(), 0);
                    worker_sync->arrive_and_wait();

                    // 1. L2 Histogram
                    size_t i = start;
                    size_t end_unrolled = start + ((end - start) & ~3);
                    for (; i < end_unrolled; i += 4) {
                        __builtin_prefetch(&p_in[i + 32], 0, 0);
                        chunk_hist[t_id][(p_in[i+0].key >> shift) & mask]++;
                        chunk_hist[t_id][(p_in[i+1].key >> shift) & mask]++;
                        chunk_hist[t_id][(p_in[i+2].key >> shift) & mask]++;
                        chunk_hist[t_id][(p_in[i+3].key >> shift) & mask]++;
                    }
                    for (; i < end; ++i) chunk_hist[t_id][(p_in[i].key >> shift) & mask]++;

                    worker_sync->arrive_and_wait();

                    // 2. Prefix Sum (1 szál csinálja)
                    if (t_id == 0) {
                        uint32_t offset = 0;
                        for (int bin = 0; bin < 65536; ++bin) {
                            for (uint32_t t = 0; t < num_threads; ++t) {
                                uint32_t count = chunk_hist[t][bin];
                                chunk_hist[t][bin] = offset;
                                offset += count;
                            }
                        }
                    }
                    worker_sync->arrive_and_wait();

                    // 3. Scatter
                    for (size_t j = start; j < end; ++j) {
                        p_out[chunk_hist[t_id][(p_in[j].key >> shift) & mask]++] = p_in[j];
                    }
                    worker_sync->arrive_and_wait();
                    if (t_id == 0) std::swap(p_in, p_out);
                    worker_sync->arrive_and_wait();
                }

                // GATHER FÁZIS
                if (phase == JobPhase::SORT_DEL) ExecuteGatherStatic<MoveCommand_Deletion>(t_id, start, end);
                else if (phase == JobPhase::SORT_LIQ) ExecuteGatherStatic<MoveCommand_Liquid>(t_id, start, end);
                else if (phase == JobPhase::SORT_BLK) ExecuteGatherStatic<MoveCommand_Block>(t_id, start, end);

                worker_sync->arrive_and_wait();

                if (t_id == 0) {
                    workers_finished.store(job, std::memory_order_release);
                    workers_finished.notify_all();
                }
                last_job = job;
            }
        }

    public:
        MutationPipeline() {
            uint32_t hw_threads = std::thread::hardware_concurrency();
            num_threads = std::min<uint32_t>(6, std::max<uint32_t>(2, hw_threads / 2));

            // 256 KB L2 Histogram kiosztása szálanként (65536 * 4 bájt = 262144 bájt)
            chunk_hist.resize(num_threads, std::vector<uint32_t>(65536, 0));

            auto on_completion = []() noexcept {};
            worker_sync = new std::barrier<std::function<void()>>(num_threads, on_completion);

            for (uint32_t i = 0; i < num_threads; ++i) workers.emplace_back(&MutationPipeline::WorkerLoop, this, i);
        }

        ~MutationPipeline() {
            current_phase.store(JobPhase::SHUTDOWN, std::memory_order_relaxed);
            current_job_id.fetch_add(1, std::memory_order_release);
            current_job_id.notify_all();
            for (auto& t : workers) t.join();
            delete worker_sync;
        }

        template <typename T>
        void ExecutePhase(std::vector<SortItem>& items, std::vector<SortItem>& aux, const std::vector<T>& src, std::vector<T>& dst, size_t valid_count, uint64_t varying_bits = 0xFFFFFFFFFFFFFFFFULL) {
            job_n = valid_count;
            if (job_n == 0) return;

            p_in = items.data(); p_out = aux.data();
            p_src = (void*)src.data(); p_dst = (void*)dst.data();

            current_varying_bits.store(varying_bits, std::memory_order_relaxed);

            if constexpr (std::is_same_v<T, MoveCommand_Deletion>) current_phase.store(JobPhase::SORT_DEL, std::memory_order_relaxed);
            else if constexpr (std::is_same_v<T, MoveCommand_Liquid>) current_phase.store(JobPhase::SORT_LIQ, std::memory_order_relaxed);
            else if constexpr (std::is_same_v<T, MoveCommand_Block>) current_phase.store(JobPhase::SORT_BLK, std::memory_order_relaxed);

            uint32_t next_job = current_job_id.load() + 1;
            current_job_id.store(next_job, std::memory_order_release);
            current_job_id.notify_all();

            while (workers_finished.load(std::memory_order_acquire) != next_job) {
                workers_finished.wait(workers_finished.load());
            }
            _mm_sfence();
        }
    };
}