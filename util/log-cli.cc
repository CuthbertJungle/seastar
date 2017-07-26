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

#include "log-cli.hh"

#include "core/print.hh"
#include "log.hh"

#include <boost/any.hpp>

#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace seastar {

namespace log_cli {

namespace bpo = boost::program_options;

static seastar::log_level log_level_from_string(seastar::sstring const& s) {
    try {
        return boost::lexical_cast<seastar::log_level>(std::string(s));
    } catch (boost::bad_lexical_cast const&) {
        throw std::runtime_error(seastar::sprint("Unknown log level '%s'", s));
    }
}

static void parse_level_assignments(std::string const& v, logger_levels& lv) {
    std::regex const colon(":");

    std::sregex_token_iterator s(v.begin(), v.end(), colon, -1);
    std::sregex_token_iterator const e;
    while (s != e) {
        sstring const p = std::string(*s++);

        auto const i = p.find('=');
        if (i == sstring::npos) {
            throw bpo::invalid_option_value(p);
        }

        auto k = p.substr(0, i);
        auto v = p.substr(i + 1, p.size());
        lv[std::move(k)] = std::move(v);
    };
}

// Custom "validator" that gets called by the internals of Boost.Test. This allows for reading the logger level
// associations into an unordered map and for multiple occurances of the option
// name to appear and be merged.
static void validate(boost::any& out, const std::vector<std::string>& in, logger_levels*, int) {
    if (out.empty()) {
        out = boost::any(logger_levels());
    }

    auto* lv = boost::any_cast<logger_levels>(&out);

    for (const auto& s : in) {
        parse_level_assignments(s, *lv);
    }
}

std::ostream& operator<<(std::ostream& os, logger_levels const& lv) {
    int n = 0;

    for (auto const& e : lv) {
        if (n > 0) {
            os << ":";
        }

        os << e.first << "=" << e.second;
        ++n;
    }

    return os;
}

std::istream& operator>>(std::istream& is, logger_levels& lv) {
    std::string str;
    is >> str;

    parse_level_assignments(str, lv);
    return is;
}

bpo::options_description get_options_description() {
    bpo::options_description opts("Logging options");

    opts.add_options()
            ("default-log-level",
             bpo::value<sstring>()->default_value("info"),
             "Default log level for log messages. Valid values are trace, debug, info, warn, error."
            )
            ("logger-log-level",
             bpo::value<logger_levels>()->default_value({}),
             "Map of logger name to log level. The format is \"NAME0=LEVEL0[:NAME1=LEVEL1:...]\". "
             "Valid logger names can be queried with --help-logging. "
             "Valid values for levels are trace, debug, info, warn, error. "
             "This option can be specified multiple times."
            )
            ("log-to-stdout", bpo::value<bool>()->default_value(true), "Send log output to stdout.")
            ("log-to-syslog", bpo::value<bool>()->default_value(false), "Send log output to syslog.")
            ("help-loggers", bpo::bool_switch(), "Print a list of logger names and exit.");

    return opts;
}

void print_available_loggers(std::ostream& os) {
    auto names = global_log_registry().get_all_logger_names();
    // For quick searching by humans.
    std::sort(names.begin(), names.end());

    os << "Available loggers:\n";

    for (auto&& name : names) {
        os << "    " << name << '\n';
    }
}

void configure(boost::program_options::variables_map const& vars) {
    global_log_registry().set_all_loggers_level(log_level_from_string(vars["default-log-level"].as<sstring>()));

    for (auto const& pair : vars["logger-log-level"].as<logger_levels>()) {
        try {
            global_log_registry().set_logger_level(pair.first, log_level_from_string(pair.second));
        } catch (std::out_of_range const&) {
            throw std::runtime_error(
                        seastar::sprint("Unknown logger '%s'. Use --help-loggers to list available loggers.",
                                        pair.first));
        }
    }

    logger::set_stdout_enabled(vars["log-to-stdout"].as<bool>());
    logger::set_syslog_enabled(vars["log-to-syslog"].as<bool>());
}

}

}
