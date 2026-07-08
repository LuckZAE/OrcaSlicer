#pragma once
#include <deque>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include "snap_telemetry/types.hpp"
namespace snap {
class EventQueue {
public:
  explicit EventQueue(size_t cap): cap_(cap) {}
  void push(Event e);
  std::vector<Event> drain(size_t max);
  size_t size() const;
  uint64_t dropped() const { return dropped_.load(); }
  // Set a callback invoked (under no lock) after each push. BatchUploader
  // uses this to wake its cv so batch-size-based flushing works.
  void set_notify(std::function<void()> fn);
private:
  mutable std::mutex m_;
  std::deque<Event> q_;
  size_t cap_;
  std::atomic<uint64_t> dropped_{0};
  std::function<void()> notify_;
};
}
