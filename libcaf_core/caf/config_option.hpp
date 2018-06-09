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

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <functional>

#include "caf/atom.hpp"
#include "caf/config_value.hpp"
#include "caf/detail/move_if_not_ptr.hpp"
#include "caf/detail/parser/ec.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/message.hpp"
#include "caf/static_visitor.hpp"
#include "caf/timestamp.hpp"
#include "caf/variant.hpp"

namespace caf {

/// Helper class to generate config readers for different input types.
class config_option {
public:
  config_option(const char* cat, const char* nm, const char* expl);

  virtual ~config_option();

  inline const char* name() const noexcept {
    return name_.c_str();
  }

  inline char short_name() const noexcept {
    return short_name_;
  }

  inline const char* category() const noexcept {
    return category_;
  }

  inline const char* explanation() const noexcept {
    return explanation_;
  }

  /// Returns the full name for this config option as "<category>.<long name>".
  std::string full_name() const;

  /// Tries to parse `arg` into a properly typed `config_value`.
  virtual expected<config_value> parse(const std::string& arg) = 0;

  /// Checks whether `x` holds a legal value for this option.
  virtual error check(const config_value& x) = 0;

  /// Stores `x` in this option unless it is stateless.
  /// @pre `check(x) == none`.
  virtual void store(const config_value& x);

  /// Returns whether this option is a boolean flag.
  virtual bool is_flag() noexcept = 0;

private:
  const char* category_;
  std::string name_;
  const char* explanation_;
  char short_name_;
};

template <class T>
class config_option_impl : public config_option {
public:
  config_option_impl(const char* ctg, const char* nm, const char* xp)
      : config_option(ctg, nm, xp) {
    // nop
  }

  expected<config_value> parse(const std::string& arg) override {
    auto result = config_value::parse(arg);
    if (result) {
      if (!holds_alternative<T>(*result))
        return make_error(detail::parser::ec::type_mismatch);
    }
    return result;
  }

  error check(const config_value& x) override {
    if (holds_alternative<T>(x))
      return none;
    return make_error(detail::parser::ec::type_mismatch);
  }

  bool is_flag() noexcept override {
    return std::is_same<T, bool>::value;
  }
};

template <class T>
class syncing_config_option : public config_option_impl<T> {
public:
  syncing_config_option(T& ref, const char* ctg, const char* nm, const char* xp)
      : config_option_impl<T>(ctg, nm, xp),
        ref_(ref) {
    // nop
  }

  void store(const config_value& x) override {
    ref_ = get<T>(x);
  }

private:
  T& ref_;
};

/// Creates a config option that synchronizes with `storage`.
template <class T>
std::unique_ptr<config_option> make_config_option(const char* category,
                                                  const char* name,
                                                  const char* explanation) {
  auto ptr = new config_option_impl<T>(category, name, explanation);
  return std::unique_ptr<config_option>{ptr};
}

/// Creates a config option that synchronizes with `storage`.
template <class T>
std::unique_ptr<config_option>
make_config_option(T& storage, const char* category, const char* name,
                   const char* explanation) {
  auto ptr = new syncing_config_option<T>(storage, category, name, explanation);
  return std::unique_ptr<config_option>{ptr};
}

} // namespace caf

