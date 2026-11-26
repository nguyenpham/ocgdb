/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */


#include "rng.h"

#include <chrono>

namespace banksia {

    namespace {
        static auto& g_mutex() // built on first use, not at load time (clazy non-pod-global-static, 2026-11-15)
        {
            static std::mutex v;
            return v;
        }
        static auto& g_engine() // built on first use, not at load time (clazy non-pod-global-static, 2026-11-15)
        {
            static std::mt19937 v{ std::random_device { }() };
            return v;
        }
        unsigned int g_seed = 0;
        bool g_seeded = false;
    }

    RngLock::RngLock() : lock_(g_mutex()) { }

    std::mt19937& RngLock::engine()
    {
        return g_engine();
    }

    RngLock rngLock()
    {
        return RngLock();
    }

    void seedRng(int seed)
    {
        unsigned int chosen;
        if (seed >= 0) {
            chosen = static_cast<unsigned int>(seed);
        } else {
            /// random_device rather than the clock: two tournaments started in the
            /// same second must not get the same "random" openings.
            chosen = std::random_device { }();
        }

        std::lock_guard<std::mutex> lock(g_mutex());
        g_engine().seed(chosen);
        g_seed = chosen;
        g_seeded = true;
    }

    unsigned int currentRngSeed()
    {
        std::lock_guard<std::mutex> lock(g_mutex());
        if (!g_seeded) {
            /// Nobody seeded us; adopt one now so the value reported is the value
            /// in use, and a run is replayable even when no seed was configured.
            g_seed = std::random_device { }();
            g_engine().seed(g_seed);
            g_seeded = true;
        }
        return g_seed;
    }

    std::size_t rngIndex(std::size_t n)
    {
        if (n == 0) {
            return 0;
        }
        auto r = rngLock();
        std::uniform_int_distribution<std::size_t> dist(0, n - 1);
        return dist(r.engine());
    }

} // namespace banksia
