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

#include "test-utils.hh"

#include <core/do_with.hh>
#include <core/lowres_system_clock.hh>
#include <core/sleep.hh>

#include <ctime>

#include <chrono>
#include <iomanip>
#include <iostream>

using namespace seastar;

//
// It's difficult to deterministically verify the sanity of the clock, since it's non-steady. At the very least,
// we can verify that the local time is within a second of the system clock.
//
SEASTAR_TEST_CASE(sanity) {
    auto const system_time = std::chrono::system_clock::now();
    auto const lowres_time = lowres_system_clock::now();

    auto const t1 = std::chrono::system_clock::to_time_t(system_time);
    auto const t2 = lowres_system_clock::to_time_t(lowres_time);

    std::tm *lt1 = std::localtime(&t1);
    std::tm *lt2 = std::localtime(&t2);

    BOOST_TEST(lt1->tm_isdst == lt2->tm_isdst);
    BOOST_TEST(lt1->tm_year == lt2->tm_year);
    BOOST_TEST(lt1->tm_mon == lt2->tm_mon);
    BOOST_TEST(lt1->tm_yday == lt2->tm_yday);
    BOOST_TEST(lt1->tm_mday == lt2->tm_mday);
    BOOST_TEST(lt1->tm_wday == lt2->tm_wday);
    BOOST_TEST(lt1->tm_hour == lt2->tm_hour);
    BOOST_TEST(lt1->tm_min == lt2->tm_min);
    BOOST_TEST(lt1->tm_sec == lt2->tm_sec);

    return make_ready_future<>();
}

//
// Verify that the clock updates its reported time point over time.
//
SEASTAR_TEST_CASE(dynamic) {
    return make_ready_future<>()
            .then([] {
                return do_with(lowres_system_clock::now(), [](auto&& t1) {
                    return ::seastar::sleep(std::chrono::milliseconds(100))
                            .then([&t1] {
                                auto const t2 = lowres_system_clock::now();
                                BOOST_TEST(t1.time_since_epoch().count() != t2.time_since_epoch().count());

                                return make_ready_future<>();
                            });
                });
            });
}