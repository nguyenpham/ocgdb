/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

/*
 The single random source for everything a tournament decides by chance:
 which openings are used and in what order, how players and games are shuffled,
 Swiss pairing among equal scores, and covered-piece setup. All of them draw
 from this one generator, seeded by `openings.base.seed`, so two runs of the
 same tournament with the same seed get the same openings and pairings.

 Thread safety: draws are serialised on one mutex. These are setup-time and
 book-selection decisions, not hot paths, so the cost is irrelevant.
 */

#ifndef BANKSIA_BASE_RNG_H
#define BANKSIA_BASE_RNG_H

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <random>

namespace banksia {

    /// RAII handle giving exclusive access to the shared generator.
    ///
    ///     {
    ///         auto r = banksia::rngLock();
    ///         std::shuffle(v.begin(), v.end(), r.engine());
    ///     }
    ///
    /// Keep the scope tight: the lock is held for its lifetime.
    class RngLock
    {
    public:
        RngLock();
        std::mt19937& engine();

    private:
        std::unique_lock<std::mutex> lock_;
    };

    RngLock rngLock();

    /// Seed the shared generator. A negative value means "unpredictable", and
    /// the seed actually chosen is recorded so it can be reported and reused.
    void seedRng(int seed);

    /// The seed in force, whether it was given or generated. Report this at the
    /// start of a run so any tournament can be replayed exactly.
    unsigned int currentRngSeed();

    /// Convenience for the common "pick an index" case. Returns 0 when n == 0.
    std::size_t rngIndex(std::size_t n);

    /// Shuffle a container with the shared generator, locking for the duration.
    template<typename It> void rngShuffle(It first, It last)
    {
        auto r = rngLock();
        std::shuffle(first, last, r.engine());
    }

} // namespace banksia

#endif // BANKSIA_BASE_RNG_H
