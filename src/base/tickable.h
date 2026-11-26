/*
 This file is part of Banksia, a chess and xiangqi tournament manager, and is
 used by Open Chess Game Database Standard (OCGDB).

 Copyright (c) 2019-2026 Nguyen Hong Pham (github@nguyenpham)

 In OCGDB this file is distributed under the MIT License (MIT), as the rest of
 OCGDB (see the file LICENSE, or http://opensource.org/licenses/MIT). The
 copyright holder relicensed the copy taken from Banksia (GPL-3) for OCGDB.
 */

#ifndef BANKSIA_BASE_TICKABLE_H
#define BANKSIA_BASE_TICKABLE_H

#include <atomic>
#include <chrono>
#include <mutex>

namespace banksia {

    /// Real-time timeouts.
    ///
    /// These measure the clock, so a timeout is the duration it claims to be
    /// whatever the tick rate is and however many ticks are dropped.
    using TickClock = std::chrono::steady_clock;

    /// How long since the last reset().
    class Stopwatch
    {
    public:
        Stopwatch() : from_(TickClock::now()) { }

        void reset() { from_ = TickClock::now(); }

        std::chrono::milliseconds elapsed() const
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(TickClock::now() - from_);
        }

        bool longerThan(std::chrono::milliseconds d) const { return elapsed() > d; }

    private:
        TickClock::time_point from_;
    };

    /// A one-shot countdown, disarmed until arm() is called.
    class Deadline
    {
    public:
        void arm(std::chrono::milliseconds d)
        {
            due_ = TickClock::now() + d;
            armed_ = true;
        }

        /// Arm for at least `d`, keeping a longer deadline already in force.
        void armAtLeast(std::chrono::milliseconds d)
        {
            auto due = TickClock::now() + d;
            if (!armed_ || due > due_) {
                due_ = due;
                armed_ = true;
            }
        }

        void disarm() { armed_ = false; }
        bool armed() const { return armed_; }
        bool expired() const { return armed_ && TickClock::now() >= due_; }

    private:
        TickClock::time_point due_;
        bool armed_ = false;
    };


    /// Something the application calls periodically to do housekeeping.
    ///
    /// tick() never blocks: if a previous tick is still running -- on this
    /// thread or another -- it returns immediately rather than queueing. Ticks
    /// are housekeeping, so a skipped one costs nothing and a backlog of them
    /// on a timer thread would cost a great deal.
    class Tickable
    {
    public:
        virtual ~Tickable() { }

        virtual void tick()
        {
            tickCount++;
            if (tickMutex.try_lock()) {
                tickWork();
                tickMutex.unlock();
            }
        }

        virtual void tickWork() = 0;

        /// Counts attempted ticks, including the ones that found the lock held
        /// and did no work. Atomic because tick() is called from timer threads
        /// while the owner reads the count.
        std::atomic<int> tickCount { 0 };

    private:
        std::mutex tickMutex;
    };

} // namespace banksia

#endif /* BANKSIA_BASE_TICKABLE_H */
