#include "event.h"

namespace criogenio {

void EventBus::Subscribe(EventType type, EventCallback callback) {
  subscribers[type].push_back(callback);
}

void EventBus::Emit(const Event &event) {
  auto it = subscribers.find(event.type);
  if (it == subscribers.end())
    return;

  for (auto &callback : it->second) {
    callback(event);
  }
}

} // namespace criogenio
