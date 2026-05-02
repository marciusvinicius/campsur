#pragma once

#include "engine.h"

namespace subterra {

struct SubterraSession;

class SubterraEngine : public criogenio::Engine {
public:
  SubterraEngine(SubterraSession &session, int width, int height, const char *title);
  ~SubterraEngine();

  SubterraSession &GetSession() { return *session_; }

protected:
  void RegisterDebugCommands() override;
  void OnFrame(float dt) override;
  void OnAfterWorldRender(criogenio::Renderer &renderer) override;
  void OnGUI() override;
  bool OnPollEvent(const void *sdlEvent) override;

private:
  SubterraSession *session_ = nullptr;
};

} // namespace subterra
