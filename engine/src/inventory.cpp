#include "inventory.h"
#include "item_catalog.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace criogenio {

namespace {

static std::string toLowerCopy(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static bool idEqualsFold(const std::string &a, const std::string &b) {
  return toLowerCopy(a) == toLowerCopy(b);
}

} // namespace

void Inventory::Deserialize(const SerializedComponent &data) {
  Clear();
  for (int i = 0; i < kNumSlots; ++i) {
    auto key = "slot_" + std::to_string(i);
    auto it = data.fields.find(key);
    if (it == data.fields.end())
      continue;
    std::string v = GetString(it->second);
    auto sp = v.find(' ');
    if (sp == std::string::npos || sp == 0)
      continue;
    int c = 0;
    for (size_t j = sp + 1; j < v.size(); ++j) {
      char ch = v[j];
      if (ch < '0' || ch > '9') {
        c = 0;
        break;
      }
      c = c * 10 + (ch - '0');
    }
    if (c > 0) {
      slots[static_cast<size_t>(i)].item_id = v.substr(0, sp);
      slots[static_cast<size_t>(i)].count = c;
    }
  }
}

int Inventory::TryAdd(const std::string &item_id, int count) {
  if (item_id.empty() || count <= 0)
    return count;
  int max_stack = ItemCatalog::MaxStack(item_id);
  int leftover = count;

  for (auto &slot : slots) {
    if (leftover <= 0)
      break;
    if (slot.item_id == item_id && slot.count < max_stack) {
      int room = max_stack - slot.count;
      int add = std::min(room, leftover);
      slot.count += add;
      leftover -= add;
    }
  }
  for (auto &slot : slots) {
    if (leftover <= 0)
      break;
    if (slot.item_id.empty() || slot.count <= 0) {
      int add = std::min(max_stack, leftover);
      slot.item_id = item_id;
      slot.count = add;
      leftover -= add;
    }
  }
  return leftover;
}

int Inventory::TryRemove(const std::string &item_id, int count) {
  if (item_id.empty() || count <= 0)
    return 0;
  int need = count;
  int removed = 0;
  for (auto &slot : slots) {
    if (need <= 0)
      break;
    if (slot.item_id == item_id && slot.count > 0) {
      int take = std::min(slot.count, need);
      slot.count -= take;
      need -= take;
      removed += take;
      if (slot.count <= 0)
        slot.item_id.clear();
    }
  }
  return removed;
}

int Inventory::TryRemoveFolded(const std::string &item_id, int count) {
  if (item_id.empty() || count <= 0)
    return 0;
  int need = count;
  int removed = 0;
  for (auto &slot : slots) {
    if (need <= 0)
      break;
    if (!slot.item_id.empty() && slot.count > 0 && idEqualsFold(slot.item_id, item_id)) {
      int take = std::min(slot.count, need);
      slot.count -= take;
      need -= take;
      removed += take;
      if (slot.count <= 0)
        slot.item_id.clear();
    }
  }
  return removed;
}

void Inventory::Clear() {
  for (auto &slot : slots) {
    slot.item_id.clear();
    slot.count = 0;
  }
}

std::string Inventory::FormatLine() const {
  std::ostringstream os;
  bool first = true;
  for (const auto &slot : slots) {
    if (slot.item_id.empty() || slot.count <= 0)
      continue;
    if (!first)
      os << " | ";
    first = false;
    os << ItemCatalog::DisplayName(slot.item_id) << " x" << slot.count;
  }
  if (first)
    return "(empty)";
  return os.str();
}

} // namespace criogenio
