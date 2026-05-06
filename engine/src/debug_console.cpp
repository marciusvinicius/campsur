#include "debug_console.h"
#include "engine.h"
#include "graphics_types.h"
#include "input.h"
#include "render.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace criogenio {

namespace {

void popLastUtf8Codepoint(std::string &s) {
  while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80)
    s.pop_back();
  if (!s.empty())
    s.pop_back();
}

std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

bool parseOnOffArg(const std::string &arg, bool &out) {
  std::string s = arg;
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (s == "on" || s == "1" || s == "true") {
    out = true;
    return true;
  }
  if (s == "off" || s == "0" || s == "false") {
    out = false;
    return true;
  }
  return false;
}

} // namespace

std::string DebugConsole::normalizeCommandName(std::string name) {
  for (char &c : name)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return name;
}

void DebugConsole::RegisterCommand(std::string name, CommandHandler handler) {
  commands_[normalizeCommandName(std::move(name))] = std::move(handler);
}

void DebugConsole::AddLogLine(std::string line) {
  logLines_.push_back(std::move(line));
  if (static_cast<int>(logLines_.size()) > kMaxLogLines)
    logLines_.erase(logLines_.begin(),
                    logLines_.begin() + (logLines_.size() - kMaxLogLines));
}

std::vector<std::string> DebugConsole::splitArgs(const std::string &line) {
  std::vector<std::string> out;
  std::string cur;
  for (char ch : line) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(ch);
    }
  }
  if (!cur.empty())
    out.push_back(std::move(cur));
  return out;
}

void DebugConsole::InitializeCommands(Engine &engine) { registerBuiltins(engine); }

void DebugConsole::Close(void *sdlWindow) {
  if (!open_)
    return;
  open_ = false;
  Input::SetSuppressGameplayInput(false);
  if (sdlWindow)
    SDL_StopTextInput(static_cast<SDL_Window *>(sdlWindow));
  inputLine_.clear();
}

void DebugConsole::registerBuiltins(Engine &engine) {
  if (builtinsRegistered_)
    return;
  builtinsRegistered_ = true;

  RegisterCommand("help", [this](Engine &, const std::vector<std::string> &) {
    AddLogLine("commands:");
    std::vector<std::string> names;
    names.reserve(commands_.size());
    for (const auto &kv : commands_)
      names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    for (const std::string &n : names)
      AddLogLine(std::string("  ") + n);
  });

  RegisterCommand("clear", [this](Engine &, const std::vector<std::string> &) {
    logLines_.clear();
  });

  RegisterCommand("netquant", [this](Engine &engine, const std::vector<std::string> &args) {
    ReplicationServer *server = engine.GetReplicationServer();
    if (!server) {
      AddLogLine("netquant: server replication is not active.");
      return;
    }
    if (args.size() <= 1) {
      AddLogLine(server->IsQuantizedTransformPayloadEnabled() ? "netquant: on"
                                                              : "netquant: off");
      AddLogLine("Usage: netquant [on|off]");
      return;
    }
    bool enabled = false;
    if (!parseOnOffArg(args[1], enabled)) {
      AddLogLine("Usage: netquant [on|off]");
      return;
    }
    server->SetQuantizedTransformPayloadEnabled(enabled);
    AddLogLine(enabled ? "netquant: enabled" : "netquant: disabled");
  });

  RegisterCommand("netstats", [this](Engine &engine, const std::vector<std::string> &args) {
    ReplicationServer *server = engine.GetReplicationServer();
    if (!server) {
      AddLogLine("netstats: server replication is not active.");
      return;
    }
    if (args.size() > 1) {
      bool reset = false;
      if (parseOnOffArg(args[1], reset) && reset) {
        server->ResetStats();
        AddLogLine("netstats: counters reset.");
        return;
      }
      if (args[1] == "reset") {
        server->ResetStats();
        AddLogLine("netstats: counters reset.");
        return;
      }
    }
    const auto &st = server->GetStats();
    char b[280];
    std::snprintf(
        b, sizeof b,
        "netstats: built=%llu sent=%llu noop=%llu considered=%llu serialized=%llu "
        "skipped=%llu bytes=%llu quant=%s",
        static_cast<unsigned long long>(st.snapshotsBuilt),
        static_cast<unsigned long long>(st.snapshotsSent),
        static_cast<unsigned long long>(st.snapshotsSkippedNoop),
        static_cast<unsigned long long>(st.entitiesConsidered),
        static_cast<unsigned long long>(st.entitiesSerialized),
        static_cast<unsigned long long>(st.entitiesDeltaSkipped),
        static_cast<unsigned long long>(st.bytesSent),
        server->IsQuantizedTransformPayloadEnabled() ? "on" : "off");
    AddLogLine(b);
    AddLogLine("Usage: netstats [reset]");
  });

  (void)engine;
}

void DebugConsole::ExecuteLine(Engine &engine, const std::string &line) {
  registerBuiltins(engine);
  std::string trimmed = trim(line);
  if (trimmed.empty())
    return;

  AddLogLine(std::string("> ") + trimmed);
  std::vector<std::string> args = splitArgs(trimmed);
  if (args.empty())
    return;

  const std::string cmd = normalizeCommandName(args[0]);
  auto it = commands_.find(cmd);
  if (it == commands_.end()) {
    AddLogLine(std::string("unknown command: ") + cmd);
    return;
  }
  it->second(engine, args);
}

bool DebugConsole::HandleEvent(Engine &engine, const void *sdlEvent,
                               void *sdlWindow) {
  const SDL_Event *e = static_cast<const SDL_Event *>(sdlEvent);
  SDL_Window *win = static_cast<SDL_Window *>(sdlWindow);

  if (e->type == SDL_EVENT_KEY_DOWN && e->key.scancode == SDL_SCANCODE_GRAVE) {
    if (open_) {
      open_ = false;
      Input::SetSuppressGameplayInput(false);
      if (win)
        SDL_StopTextInput(win);
      inputLine_.clear();
    } else {
      open_ = true;
      Input::SetSuppressGameplayInput(true);
      if (win)
        SDL_StartTextInput(win);
      inputLine_.clear();
    }
    return true;
  }

  if (!open_)
    return false;

  switch (e->type) {
  case SDL_EVENT_KEY_DOWN: {
    SDL_Scancode sc = e->key.scancode;
    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
      std::string line = inputLine_;
      inputLine_.clear();
      ExecuteLine(engine, line);
      return true;
    }
    if (sc == SDL_SCANCODE_BACKSPACE) {
      popLastUtf8Codepoint(inputLine_);
      return true;
    }
    if (sc == SDL_SCANCODE_ESCAPE) {
      open_ = false;
      Input::SetSuppressGameplayInput(false);
      if (win)
        SDL_StopTextInput(win);
      inputLine_.clear();
      return true;
    }
    return true;
  }
  case SDL_EVENT_TEXT_INPUT:
    if (e->text.text)
      inputLine_ += e->text.text;
    return true;
  default:
    return false;
  }
}

void DebugConsole::Draw(Renderer &renderer, int viewportW, int viewportH) {
  if (!open_ || viewportW <= 0 || viewportH <= 0)
    return;

  const int panelH = 200;
  const float panelTop = 0.f;
  Color bg{20, 20, 24, 220};
  renderer.DrawRect(0.f, panelTop, static_cast<float>(viewportW), static_cast<float>(panelH),
                    bg);

  const float lineStep = 14.f;
  const float panelBottom = static_cast<float>(panelH);
  float y = panelBottom - 22.f;
  std::string prompt = std::string("> ") + inputLine_ + "_";
  renderer.DrawDebugText(6.f, y, prompt.c_str());
  y -= lineStep;

  const int maxVisible = (panelH - 28) / static_cast<int>(lineStep);
  int start = static_cast<int>(logLines_.size()) - maxVisible;
  if (start < 0)
    start = 0;
  for (int i = static_cast<int>(logLines_.size()) - 1; i >= start && y >= panelTop + 4.f; --i) {
    renderer.DrawDebugText(6.f, y, logLines_[static_cast<size_t>(i)].c_str());
    y -= lineStep;
  }
}

void DebugConsole::shutdown(void *sdlWindow) {
  if (!open_)
    return;
  open_ = false;
  Input::SetSuppressGameplayInput(false);
  if (sdlWindow)
    SDL_StopTextInput(static_cast<SDL_Window *>(sdlWindow));
  inputLine_.clear();
}

} // namespace criogenio
