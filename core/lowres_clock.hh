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

class lowres_clock_impl final {
private:
    //
    // To prevent construction except by friends.
    //

    struct private_token final {};
    static constexpr private_token token{};

public:
    using system_clock = std::chrono::system_clock;
    using steady_clock = std::chrono::steady_clock;

    using system_rep = typename system_clock::rep;
    using steady_rep = typename steady_clock::rep;

    template<class TimePoint>
    inline static TimePoint system_now() {
        return convert_count_to_time_point<TimePoint, system_clock>(_system_count.load(std::memory_order_relaxed));
    }

    template<class TimePoint>
    inline static TimePoint steady_now() {
        return convert_count_to_time_point<TimePoint, steady_clock>(_steady_count.load(std::memory_order_relaxed));
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
        auto const system_count = system_clock::now().time_since_epoch().count();
        auto const steady_count = steady_clock::now().time_since_epoch().count();

        _system_count.store(system_count, std::memory_order_relaxed);
        _steady_count.store(steady_count, std::memory_order_relaxed);
    }

    template<class TimePoint, class BaseClock>
    static TimePoint convert_count_to_time_point(typename BaseClock::rep count) {
        return TimePoint(
                std::chrono::duration_cast<typename TimePoint::duration>(
                        typename BaseClock::duration(count)));
    }
};

///
/// \brief Low-resolution and efficient steady clock.
///
class lowres_clock final {
public:
    using rep = typename lowres_clock_impl::steady_rep;
    using period = std::ratio<1, 1000>;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<lowres_clock, duration>;

    static constexpr bool is_steady = true;

    inline static time_point now() {
        return lowres_clock_impl::steady_now<time_point>();
    }

    lowres_clock() = delete;
};

///
/// \brief Low-resolution and efficient system clock.
///
class lowres_system_clock final {
public:
    using rep = typename lowres_clock_impl::system_rep;
    using period = std::ratio<1, 1000>;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<lowres_system_clock, duration>;

    constexpr static bool is_steady = false;

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
