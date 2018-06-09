/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "caf/fwd.hpp"

namespace caf {

/// A set of `config_option` objects that parses CLI arguments into a
/// `config_value::dictionary`.
class config_option_set {
public:
  // -- member types -----------------------------------------------------------

  /// Encodes in which state `parse` stopped.
  enum class parse_state {
    successful,
    option_already_exists,
    not_an_option,
    name_not_declared,
    arg_passed_but_not_declared,
    arg_declared_but_not_passed,
    failed_to_parse_argument,
    type_not_parsebale,
    in_progress,
  };

  /// An iterator over CLI arguments.
  using argument_iterator = std::vector<std::string>::const_iterator;

  // -- constructors, destructors, and assignment operators --------------------

  config_option_set();

  // -- properties -------------------------------------------------------------

  config_option* find(const std::string& long_name) const noexcept;

  config_option* find(char short_name) const noexcept;

  size_t size() const noexcept;

  // -- parsing ----------------------------------------------------------------

  std::pair<parse_state, argument_iterator>
  parse(std::map<std::string, config_value>& config, argument_iterator begin,
        argument_iterator end) const;

private:
  // -- member variables -------------------------------------------------------

  std::vector<std::unique_ptr<config_option>> opts_;
};

} // namespace caf
