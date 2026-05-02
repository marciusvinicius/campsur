#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace criogenio {

class Engine;
class Renderer;

/** In-game debug console: ` key toggles; games extend via Engine::RegisterDebugCommands(). */
class DebugConsole {
public:
  using CommandHandler =
      std::function<void(Engine &engine, const std::vector<std::string> &args)>;

  /** Registers built-in commands (help, clear). Safe to call once from Engine::Run(). */
  void InitializeCommands(Engine &engine);

  void RegisterCommand(std::string name, CommandHandler handler);
  void AddLogLine(std::string line);

  bool IsOpen() const { return open_; }

  /** SDL_Event from SDL3; sdlWindow is SDL_Window* for text input. Returns true if consumed. */
  bool HandleEvent(Engine &engine, const void *sdlEvent, void *sdlWindow);

  void Draw(Renderer &renderer, int viewportW, int viewportH);

  void ExecuteLine(Engine &engine, const std::string &line);

  void shutdown(void *sdlWindow);

private:
  void registerBuiltins(Engine &engine);
  static std::vector<std::string> splitArgs(const std::string &line);
  static std::string normalizeCommandName(std::string name);

  std::unordered_map<std::string, CommandHandler> commands_;
  std::vector<std::string> logLines_;
  std::string inputLine_;
  bool open_ = false;
  bool builtinsRegistered_ = false;
  static constexpr int kMaxLogLines = 200;
};

} // namespace criogenio
