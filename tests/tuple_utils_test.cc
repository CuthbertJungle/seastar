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

#define BOOST_TEST_MODULE core

#include <util/tuple_utils.hh>

#include <boost/test/included/unit_test.hpp>

using namespace seastar;


BOOST_AUTO_TEST_CASE(generate_from_integers) {
    const auto squares = tuple_generate_from_integers(std::make_integer_sequence<int, 5>(), [](auto&& x) {
        return x * x;
    });

    BOOST_REQUIRE(squares == std::make_tuple(0, 1, 4, 9, 16));
}

BOOST_AUTO_TEST_CASE(generate_from_indices) {
    const auto odds = tuple_generate_from_indices(std::make_index_sequence<5>(), [](auto&& e) {
        return (2 * e) + 1;
    });

    BOOST_REQUIRE(odds == std::make_tuple(1u, 3u, 5u, 7u, 9u));
}
