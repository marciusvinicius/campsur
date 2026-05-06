#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace criogenio {

/** Runs callbacks when `now_` (advanced by update) reaches each entry's `fireAt`. */
class DelayedCommandQueue {
public:
  void push(float delaySeconds, std::function<void()> fn);
  void clear();
  void update(float dt);
  bool empty() const { return entries_.empty(); }

private:
  struct Entry {
    float fireAt = 0.f;
    std::function<void()> fn;
  };
  float now_ = 0.f;
  std::vector<Entry> entries_;
};

} // namespace criogenio
