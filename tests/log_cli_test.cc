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

#include "util/log.hh"
#include "util/log-cli.hh"

#include <boost/program_options.hpp>
#include <boost/test/included/unit_test.hpp>

#include <initializer_list>
#include <sstream>
#include <string>

namespace bpo = boost::program_options;

using namespace seastar;

void parse_and_configure(std::initializer_list<const char*> args) {
    const auto desc = log_cli::get_options_description();

    std::vector<const char*> raw_args{"log_cli_test"};
    for (const char* arg : args) {
        raw_args.push_back(arg);
    }

    bpo::variables_map vars;
    bpo::store(bpo::parse_command_line(raw_args.size(), raw_args.data(), desc), vars);
    bpo::notify(vars);

    log_cli::configure(vars);
}

BOOST_AUTO_TEST_CASE(logger_log_level) {
    logger logger1("log1");
    logger logger2("log2");
    logger logger3("log3");

    // `--logger-log-level` can be specified multiple times. Each entry can contain one or more associations.
    // The last association takes precedence.
    parse_and_configure({"--logger-log-level", "log1=debug:log3=debug", "--logger-log-level", "log2=warn:log1=error"});

    BOOST_REQUIRE(global_log_registry().get_logger_level("log1") == log_level::error);
    BOOST_REQUIRE(global_log_registry().get_logger_level("log2") == log_level::warn);
    BOOST_REQUIRE(global_log_registry().get_logger_level("log3") == log_level::debug);

    BOOST_REQUIRE_THROW(parse_and_configure({"--logger-log-level", "log1:"}), bpo::invalid_option_value);
}

BOOST_AUTO_TEST_CASE(default_log_level) {
    BOOST_REQUIRE_THROW(parse_and_configure({"--default-log-level", "foo"}), std::runtime_error);

    logger logger1("log1");
    BOOST_REQUIRE(logger1.level() != log_level::debug);

    parse_and_configure({"--default-log-level", "debug"});
    BOOST_REQUIRE(logger1.level() == log_level::debug);
}
