/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2016 ScyllaDB
 */

#pragma once

#include "timer.hh"

#include <atomic>
#include <chrono>
#include <ratio>

namespace seastar {


/// \cond internal

class lowres_clock_impl final {
private:
    //
    // To prevent construction except by friends.
    //

    struct private_token final {};
    static constexpr private_token token{};

public:
    //
    // To make it easier to do calculations with `std::chrono::milliseconds`, we make the clock's period 1 ms instead
    // of 10 ms (its actual precision).
    //
    using period = std::ratio<1, 1000>;

    using system_base_clock = std::chrono::system_clock;
    using system_rep = typename system_base_clock::rep;
    using system_duration = std::chrono::duration<system_rep, period>;

    using steady_base_clock = std::chrono::steady_clock;
    using steady_rep = typename steady_base_clock::rep;
    using steady_duration = std::chrono::duration<steady_rep, period>;

    template<class TimePoint>
    inline static TimePoint system_now() {
        return TimePoint(system_duration(_system_count.load(std::memory_order_relaxed)));
    }

    template<class TimePoint>
    inline static TimePoint steady_now() {
        return TimePoint(steady_duration(_steady_count.load(std::memory_order_relaxed)));
    }

    inline explicit lowres_clock_impl(private_token) {
        update();

        _timer.set_callback(&lowres_clock_impl::update);
        _timer.arm_periodic(_granularity);
    }

    // For construction.
    friend class smp;

private:
    // The timer updates the counters with this period.
    static constexpr auto _granularity = std::chrono::milliseconds(10);

    //
    // Both counters are updated by CPU0 and read by other CPUs. Make them on their own
    // cache line to avoid false sharing.
    //

    alignas(64) static std::atomic<system_rep> _system_count;
    static std::atomic<steady_rep> _steady_count;

    // High-resolution timer to update the lower-resolution counts.
    timer<> _timer{};

private:
    inline static void update() {
        auto const system_count = now_count<system_duration, system_base_clock>();
        auto const steady_count = now_count<steady_duration, steady_base_clock>();

        _system_count.store(system_count, std::memory_order_relaxed);
        _steady_count.store(steady_count, std::memory_order_relaxed);
    }

    template <class Duration, class BaseClock>
    static typename Duration::rep now_count() {
        return std::chrono::duration_cast<Duration>(BaseClock::now().time_since_epoch()).count();
    };
};

/// \endcond

///
/// \brief Low-resolution and efficient steady clock.
///
/// This is a monotonic clock with a granularity of 10 ms. Time points from this clock do not correspond to system
/// time.
///
/// The primary benefit of this clock is that invoking \c now() is inexpensive compared to
/// \c std::chrono::steady_clock::now().
///
/// \see \c lowres_system_clock for a low-resolution clock which produces time points corresponding to system time.
///
class lowres_clock final {
public:
    using rep = typename lowres_clock_impl::steady_rep;
    using period = typename lowres_clock_impl::period;
    using duration = typename lowres_clock_impl::steady_duration;
    using time_point = std::chrono::time_point<lowres_clock, duration>;

    static constexpr bool is_steady = true;

    ///
    /// \note The result is undefined unless this is called inside a Seastar application.
    ///
    inline static time_point now() {
        return lowres_clock_impl::steady_now<time_point>();
    }

    lowres_clock() = delete;
};

///
/// \brief Low-resolution and efficient system clock.
///
/// This clock has the same granularity as \c lowres_clock, but it is not monotonic and its time points correspond to
/// system time.
///
/// The primary benefit of this clock is that invoking \c now() is inexpensive compared to
/// \c std::chrono::system_clock::now().
///
class lowres_system_clock final {
public:
    using rep = typename lowres_clock_impl::system_rep;
    using period = typename lowres_clock_impl::period;
    using duration = typename lowres_clock_impl::system_duration;
    using time_point = std::chrono::time_point<lowres_system_clock, duration>;

    constexpr static bool is_steady = false;

    ///
    /// \note The result is undefined unless this is called inside a Seastar application.
    ///
    inline static time_point now() {
        return lowres_clock_impl::system_now<time_point>();
    }

    //
    // We assume that `std::chrono::system_clock` time points are relative to the Unix time epoch. This is not
    // technically standardized, but it's a defacto convention.
    //

    inline static std::time_t to_time_t(time_point t) {
        return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
    }

    inline static time_point from_time_t(std::time_t t) {
        return time_point(std::chrono::duration_cast<duration>(std::chrono::seconds(t)));
    }

    lowres_system_clock() = delete;
};

}
