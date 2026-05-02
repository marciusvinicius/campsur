#include "subterra_console_commands.h"
#include "subterra_engine.h"

#include "items.h"
#include "map_events.h"
#include "spawn_service.h"
#include "subterra_components.h"
#include "subterra_session.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace subterra {
namespace {

bool parseBoolArg(const std::string &a, bool *out) {
  std::string l = a;
  for (char &c : l)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (l == "on" || l == "true" || l == "1") {
    *out = true;
    return true;
  }
  if (l == "off" || l == "false" || l == "0") {
    *out = false;
    return true;
  }
  return false;
}

bool playerWorldCenter(const SubterraSession &session, float *outCx, float *outCy) {
  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY || !outCx || !outCy)
    return false;
  auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
  if (!tr)
    return false;
  *outCx = tr->x + static_cast<float>(session.playerW) * 0.5f;
  *outCy = tr->y + static_cast<float>(session.playerH) * 0.5f;
  return true;
}

std::string joinArgsFrom(const std::vector<std::string> &args, size_t start) {
  std::string s;
  for (size_t i = start; i < args.size(); ++i) {
    if (i > start)
      s += ' ';
    s += args[i];
  }
  return s;
}

} // namespace

void RegisterSubterraConsoleCommands(SubterraEngine &engine) {
  criogenio::DebugConsole &c = engine.GetDebugConsole();
  SubterraSession &session = engine.GetSession();

  c.RegisterCommand("help", [&c](criogenio::Engine &, const std::vector<std::string> &) {
    c.AddLogLine("help clear where debug [on|off] triggers [on|off] event emit map reload");
    c.AddLogLine("spawn mob [n] | spawn item <id> [count] — at player; give/take/inv");
  });

  c.RegisterCommand("where", [&c, &session](criogenio::Engine &,
                                              const std::vector<std::string> &) {
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
    if (!tr)
      c.AddLogLine("No transform.");
    else {
      char b[160];
      std::snprintf(b, sizeof b, "Player pos: %.1f %.1f (map: %s)", tr->x, tr->y,
                    session.mapPath.c_str());
      c.AddLogLine(b);
    }
  });

  c.RegisterCommand("debug", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &args) {
    if (args.size() <= 1) {
      session.debugOverlay = !session.debugOverlay;
    } else {
      bool v = false;
      if (parseBoolArg(args[1], &v))
        session.debugOverlay = v;
      else {
        c.AddLogLine("Usage: debug [on|off]");
        return;
      }
    }
    c.AddLogLine(session.debugOverlay ? "debug draw: on" : "debug draw: off");
  });

  c.RegisterCommand("triggers", [&c, &session](criogenio::Engine &,
                                                const std::vector<std::string> &args) {
    if (args.size() <= 1) {
      session.debugOverlay = !session.debugOverlay;
    } else {
      bool v = false;
      if (parseBoolArg(args[1], &v))
        session.debugOverlay = v;
    }
    c.AddLogLine(session.debugOverlay ? "trigger overlay: on" : "trigger overlay: off");
  });

  c.RegisterCommand("reload", [&c, &session](criogenio::Engine &,
                                              const std::vector<std::string> &) {
    std::string err;
    if (session.mapPath.empty()) {
      c.AddLogLine("No map path.");
      return;
    }
    if (session.loadMap(session.mapPath, err)) {
      session.placePlayerAtSpawn("");
      c.AddLogLine("Reloaded map.");
    } else
      c.AddLogLine(std::string("Reload failed: ") + err);
  });

  c.RegisterCommand("map", [&c, &session](criogenio::Engine &,
                                             const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: map <path.tmx>");
      return;
    }
    std::string path = joinArgsFrom(args, 1);
    std::string err;
    if (session.loadMap(path, err)) {
      session.placePlayerAtSpawn("");
      c.AddLogLine("Loaded " + path);
    } else
      c.AddLogLine(std::string("Failed: ") + err);
  });

  c.RegisterCommand("event", [&c, &session](criogenio::Engine &,
                                             const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: event <event_id|storage_key>");
      return;
    }
    std::string key = joinArgsFrom(args, 1);
    if (session.fireTiledEventByKey(key))
      c.AddLogLine("Dispatched event " + key);
    else
      c.AddLogLine("No trigger with key " + key);
  });

  c.RegisterCommand("emit", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: emit <trigger_string>");
      return;
    }
    std::string rest = joinArgsFrom(args, 1);
    session.emitManual("", rest, "manual");
    c.AddLogLine("Emitted manual trigger.");
  });

  c.RegisterCommand("spawn", [&c, &session](criogenio::Engine &,
                                              const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: spawn mob [n] | spawn item <id> [count]");
      return;
    }
    std::string kind = args[1];
    for (char &ch : kind)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (kind == "mob") {
      int n = 1;
      if (args.size() >= 3)
        n = std::atoi(args[2].c_str());
      if (n < 1)
        n = 1;
      if (n > 50)
        n = 50;
      float cx = 0, cy = 0;
      if (!playerWorldCenter(session, &cx, &cy)) {
        c.AddLogLine("No player position.");
        return;
      }
      SpawnMobsAround(session, cx, cy, n);
      c.AddLogLine("Spawned " + std::to_string(n) + " mob(s).");
      return;
    }
    if (kind == "item" || kind == "pickup") {
      if (args.size() < 3) {
        c.AddLogLine("Usage: spawn item <item_id> [count]");
        return;
      }
      std::string itemId = args[2];
      int count = 1;
      if (args.size() >= 4)
        count = std::atoi(args[3].c_str());
      if (count < 1)
        count = 1;
      if (count > 999)
        count = 999;
      float cx = 0, cy = 0;
      if (!playerWorldCenter(session, &cx, &cy)) {
        c.AddLogLine("No player position.");
        return;
      }
      SpawnPickupAt(session, cx, cy + static_cast<float>(session.playerH) * 0.35f, itemId,
                    count);
      c.AddLogLine("Dropped pickup: " + itemId + " x" + std::to_string(count));
      return;
    }
    c.AddLogLine("Usage: spawn mob [n] | spawn item <id> [count]");
  });

  c.RegisterCommand("give", [&c, &session](criogenio::Engine &,
                                            const std::vector<std::string> &args) {
    if (args.size() < 2) {
      c.AddLogLine("Usage: give <item_id> [count]");
      return;
    }
    std::string itemId = args[1];
    int cnt = 1;
    if (args.size() >= 3)
      cnt = std::atoi(args[2].c_str());
    if (cnt < 1)
      cnt = 1;
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *inv = session.world->GetComponent<Inventory>(session.player);
    if (!inv) {
      c.AddLogLine("No inventory.");
      return;
    }
    int left = inv->TryAdd(itemId, cnt);
    int added = cnt - left;
    if (added <= 0)
      c.AddLogLine("Inventory full.");
    else if (left == 0)
      c.AddLogLine("Added " + ItemCatalog::DisplayName(itemId) + " x" + std::to_string(added));
    else {
      c.AddLogLine("Added " + std::to_string(added) + "; " + std::to_string(left) +
                   " did not fit.");
    }
  });

  auto takeOrDrop = [&c, &session](criogenio::Engine &, const std::vector<std::string> &args,
                                   const char *opName, bool dropPickup) {
    if (args.size() < 2) {
      c.AddLogLine(std::string("Usage: ") + opName + " <item_id> [count]");
      return;
    }
    std::string itemId = args[1];
    int cnt = 1;
    if (args.size() >= 3)
      cnt = std::atoi(args[2].c_str());
    if (cnt < 1)
      cnt = 1;
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *inv = session.world->GetComponent<Inventory>(session.player);
    if (!inv) {
      c.AddLogLine("No inventory.");
      return;
    }
    int removed = inv->TryRemove(itemId, cnt);
    if (removed <= 0)
      c.AddLogLine("None removed.");
    else {
      c.AddLogLine("Removed " + std::to_string(removed) + ".");
      if (dropPickup) {
        float cx = 0, cy = 0;
        if (playerWorldCenter(session, &cx, &cy))
          SpawnPickupAt(session, cx, cy + static_cast<float>(session.playerH) * 0.4f, itemId,
                        removed);
      }
    }
  };

  c.RegisterCommand("take", [takeOrDrop](criogenio::Engine &e,
                                         const std::vector<std::string> &args) {
    takeOrDrop(e, args, "take", false);
  });

  c.RegisterCommand("drop", [takeOrDrop](criogenio::Engine &e,
                                         const std::vector<std::string> &args) {
    takeOrDrop(e, args, "drop", true);
  });

  auto showInv = [&c, &session](criogenio::Engine &, const std::vector<std::string> &) {
    if (!session.world || session.player == criogenio::ecs::NULL_ENTITY) {
      c.AddLogLine("No player.");
      return;
    }
    auto *inv = session.world->GetComponent<Inventory>(session.player);
    if (!inv)
      c.AddLogLine("No inventory.");
    else
      c.AddLogLine(inv->FormatLine());
  };

  c.RegisterCommand("inv", showInv);
  c.RegisterCommand("inventory", showInv);
}

} // namespace subterra
