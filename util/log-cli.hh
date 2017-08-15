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

#include "core/sstring.hh"

#include <boost/program_options.hpp>

#include <iostream>
#include <unordered_map>

/// \addtogroup logging
/// @{
namespace seastar {

///
/// \brief Configure application logging at run-time with program options.
///
namespace log_cli {

/// \cond internal

// We need a distinct type (not an alias) for overload resolution in the implementation.
class logger_levels final : private std::unordered_map<sstring, sstring> {
private:
    using base = std::unordered_map<sstring, sstring>;

public:
    using base::begin;
    using base::end;
    using base::operator[];
};

std::istream& operator>>(std::istream& is, logger_levels&);
std::ostream& operator<<(std::ostream& os, const logger_levels&);

/// \endcond

///
/// \brief Options for controlling logging at run-time.
///
boost::program_options::options_description get_options_description();

///
/// \brief Print a human-friendly list of the available loggers.
///
void print_available_loggers(std::ostream& os);

///
/// \brief Configure the logging system with the result of parsing the program options.
//
void configure(const boost::program_options::variables_map&);

}

}

/// @}
