#include "game_ui.h"
#include "input.h"
#include "render.h"

namespace criogenio {

void GameUi::beginFrame(Renderer& renderer, int viewportW, int viewportH) {
  renderer_ = &renderer;
  vw_ = viewportW;
  vh_ = viewportH;
  pointerPressConsumed_ = false;
}

void GameUi::endFrame() {
  renderer_ = nullptr;
}

void GameUi::screenPanel(const Rect& bounds, Color fill, Color outline) {
  if (!renderer_)
    return;
  renderer_->DrawRectScreen(bounds.x, bounds.y, bounds.width, bounds.height, fill);
  renderer_->DrawRectOutline(bounds.x, bounds.y, bounds.width, bounds.height, outline);
}

bool GameUi::screenButton(const Rect& bounds, const char* label, bool highlighted) {
  if (!renderer_)
    return false;
  Vec2 m = Input::GetMousePosition();
  const bool inside = PointInRect(m, bounds);
  Color base = highlighted ? Color{70, 90, 130, 240} : Color{55, 55, 70, 220};
  Color fill = inside ? Color{100, 110, 140, 240} : base;
  renderer_->DrawRectScreen(bounds.x, bounds.y, bounds.width, bounds.height, fill);
  renderer_->DrawRectOutline(bounds.x, bounds.y, bounds.width, bounds.height,
                             Colors::White);
  if (label && label[0] != '\0') {
    const float tx = bounds.x + 4.f;
    const float ty = bounds.y + (bounds.height - 10.f) * 0.5f;
    renderer_->DrawDebugText(tx, ty, label);
  }
  if (pointerPressConsumed_)
    return false;
  if (inside && Input::IsMousePressed(0)) {
    pointerPressConsumed_ = true;
    return true;
  }
  return false;
}

void GameUi::text(float x, float y, const char* utf8, Color color) {
  if (!renderer_ || !utf8)
    return;
  (void)color;
  renderer_->DrawDebugText(x, y, utf8);
}

Rect GameUi::layoutBottomBar(float height, float margin) const {
  const float m = margin > 0.f ? margin : 0.f;
  const float h = height > 1.f ? height : 1.f;
  return Rect{m, static_cast<float>(vh_) - h - m, static_cast<float>(vw_) - 2.f * m, h};
}

} // namespace criogenio
