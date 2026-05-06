#include "delayed_command_queue.h"

#include <algorithm>
#include <utility>

namespace criogenio {

void DelayedCommandQueue::push(float delaySeconds, std::function<void()> fn) {
  if (!fn)
    return;
  float t = delaySeconds;
  if (t < 0.f)
    t = 0.f;
  entries_.push_back({now_ + t, std::move(fn)});
}

void DelayedCommandQueue::clear() {
  entries_.clear();
  now_ = 0.f;
}

void DelayedCommandQueue::update(float dt) {
  now_ += dt;
  if (entries_.empty())
    return;
  std::sort(entries_.begin(), entries_.end(),
            [](const Entry &a, const Entry &b) { return a.fireAt < b.fireAt; });
  size_t i = 0;
  while (i < entries_.size() && entries_[i].fireAt <= now_) {
    if (entries_[i].fn)
      entries_[i].fn();
    ++i;
  }
  if (i > 0)
    entries_.erase(entries_.begin(), entries_.begin() + static_cast<decltype(entries_)::difference_type>(i));
}

} // namespace criogenio
