#include "subterra_input_config.h"
#include "input.h"
#include "subterra_components.h"
#include "subterra_session.h"

#include "json.hpp"
#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <unordered_map>

#include "subterra_json_strip.h"

namespace subterra {
namespace {

float jsonNumF(const nlohmann::json &v, float fallback) {
  if (v.is_number_float())
    return v.get<float>();
  if (v.is_number_integer())
    return static_cast<float>(v.get<int>());
  if (v.is_number_unsigned())
    return static_cast<float>(v.get<unsigned>());
  return fallback;
}

std::string upperTrim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(0, 1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  for (char &c : s)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

int keyboardNameToScancode(const std::string &raw) {
  std::string name = upperTrim(raw);
  if (name.empty() || name == "UNBOUND" || name == "NONE")
    return -1;
  if (name.size() >= 4 && name.compare(0, 4, "KEY_") == 0)
    name = name.substr(4);
  if (name.size() >= 9 && name.compare(0, 9, "KEYBOARD_") == 0)
    name = name.substr(9);
  if (name.size() == 1) {
    const char c = name[0];
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      std::string one(1, c);
      const SDL_Scancode sc = SDL_GetScancodeFromName(one.c_str());
      if (sc != SDL_SCANCODE_UNKNOWN)
        return static_cast<int>(sc);
    }
  }
  static const std::unordered_map<std::string, SDL_Scancode> kSpecial = {
      {"UP", SDL_SCANCODE_UP},         {"DOWN", SDL_SCANCODE_DOWN},
      {"LEFT", SDL_SCANCODE_LEFT},     {"RIGHT", SDL_SCANCODE_RIGHT},
      {"TAB", SDL_SCANCODE_TAB},       {"ESCAPE", SDL_SCANCODE_ESCAPE},
      {"ESC", SDL_SCANCODE_ESCAPE},    {"ENTER", SDL_SCANCODE_RETURN},
      {"RETURN", SDL_SCANCODE_RETURN}, {"SPACE", SDL_SCANCODE_SPACE},
      {"LEFT_SHIFT", SDL_SCANCODE_LSHIFT},
      {"LSHIFT", SDL_SCANCODE_LSHIFT},
      {"RIGHT_SHIFT", SDL_SCANCODE_RSHIFT},
      {"RSHIFT", SDL_SCANCODE_RSHIFT},
      {"LEFT_CTRL", SDL_SCANCODE_LCTRL},
      {"RIGHT_CTRL", SDL_SCANCODE_RCTRL},
      {"R", SDL_SCANCODE_R},
      {"E", SDL_SCANCODE_E},
      {"U", SDL_SCANCODE_U},
      {"W", SDL_SCANCODE_W},
      {"A", SDL_SCANCODE_A},
      {"S", SDL_SCANCODE_S},
      {"D", SDL_SCANCODE_D},
  };
  auto it = kSpecial.find(name);
  if (it != kSpecial.end())
    return static_cast<int>(it->second);
  const SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
  if (sc != SDL_SCANCODE_UNKNOWN)
    return static_cast<int>(sc);
  return -1;
}

int mouseNameToButton(const std::string &raw) {
  std::string name = upperTrim(raw);
  if (name.empty() || name == "UNBOUND" || name == "NONE")
    return -1;
  if (name.size() >= 6 && name.compare(0, 6, "MOUSE_") == 0)
    name = name.substr(6);
  if (name == "LEFT")
    return 0;
  if (name == "RIGHT")
    return 1;
  if (name == "MIDDLE")
    return 2;
  return -1;
}

void fillBindingFromJson(const nlohmann::json *obj, SubterraInputBinding &b) {
  b = {};
  if (!obj || !obj->is_object())
    return;
  const auto &o = *obj;
  if (o.contains("keyboard_primary")) {
    const auto &v = o["keyboard_primary"];
    if (v.is_number_integer())
      b.keyboard_primary = v.get<int>();
    else if (v.is_string())
      b.keyboard_primary = keyboardNameToScancode(v.get<std::string>());
  }
  if (o.contains("keyboard_secondary")) {
    const auto &v = o["keyboard_secondary"];
    if (v.is_number_integer())
      b.keyboard_secondary = v.get<int>();
    else if (v.is_string())
      b.keyboard_secondary = keyboardNameToScancode(v.get<std::string>());
  }
  if (o.contains("mouse_primary")) {
    const auto &v = o["mouse_primary"];
    if (v.is_number_integer())
      b.mouse_primary = v.get<int>();
    else if (v.is_string())
      b.mouse_primary = mouseNameToButton(v.get<std::string>());
  }
}

const SubterraInputBinding *bindingByName(const SubterraInputRuntime &in, const char *action) {
  std::string a = action;
  for (char &c : a)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (a == "respawn")
    return &in.respawn;
  if (a == "inventory_toggle")
    return &in.inventory_toggle;
  if (a == "cancel_ui")
    return &in.cancel_ui;
  if (a == "nav_up")
    return &in.nav_up;
  if (a == "nav_down")
    return &in.nav_down;
  if (a == "use_item")
    return &in.use_item;
  if (a == "interact")
    return &in.interact;
  if (a == "confirm")
    return &in.confirm;
  if (a == "menu_left")
    return &in.menu_left;
  if (a == "menu_right")
    return &in.menu_right;
  if (a == "melee_attack")
    return &in.melee_attack;
  if (a == "spell_cast")
    return &in.spell_cast;
  if (a == "move_left")
    return &in.move_left;
  if (a == "move_right")
    return &in.move_right;
  if (a == "move_up")
    return &in.move_up;
  if (a == "move_down")
    return &in.move_down;
  if (a == "run")
    return &in.run;
  return nullptr;
}

} // namespace

void SubterraInputApplyDefaults(SubterraInputRuntime &out) {
  out = SubterraInputRuntime{};
  out.left_stick_deadzone = 0.22f;
  out.right_stick_deadzone = 0.22f;
  auto kb = [](int a, int b = -1) {
    SubterraInputBinding x;
    x.keyboard_primary = a;
    x.keyboard_secondary = b;
    return x;
  };
  out.move_left = kb(static_cast<int>(SDL_SCANCODE_A));
  out.move_right = kb(static_cast<int>(SDL_SCANCODE_D));
  out.move_up = kb(static_cast<int>(SDL_SCANCODE_W));
  out.move_down = kb(static_cast<int>(SDL_SCANCODE_S));
  out.run = kb(static_cast<int>(SDL_SCANCODE_LSHIFT), static_cast<int>(SDL_SCANCODE_RSHIFT));
  out.interact = kb(static_cast<int>(SDL_SCANCODE_E));
  out.use_item = kb(static_cast<int>(SDL_SCANCODE_U));
  out.inventory_toggle = kb(static_cast<int>(SDL_SCANCODE_TAB));
  out.cancel_ui = kb(static_cast<int>(SDL_SCANCODE_ESCAPE));
  out.confirm = kb(static_cast<int>(SDL_SCANCODE_RETURN));
  out.nav_up = kb(static_cast<int>(SDL_SCANCODE_UP), static_cast<int>(SDL_SCANCODE_W));
  out.nav_down = kb(static_cast<int>(SDL_SCANCODE_DOWN), static_cast<int>(SDL_SCANCODE_S));
  out.menu_left = kb(static_cast<int>(SDL_SCANCODE_A), static_cast<int>(SDL_SCANCODE_LEFT));
  out.menu_right = kb(static_cast<int>(SDL_SCANCODE_D), static_cast<int>(SDL_SCANCODE_RIGHT));
  out.respawn = kb(static_cast<int>(SDL_SCANCODE_R));
  out.melee_attack.mouse_primary = 0;
  out.spell_cast.mouse_primary = 1;
}

bool SubterraTryLoadInputConfigFromPath(const std::string &path, SubterraInputRuntime &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (raw.empty())
    return false;
  nlohmann::json root;
  try {
    root = nlohmann::json::parse(SubterraStripJsonLineComments(raw));
  } catch (...) {
    return false;
  }
  if (!root.is_object())
    return false;
  SubterraInputApplyDefaults(out);
  if (root.contains("gamepad_input_config") && root["gamepad_input_config"].is_object()) {
    const auto &g = root["gamepad_input_config"];
    if (g.contains("left_stick_deadzone"))
      out.left_stick_deadzone = jsonNumF(g["left_stick_deadzone"], out.left_stick_deadzone);
    if (g.contains("right_stick_deadzone"))
      out.right_stick_deadzone = jsonNumF(g["right_stick_deadzone"], out.right_stick_deadzone);
    if (g.contains("gamepad_mouse_speed"))
      out.gamepad_mouse_speed = jsonNumF(g["gamepad_mouse_speed"], out.gamepad_mouse_speed);
  }
  if (root.contains("keyboard_input_config") && root["keyboard_input_config"].is_object()) {
    const auto &k = root["keyboard_input_config"];
    if (k.contains("keyboard_mouse_speed"))
      out.keyboard_mouse_speed =
          jsonNumF(k["keyboard_mouse_speed"], out.keyboard_mouse_speed);
  }
  if (root.contains("bindings") && root["bindings"].is_object()) {
    const auto &b = root["bindings"];
    auto applyBind = [&b](const char *key, SubterraInputBinding &slot) {
      if (b.contains(key))
        fillBindingFromJson(&b[key], slot);
    };
    applyBind("respawn", out.respawn);
    applyBind("inventory_toggle", out.inventory_toggle);
    applyBind("cancel_ui", out.cancel_ui);
    applyBind("nav_up", out.nav_up);
    applyBind("nav_down", out.nav_down);
    applyBind("use_item", out.use_item);
    applyBind("interact", out.interact);
    applyBind("confirm", out.confirm);
    applyBind("menu_left", out.menu_left);
    applyBind("menu_right", out.menu_right);
    applyBind("melee_attack", out.melee_attack);
    applyBind("spell_cast", out.spell_cast);
    applyBind("move_left", out.move_left);
    applyBind("move_right", out.move_right);
    applyBind("move_up", out.move_up);
    applyBind("move_down", out.move_down);
    applyBind("run", out.run);
  }
  return true;
}

void SubterraInputHotReloadTick(SubterraSession &session, float dt) {
  if (session.inputConfigPath.empty())
    return;
  session.inputHotReloadAccum += dt;
  if (session.inputHotReloadAccum < kSubterraInputConfigReloadIntervalSec)
    return;
  session.inputHotReloadAccum = 0.f;
  SubterraInputRuntime tmp;
  if (SubterraTryLoadInputConfigFromPath(session.inputConfigPath, tmp))
    session.input = std::move(tmp);
}

bool SubterraInputBindingDown(const SubterraInputRuntime &, const SubterraInputBinding &b) {
  if (b.keyboard_primary >= 0 && criogenio::Input::IsKeyDown(b.keyboard_primary))
    return true;
  if (b.keyboard_secondary >= 0 && criogenio::Input::IsKeyDown(b.keyboard_secondary))
    return true;
  if (b.mouse_primary >= 0 && criogenio::Input::IsMouseDown(b.mouse_primary))
    return true;
  return false;
}

bool SubterraInputBindingPressed(const SubterraInputRuntime &, const SubterraInputBinding &b) {
  if (b.keyboard_primary >= 0 && criogenio::Input::IsKeyPressed(b.keyboard_primary))
    return true;
  if (b.keyboard_secondary >= 0 && criogenio::Input::IsKeyPressed(b.keyboard_secondary))
    return true;
  if (b.mouse_primary >= 0 && criogenio::Input::IsMouseDown(b.mouse_primary))
    return false;
  return false;
}

bool SubterraInputActionDown(const SubterraSession &session, const char *action) {
  const SubterraInputBinding *b = bindingByName(session.input, action);
  if (!b)
    return false;
  return SubterraInputBindingDown(session.input, *b);
}

bool SubterraInputActionPressed(const SubterraSession &session, const char *action) {
  const SubterraInputBinding *b = bindingByName(session.input, action);
  if (!b)
    return false;
  return SubterraInputBindingPressed(session.input, *b);
}

bool SubterraPollPlayerMovementAxes(SubterraSession &session, criogenio::World &world,
                                    criogenio::ecs::EntityId id, float *outDx, float *outDy) {
  if (!outDx || !outDy)
    return false;
  *outDx = *outDy = 0.f;
  if (!world.HasComponent<PlayerTag>(id))
    return false;
  const SubterraInputRuntime &in = session.input;
  if (SubterraInputBindingDown(in, in.move_right))
    *outDx += 1.f;
  if (SubterraInputBindingDown(in, in.move_left))
    *outDx -= 1.f;
  if (SubterraInputBindingDown(in, in.move_down))
    *outDy += 1.f;
  if (SubterraInputBindingDown(in, in.move_up))
    *outDy -= 1.f;
  return true;
}

} // namespace subterra
