#pragma once

#include "graphics_types.h"

namespace criogenio {

class Renderer;

/**
 * Screen-space immediate-mode UI for in-game overlays (inventory, HUD, prompts).
 * Built on DrawRectScreen / DrawDebugText — independent of ImGui. Games compose
 * panels by calling these helpers from their own draw hooks (e.g. Engine::OnGUI).
 */
class GameUi {
public:
  void beginFrame(Renderer& renderer, int viewportW, int viewportH);
  void endFrame();

  /** Axis-aligned click target; true on left-click press inside bounds. */
  bool screenButton(const Rect& bounds, const char* label, bool highlighted = false);
  void screenPanel(const Rect& bounds, Color fill, Color outline);
  /** SDL debug font (fixed style; color param reserved for future TTF). */
  void text(float x, float y, const char* utf8, Color color = Colors::White);

  /** Full-width bar along the bottom (inside viewport). */
  Rect layoutBottomBar(float height, float margin = 0.f) const;

  int viewportW() const { return vw_; }
  int viewportH() const { return vh_; }

private:
  Renderer* renderer_ = nullptr;
  int vw_ = 0;
  int vh_ = 0;
  bool pointerPressConsumed_ = false;
};

} // namespace criogenio
