#include "subterra_camera.h"
#include "subterra_session.h"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace subterra {
namespace {

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

const SubterraCameraShakeEventCfg *pickAttackShakeProfile(const SubterraCameraConfig &c) {
  if (c.shake_on_attack.duration > 0.f && c.shake_on_attack.intensity > 0.f)
    return &c.shake_on_attack;
  if (c.shake_player_attack.duration > 0.f && c.shake_player_attack.intensity > 0.f)
    return &c.shake_player_attack;
  return nullptr;
}

SubterraCameraZoomEventCfg mergeRunStart(const SubterraCameraConfig &c) {
  SubterraCameraZoomEventCfg z = c.zoom_on_run_start;
  if (c.zoom_on_run_alias.speed > 0.f) {
    if (c.zoom_on_run_alias.multiplier > 0.f)
      z.multiplier = c.zoom_on_run_alias.multiplier;
    z.speed = c.zoom_on_run_alias.speed;
  }
  if (z.multiplier <= 0.f)
    z.multiplier = 1.f;
  if (z.speed < 0.f)
    z.speed = 0.f;
  return z;
}

SubterraCameraZoomEventCfg mergeRunStop(const SubterraCameraConfig &c) {
  SubterraCameraZoomEventCfg z = c.zoom_on_run_stop;
  if (z.multiplier <= 0.f)
    z.multiplier = 1.f;
  if (z.speed < 0.f)
    z.speed = 0.f;
  return z;
}

} // namespace

void SubterraCameraApplyConfigDefaults(SubterraCameraBundle &cam) {
  cam.cfg = SubterraCameraConfig{};
  cam.cfg.zoom = 1.15f;
  cam.cfg.zoom_on_run_start = {1.12f, 6.f};
  cam.cfg.zoom_on_run_stop = {1.f, 4.f};
  cam.cfg.shake_on_attack = {4.f, 0.10f, 35.f, 1.f, 0.f, 0.f};
  SubterraCameraResetRuntimeFromConfig(cam);
}

void SubterraCameraResetRuntimeFromConfig(SubterraCameraBundle &cam) {
  cam.shake = {};
  cam.runZoom.current_mul = 1.f;
  cam.runZoom.target_mul = 1.f;
  cam.runZoom.target_speed = 0.f;
  const auto stop = mergeRunStop(cam.cfg);
  cam.runZoom.current_mul = stop.multiplier;
  cam.runZoom.target_mul = stop.multiplier;
  cam.shake_tx = cam.shake_ty = 0.f;
}

void SubterraCameraNotifyTrigger(SubterraSession &session, const std::string &triggerRaw) {
  SubterraCameraBundle &cam = session.camera;
  const std::string t = lowerAscii(triggerRaw);
  if (t == "on_attack") {
    if (!cam.cfg.shake_active)
      return;
    const SubterraCameraShakeEventCfg *p = pickAttackShakeProfile(cam.cfg);
    if (!p || p->duration <= 0.f || p->intensity <= 0.f)
      return;
    cam.shake.active = true;
    cam.shake.elapsed = 0.f;
    cam.shake.intensity = p->intensity;
    cam.shake.duration = p->duration;
    cam.shake.frequency = p->frequency;
    cam.shake.decay = p->decay;
    cam.shake.dir_x = p->dir_x;
    cam.shake.dir_y = p->dir_y;
    return;
  }
  if (!cam.cfg.zoom_active)
    return;
  if (t == "on_run_start") {
    const auto ev = mergeRunStart(cam.cfg);
    cam.runZoom.target_mul = ev.multiplier;
    cam.runZoom.target_speed = ev.speed;
    return;
  }
  if (t == "on_run_stop") {
    const auto ev = mergeRunStop(cam.cfg);
    cam.runZoom.target_mul = ev.multiplier;
    cam.runZoom.target_speed = ev.speed;
  }
}

void SubterraCameraTick(SubterraSession &session, float dt, bool runHeld) {
  (void)runHeld;
  SubterraCameraBundle &cam = session.camera;
  if (cam.cfg.zoom_active) {
    const float tgt = cam.runZoom.target_mul;
    const float cur = cam.runZoom.current_mul;
    float speed = cam.runZoom.target_speed;
    if (speed < 0.f)
      speed = 0.f;
    const float blend = std::clamp(dt * speed, 0.f, 1.f);
    cam.runZoom.current_mul = cur + (tgt - cur) * blend;
  } else {
    cam.runZoom.current_mul = 1.f;
    cam.runZoom.target_mul = 1.f;
  }

  cam.shake_tx = cam.shake_ty = 0.f;
  if (!cam.shake.active)
    return;
  cam.shake.elapsed += dt;
  const float t = cam.shake.elapsed;
  if (t >= cam.shake.duration) {
    cam.shake.active = false;
    return;
  }
  float decay = 1.f;
  if (cam.shake.decay > 0.f && cam.shake.duration > 1e-6f)
    decay = std::clamp(1.f - t / (cam.shake.duration * cam.shake.decay), 0.f, 1.f);
  const float wave =
      std::sin(2.f * 3.14159265f * cam.shake.frequency * t);
  const float amp = cam.shake.intensity * wave * decay;
  float dx = cam.shake.dir_x;
  float dy = cam.shake.dir_y;
  if (dx * dx + dy * dy <= 1e-6f) {
    cam.shake_tx = amp;
    cam.shake_ty = amp * 0.6f * std::cos(2.f * 3.14159265f * cam.shake.frequency * t);
  } else {
    const float mag = std::sqrt(dx * dx + dy * dy);
    if (mag > 1e-6f) {
      dx /= mag;
      dy /= mag;
    }
    cam.shake_tx = dx * amp;
    cam.shake_ty = dy * amp;
  }
}

void SubterraCameraApplyToView(SubterraSession &session, criogenio::Camera2D *cam, float playerCx,
                                 float playerCy) {
  if (!cam)
    return;
  SubterraCameraBundle &cb = session.camera;
  float z = cb.cfg.zoom > 1e-4f ? cb.cfg.zoom : 1.15f;
  if (cb.cfg.zoom_active)
    z *= cb.runZoom.current_mul;
  cam->zoom = z;
  if (cb.cfg.follow_player) {
    cam->target.x = playerCx + cb.cfg.follow_offset.x + cb.shake_tx;
    cam->target.y = playerCy + cb.cfg.follow_offset.y + cb.shake_ty;
  } else {
    cam->target.x = playerCx + cb.shake_tx;
    cam->target.y = playerCy + cb.shake_ty;
  }
}

} // namespace subterra
