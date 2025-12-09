#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace criogenio {

enum class EventType {
  None = 0,
  EntityCreated,
  EntityDestroyed,
  KeyPressed,
  CollisionEnter,
  CollisionExit,
  Custom
};

// Base event
struct Event {
  EventType type;
  explicit Event(EventType type) : type(type) {}
  virtual ~Event() = default;
};

using EventCallback = std::function<void(const Event &)>;

class EventBus {
public:
  void Subscribe(EventType type, EventCallback callback);
  void Emit(const Event &event);

private:
  std::unordered_map<EventType, std::vector<EventCallback>> subscribers;
};

struct CollisionEvent : public Event {
  int a; // entity A
  int b; // entity B
  CollisionEvent(EventType type, int a, int b) : Event(type), a(a), b(b) {}
};

} // namespace criogenio
