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

#include <atomic>
#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>

#include "caf/config.hpp"

namespace caf {
namespace detail {

/// A thread-safe doubly-linked queue with a single mutex for locking.
template <class T>
class thread_safe_queue {
public:
  // -- member types -----------------------------------------------------------

  struct node;

  using value_type = T;

  using size_type = size_t;

  using difference_type = ptrdiff_t;

  using reference = value_type&;

  using const_reference = const value_type&;

  using pointer = value_type*;

  using const_pointer = const value_type*;

  using node_pointer = node*;

  using atomic_node_pointer = std::atomic<node_pointer>;

  using lock_guard = std::unique_lock<std::mutex>;

  struct node {
    // -- constants ------------------------------------------------------------

    static constexpr size_type payload_size =
      sizeof(pointer) + (sizeof(atomic_node_pointer) * 2);

    static constexpr size_type pad_size = CAF_CACHE_LINE_SIZE - payload_size;

    static_assert(pad_size > 0, "invalid padding size calculated");

    // -- member variables -----------------------------------------------------

    // Pointer to our data.
    pointer value;

    // Pointer to the next node in the list.
    atomic_node_pointer next;

    // Pointer to the previous node in the list.
    atomic_node_pointer prev;

    // Makes sure no two nodes are mapped to the same cache line.
    char pad[pad_size];

    // -- constructors, destructors, and assignment operators ------------------

    explicit node(pointer val) : value(val), next(nullptr), prev(nullptr) {
      // nop
    }
  };

  using unique_node_ptr = std::unique_ptr<node>;

  thread_safe_queue() {
    auto ptr = new node(nullptr);
    head_ = ptr;
    tail_ = ptr;
  }

  ~thread_safe_queue() {
    auto ptr = head_.load();
    while (ptr) {
      unique_node_ptr tmp{ptr};
      ptr = tmp->next.load();
    }
  }

  /// Appends `value` to the queue in O(1).
  template <bool NotifyConsumer = true>
  void append(pointer value) {
    CAF_ASSERT(value != nullptr);
    auto* tmp = new node(value);
    lock_guard guard{lock_};
    // Connect the last element to the new element.
    auto tail = tail_.load();
    tail->next = tmp;
    tmp->prev = tail;
    // Appending to an empty queue potentially wakes up consumers.
    if (NotifyConsumer && tail == head_.load())
      cv_.notify_one();
    // Advance tail.
    tail_ = tmp;
  }

  /// Appends `value` to the queue in O(1) but never notifies a sleeping
  /// consumer.
  void internal_append(pointer value) {
    append<false>(value);
  }

  /// Prepends `value` to the queue in O(1).
  template <bool NotifyConsumer = true>
  void prepend(pointer value) {
    CAF_ASSERT(value != nullptr);
    auto* tmp = new node(value);
    // acquire both locks since we might touch last_ too
    lock_guard guard{lock_};
    auto head = head_.load();
    CAF_ASSERT(head != nullptr);
    auto next = head->next.load();
    // Our head always points to a dummy with no value,
    // hence we put the new element second.
    if (next != nullptr) {
      CAF_ASSERT(head != tail_);
      // Connect head to the new first element.
      tmp->prev = head;
      head->next = tmp;
      // Connect new first element with the next one.
      tmp->next = next;
      next->prev = tmp;
    } else {
      // Queue is empty.
      CAF_ASSERT(head == tail_);
      // Connect head to the new first element.
      tmp->prev = head;
      head->next = tmp;
      // Let tail point to the first (and last) element.
      tail_ = tmp;
      // Wakeup potentially sleeping consumers.
      // TODO: replace with `if constexpr` when switching to C++17.
      if (NotifyConsumer)
        cv_.notify_one();
    }
  }

  /// Prepends `value` to the queue in O(1) but never notifies a sleeping
  /// consumer.
  void internal_prepend(pointer value) {
    prepend<false>(value);
  }

  /// Tries to remove the first element and returns it on success, returns
  /// `nullptr` otherwise.
  pointer try_take_head() {
    return try_take_head_impl([](lock_guard&, atomic_node_pointer& ptr) {
      return ptr == nullptr;
    });
  }

  /// Tries to remove the first element before `rel_time` passes and returns it
  /// on success, returns `nullptr` otherwise.
  template <class Duration>
  pointer try_take_head(Duration rel_time) {
    return try_take_head_impl([=](lock_guard& guard, atomic_node_pointer& ptr) {
      return cv_.wait_for(guard, rel_time, [&] { return ptr == nullptr; });
    });
  }

  /// Tries to removes the last element and returns it on success, returns
  /// `nullptr` otherwise.
  pointer try_take_tail() {
    pointer result = nullptr;
    unique_node_ptr tail;
    { // lifetime scope of guards
      lock_guard guard{lock_};
      CAF_ASSERT(head_ != nullptr);
      tail.reset(tail_.load());
      if (tail.get() == head_.load()) {
        tail.release();
        return nullptr;
      }
      result = tail->value;
      auto prev = tail->prev.load();
      prev->next = nullptr;
      tail_ = prev;
    }
    return result;
  }

  // Returns whether the queue is empty.
  bool empty() const {
    // Atomically compares first and last pointer without locks.
    return head_.load() == tail_.load();
  }

private:
  template <class Predicate>
  pointer try_take_head_impl(Predicate pred) {
    unique_node_ptr head;
    pointer result = nullptr;
    { // lifetime scope of guard
      lock_guard guard{lock_};
      head.reset(head_.load());
      if (pred(guard, head->next)) {
        // Queue is empty.
        head.release();
        return nullptr;
      }
      auto next = head->next.load();
      CAF_ASSERT(next != nullptr);
      // Take value out of the node.
      result = next->value;
      next->value = nullptr;
      // Advance head.
      next->prev = nullptr;
      head_ = next;
    }
    return result;
  }

  // Our dummy head node, located on the first "cache page".
  std::atomic<node*> head_;
  char pad1_[CAF_CACHE_LINE_SIZE - sizeof(node*)];

  // Our dummy tail node, located on the second "cache page".
  std::atomic<node*> tail_;
  char pad2_[CAF_CACHE_LINE_SIZE - sizeof(node*)];

  // Our lock, located after our dummy nodes.
  std::mutex lock_;
  std::condition_variable cv_;
};

} // namespace detail
} // namespace caf

