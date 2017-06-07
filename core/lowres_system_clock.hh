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

#include "lowres_clock.hh"

#include <cstdint>
#include <ctime>

#include <chrono>

namespace seastar {

///
/// \brief Low-resolution system clock.
///
/// Similar to \c std::chrono::system_clock but with millisecond-level resolution. This clock should be more efficient
/// than \c std::chrono::system_clock when high-precision time points are not necessary.
///
/// Unlike \c lowres_clock, time points from \c lowres_system_clock can be converted to \c std::time_t and back.
///
/// \c lowres_system_clock is a model of \c Clock from the \c std::chrono library.
///
class lowres_system_clock final {
    static_assert(lowres_clock::is_steady);
public:
    using rep = std::int64_t;
    using period = typename lowres_clock::period;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<lowres_system_clock, duration>;

    constexpr static bool is_steady = false;

    ///
    /// \brief Get the current time.
    ///
    /// \note This function must be called in the context of a Seastar application or the result is undefined.
    ///
    inline static time_point now() {
        auto const elapsed = lowres_clock::now().time_since_epoch() - _ref;

        return time_point(
                std::chrono::duration_cast<duration>(
                        _base + std::chrono::duration_cast<typename std::chrono::system_clock::duration>(elapsed)));
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

    // The static clock state is initialized in `smp::configure` via the private constructor.
    // We assume only a single instance is created to initialize the steady-clock reference.
    friend class smp;

private:
    //
    // Reference time, relative to the Unix time epoch at program initialization.
    //
    static typename std::chrono::system_clock::duration const _base;

    //
    // The first captured time point from `lowres_clock` at program initialization. Relative to an opaque epoch.
    //
    static typename lowres_clock::duration _ref;

private:
    lowres_system_clock();
};

}