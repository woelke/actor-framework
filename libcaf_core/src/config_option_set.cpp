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

#include "caf/config_option_set.hpp"

#include "caf/config_option.hpp"
#include "caf/config_value.hpp"

namespace caf {

config_option_set::config_option_set() {
  // nop
}

auto config_option_set::parse(std::map<std::string, config_value>& config,
                              argument_iterator first,
                              argument_iterator last) const
-> std::pair<parse_state, argument_iterator> {
  // Sanity check.
  if (first == last)
    return {parse_state::successful, last};
  // We loop over the first N-1 values, because we always consider two
  // arguments at once.
  auto i = first;
  auto tail = std::prev(last);
  for (; i != tail;) {
    if (*i== "--")
      return {parse_state::successful, std::next(first)};
    if (i->compare(0, 2, "--")) {
      // Long options use the syntax "--<name>=<value>".
      // Consume only a single argument.
      ++i;
    } else {
      // Consume both elements.
      std::advance(i, 2);
    } else {
      // Stop
      return {i, parse_state::not_an_option};
    }
  }
  return {parse_state::successful, last}
}

config_option*
config_option_set::find(const std::string& long_name) const noexcept {
  for (auto& opt : opts_)
    if (opt->name() == long_name)
      return opt.get();
  return nullptr;
}

config_option* config_option_set::find(char short_name) const noexcept {
  for (auto& opt : opts_)
    if (opt->short_name() == short_name)
      return opt.get();
  return nullptr;
}

size_t config_option_set::size() const noexcept {
  return opts_.size();
}

} // namespace caf
