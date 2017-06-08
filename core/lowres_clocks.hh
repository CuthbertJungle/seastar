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
 * Copyright (C) 2017 ScyllaDB
 */

#pragma once

#include "timer.hh"

#include <atomic>
#include <chrono>
#include <ratio>

namespace seastar {

template <
        class BaseClock,
        class GranularityDuration, typename GranularityDuration::rep granularity_count>
class clock_impl final {
public:
    using base_clock = BaseClock;
    using rep = typename base_clock::rep;

    template <class TimePoint>
    static TimePoint now() {
        auto const n = _now.load(std::memory_order_relaxed);

        return TimePoint(
                std::chrono::duration_cast<typename TimePoint::duration>(
                        typename BaseClock::duration(n)));
    }

    clock_impl() {
        update();

        _timer.set_callback(&clock_impl<BaseClock, GranularityDuration, granularity_count>::update);
        _timer.arm_periodic(_granularity);
    }

private:
    // The timer updates the counter with this period.
    static constexpr auto _granularity = GranularityDuration(granularity_count);

    //
    // `_now` is updated by CPU0 and read by other CPUs. Make `_now` on its own
    // cache line to avoid false sharing.
    //
    static std::atomic<rep> _now [[gnu::aligned(64)]];

    // High-resolution timer to update the lower-resolution count.
    timer<> _timer{};

private:
    static void update() {
        auto const count = BaseClock::now().time_since_epoch().count();
        _now.store(count, std::memory_order_relaxed);
    }
};

template <
        class BaseClock,
        class GranularityDuration, typename GranularityDuration::rep granularity_count>
std::atomic<typename clock_impl<BaseClock, GranularityDuration, granularity_count>::rep>
clock_impl<BaseClock, GranularityDuration, granularity_count>::_now;

using lowres_clock_impl = clock_impl<
        std::chrono::steady_clock,
        std::chrono::milliseconds, 10>;

using lowres_system_clock_impl = clock_impl<
        std::chrono::system_clock,
        std::chrono::milliseconds, 10>;

//
// Defines a `Impl`-specific type, `type_token`.
//
template <class Impl>
class with_type_token {
private:
    template <class T>
    struct token_impl final {};
public:
    using type_token = token_impl<Impl>;

    constexpr static type_token token{};
};

//
// Both `lowres_clock` and `lowres_system_clock` use private inheritance so that `type_token`'s name is not accessible.
// This is a mechanism to prevent construction of instances except by friends. While declaring a
// private constructor also achieves this goal, perfect-forwarding helpers such as `std::make_unique` do not work in
// that case.
//

///
/// \brief Low-resolution and efficient steady clock.
///
class lowres_clock final : private with_type_token<lowres_clock> {
    using private_token = type_token;
public:
    using rep = typename lowres_clock_impl::rep;
    using period = std::ratio<1, 1000>;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<lowres_clock, duration>;

    static constexpr bool is_steady = true;

    static inline time_point now() {
        return lowres_clock_impl::template now<time_point>();
    }

    explicit lowres_clock(private_token) {}

    friend class smp;

private:
    lowres_clock_impl _impl{};
};

///
/// \brief Low-resolution and efficient system clock.
///
class lowres_system_clock final : private with_type_token<lowres_system_clock> {
    using private_token = type_token;
public:
    using rep = typename lowres_system_clock_impl::rep;
    using period = std::ratio<1, 1000>;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<lowres_system_clock, duration>;

    constexpr static bool is_steady = false;

    static inline time_point now() {
        return lowres_system_clock_impl::template now<time_point>();
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

    explicit lowres_system_clock(private_token) {}

    friend class smp;

private:
    lowres_system_clock_impl _impl{};
};

}
