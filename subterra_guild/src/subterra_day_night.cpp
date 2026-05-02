#include "subterra_day_night.h"
#include "components.h"
#include "gameplay_tags.h"
#include "graphics_types.h"
#include "render.h"
#include "subterra_item_light.h"
#include "subterra_loadout.h"
#include "subterra_session.h"
#include "world.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace subterra {

namespace {

constexpr double kPi = 3.14159265358979323846;

double phaseSizeClamped(double v) {
  if (v < 0.05)
    return 0.05;
  if (v > 0.45)
    return 0.45;
  return v;
}

float lightMapDarkenMatch(float outdoorFactor, double dayTime) {
  const float dayBri = SubterraDayAmbientBrightness(dayTime);
  const float nightAmt = 1.f - dayBri;
  const float x = (1.f - outdoorFactor) + outdoorFactor * nightAmt;
  return std::clamp(x, 0.f, 1.f);
}

std::string lowerAscii(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

/** Reference `lantern_auto_light` + open-sky dim branch for carried lanterns. */
float carrierLightLitMul(const ItemLightEmission &em, float day_ambient,
                           float outdoor_factor) {
  if (!em.valid)
    return 0.f;
  if (em.lantern_style) {
    if (day_ambient < 0.45f)
      return 1.f;
    if (outdoor_factor >= 0.5f)
      return 0.52f;
    return 0.f;
  }
  if (day_ambient < 0.5f || outdoor_factor < 0.4f)
    return 1.f;
  return 0.f;
}

void drawMapLightStackScreen(criogenio::Renderer &r, const criogenio::Vec2 &screen, float wx,
                            float wy, float radius, unsigned char rr, unsigned char gg,
                            unsigned char bb, float effect_t, const criogenio::Camera2D &cam,
                            float lit_mul, float rad_mul) {
  const float flicker =
      1.f + 0.06f * std::sin(effect_t * 7.3f + wx * 0.01f + wy * 0.007f);
  const float zoom = cam.zoom > 1e-4f ? cam.zoom : 1.f;
  const float base_r =
      std::clamp(radius * zoom * flicker * rad_mul * 1.1f, 10.f, 720.f);
  r.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Add);
  for (int i = 0; i < 5; ++i) {
    const float rr2 = base_r * (1.f - 0.16f * static_cast<float>(i));
    const float a_f =
        std::clamp((28.f - static_cast<float>(i * 4)) * lit_mul, 4.f, 230.f);
    criogenio::Color c{rr, gg, bb, static_cast<unsigned char>(a_f)};
    r.DrawFilledCircleScreen(screen.x, screen.y, rr2, c);
  }
  r.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Alpha);
}

void drawEmitterStackScreen(criogenio::Renderer &r, const criogenio::Vec2 &screen, float wx,
                            float wy, const ItemLightEmission &em, float effect_t,
                            const criogenio::Camera2D &cam, float lit_mul) {
  const float flicker =
      1.f + 0.06f * std::sin(effect_t * 7.3f + wx * 0.01f + wy * 0.007f);
  const float zoom = cam.zoom > 1e-4f ? cam.zoom : 1.f;
  const float base_r =
      std::clamp(em.radius * zoom * flicker * lit_mul * em.intensity * 0.55f, 10.f, 640.f);
  r.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Add);
  for (int i = 0; i < 5; ++i) {
    const float rr = base_r * (1.f - 0.16f * static_cast<float>(i));
    const float a_f =
        std::clamp((28.f - static_cast<float>(i * 4)) * lit_mul * em.intensity, 4.f, 230.f);
    criogenio::Color c{em.r, em.g, em.b,
                         static_cast<unsigned char>(a_f)};
    r.DrawFilledCircleScreen(screen.x, screen.y, rr, c);
  }
  r.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Alpha);
}

} // namespace

static bool localPlayerUnderRoof(const SubterraSession &session) {
  if (!session.world || session.player == criogenio::ecs::NULL_ENTITY)
    return false;
  criogenio::Terrain2D *ter = session.world->GetTerrain();
  auto *tr = session.world->GetComponent<criogenio::Transform>(session.player);
  if (!ter || !tr || !ter->UsesGidMode())
    return false;
  const int stepX = ter->GridStepX();
  const int stepY = ter->GridStepY();
  if (stepX <= 0 || stepY <= 0)
    return false;
  const float pcx = tr->x + static_cast<float>(session.playerW) * 0.5f;
  const float pcy = tr->y + static_cast<float>(session.playerH) * 0.5f;
  const int gx = static_cast<int>(std::floor(pcx / static_cast<float>(stepX)));
  const int gy = static_cast<int>(std::floor(pcy / static_cast<float>(stepY)));

  bool hasRoofProperty = false;
  for (const auto &lm : ter->tmxMeta.layerInfo) {
    if (lm.roof) {
      hasRoofProperty = true;
      break;
    }
  }

  const int n = static_cast<int>(std::min(ter->layers.size(), ter->tmxMeta.layerInfo.size()));
  for (int li = 0; li < n; ++li) {
    const auto &meta = ter->tmxMeta.layerInfo[static_cast<size_t>(li)];
    if (hasRoofProperty) {
      if (!meta.roof)
        continue;
    } else {
      if (meta.mapLayerIndex <= 0)
        continue;
    }
    if (ter->CellHasTile(li, gx, gy))
      return true;
  }
  return false;
}

void SubterraUpdateOutdoorFactor(SubterraSession &session, float dt) {
  if (!session.dayNight.outdoorFromRoof)
    return;
  const float target = localPlayerUnderRoof(session) ? 0.f : 1.f;
  float &o = session.dayNight.outdoorFactor;
  const float k = std::min(dt * 6.f, 1.f);
  o += (target - o) * k;
}

float SubterraDayAmbientBrightness(double dayTime) {
  const double ph = 2.0 * kPi * dayTime - kPi / 2.0;
  return static_cast<float>(0.5 + 0.5 * std::sin(ph));
}

criogenio::Color SubterraDayNightOverlayColor(double dayTime) {
  const float brightness = SubterraDayAmbientBrightness(dayTime);
  const float night = 1.f - brightness;
  const float alpha_f = night * night * 90.f + night * 165.f;
  const unsigned char alpha =
      static_cast<unsigned char>(std::clamp(alpha_f, 0.f, 255.f));
  const float warm =
      std::max(0.f, static_cast<float>(std::sin(4.0 * kPi * dayTime)));
  const unsigned char r = static_cast<unsigned char>(8.f + warm * 188.f);
  const unsigned char g = static_cast<unsigned char>(12.f + warm * 76.f);
  const unsigned char b = static_cast<unsigned char>(58.f - warm * 82.f);
  return {r, g, b, alpha};
}

void SubterraAdvanceDayNight(SubterraDayNight &dn, float dt) {
  dn.effectTime += dt;
  if (dn.dayDuration > 1e-6) {
    dn.dayTime += static_cast<double>(dt) / dn.dayDuration;
    while (dn.dayTime >= 1.0) {
      dn.dayTime -= 1.0;
      dn.worldDay++;
    }
    while (dn.dayTime < 0.0)
      dn.dayTime += 1.0;
  }
}

void SubterraClampDayPhaseSize(SubterraDayNight &dn) {
  dn.dayPhaseSize = phaseSizeClamped(dn.dayPhaseSize);
}

std::string SubterraDayPhaseName(double dayTime, double phaseSize) {
  const double half = phaseSizeClamped(phaseSize) * 0.5;
  int idx = 0;
  if (dayTime < half || dayTime >= 1.0 - half)
    idx = 0;
  else if (dayTime < 0.5 - half)
    idx = 1;
  else if (dayTime < 0.5 + half)
    idx = 2;
  else
    idx = 3;
  switch (idx) {
  case 0:
    return "midnight";
  case 1:
    return "dawn";
  case 2:
    return "noon";
  case 3:
    return "dusk";
  default:
    return "midnight";
  }
}

std::string SubterraDayTimeClockLabel(double dayTime) {
  double t = dayTime;
  while (t < 0.0)
    t += 1.0;
  while (t >= 1.0)
    t -= 1.0;
  const double mins = t * 24.0 * 60.0;
  const int h = static_cast<int>(std::floor(mins / 60.0)) % 24;
  const int m = static_cast<int>(std::floor(std::fmod(mins, 60.0)));
  char buf[8];
  std::snprintf(buf, sizeof buf, "%02d:%02d", h, m);
  return std::string(buf);
}

void SubterraRenderAtmosphere(SubterraSession &session, criogenio::Renderer &r) {
  const float vpW = static_cast<float>(r.GetViewportWidth());
  const float vpH = static_cast<float>(r.GetViewportHeight());
  if (vpW < 1.f || vpH < 1.f)
    return;

  r.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Alpha);

  SubterraDayNight &dn = session.dayNight;
  const float darkMix = lightMapDarkenMatch(dn.outdoorFactor, dn.dayTime);

  if (darkMix > 0.02f) {
    const float dayBri = SubterraDayAmbientBrightness(dn.dayTime);
    const float ambBright = 0.035f + dayBri * 0.10f;
    const unsigned char amb =
        static_cast<unsigned char>(std::clamp(ambBright * 255.f, 0.f, 255.f));
    const unsigned char dr = static_cast<unsigned char>(amb / 2);
    const unsigned char dg = static_cast<unsigned char>(amb / 2);
    const unsigned char db =
        static_cast<unsigned char>(std::min(amb * 1.3f, 255.f));
    auto lerp8 = [](float a, float b, float t) {
      return static_cast<unsigned char>(std::clamp(a + (b - a) * t, 0.f, 255.f));
    };
    const unsigned char cr = lerp8(255.f, static_cast<float>(dr), darkMix);
    const unsigned char cg = lerp8(255.f, static_cast<float>(dg), darkMix);
    const unsigned char cb = lerp8(255.f, static_cast<float>(db), darkMix);
    if (cr < 252 || cg < 252 || cb < 252) {
      r.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Mul);
      r.DrawRectScreen(0.f, 0.f, vpW, vpH, {cr, cg, cb, 255});
      r.SetRenderDrawBlendMode(criogenio::TextureBlendMode::Alpha);
    }
  }

  if (dn.outdoorFactor > 0.005f) {
    criogenio::Color overlay = SubterraDayNightOverlayColor(dn.dayTime);
    const float dayBri = SubterraDayAmbientBrightness(dn.dayTime);
    const float night = 1.f - dayBri;
    const float suppress = dn.outdoorFactor * night;
    const float scale = dn.outdoorFactor * (1.f - suppress);
    overlay.a = static_cast<unsigned char>(
        std::clamp(static_cast<float>(overlay.a) * scale, 0.f, 255.f));
    if (overlay.a > 0)
      r.DrawRectScreen(0.f, 0.f, vpW, vpH, overlay);
  }

  const criogenio::Camera2D *cam = session.world ? session.world->GetActiveCamera() : nullptr;
  if (session.world && cam) {
    criogenio::Terrain2D *ter = session.world->GetTerrain();
    if (ter && ter->UsesGidMode() && !ter->tmxMeta.mapLightSources.empty()) {
      const float day_bri = SubterraDayAmbientBrightness(dn.dayTime);
      const float night_amt = 1.f - day_bri;
      const float lm_mul = 1.f + dn.outdoorFactor * night_amt * 0.22f;
      const float lm_rad = 1.f + dn.outdoorFactor * night_amt * 0.08f;
      for (const auto &ls : ter->tmxMeta.mapLightSources) {
        float lit = lm_mul;
        float radm = lm_rad;
        if (ls.is_window) {
          lit *= std::clamp(0.08f + day_bri * 1.18f, 0.08f, 1.26f);
          radm *= 0.90f + day_bri * 0.35f;
        }
        criogenio::Vec2 sc =
            criogenio::WorldToScreen2D({ls.x, ls.y}, *cam, vpW, vpH);
        drawMapLightStackScreen(r, sc, ls.x, ls.y, ls.radius, ls.r, ls.g, ls.b, dn.effectTime,
                                *cam, lit * 0.45f, radm);
      }
    }
  }

  if (session.world && cam && session.player != criogenio::ecs::NULL_ENTITY) {
    const float day_amb = SubterraDayAmbientBrightness(dn.dayTime);
    auto *ptr = session.world->GetComponent<criogenio::Transform>(session.player);
    if (ptr) {
      const float pcx = ptr->x + static_cast<float>(session.playerW) * 0.5f;
      const float pcy = ptr->y + static_cast<float>(session.playerH) * 0.5f;
      std::unordered_set<std::string> seen_player;

      auto emit_carried = [&](const std::string &raw_id) {
        if (raw_id.empty())
          return;
        const std::string key = lowerAscii(raw_id);
        if (!seen_player.insert(key).second)
          return;
        ItemLightEmission em;
        if (!SubterraItemLight::Lookup(key, em))
          return;
        const float lit = carrierLightLitMul(em, day_amb, dn.outdoorFactor);
        if (lit <= 0.f)
          return;
        criogenio::Vec2 sc = criogenio::WorldToScreen2D({pcx, pcy}, *cam, vpW, vpH);
        drawEmitterStackScreen(r, sc, pcx, pcy, em, dn.effectTime, *cam, lit);
      };

      if (auto *load = session.world->GetComponent<SubterraLoadout>(session.player)) {
        int abi = load->active_action_slot;
        if (abi < 0 || abi >= kActionBarSlots)
          abi = 0;
        emit_carried(load->action_bar[static_cast<size_t>(abi)]);
        for (const std::string &eq : load->equipment)
          emit_carried(eq);
      }

      auto pickups =
          session.world->GetEntitiesWith<criogenio::WorldPickup, criogenio::Transform>();
      for (criogenio::ecs::EntityId pid : pickups) {
        auto *pk = session.world->GetComponent<criogenio::WorldPickup>(pid);
        auto *tr = session.world->GetComponent<criogenio::Transform>(pid);
        if (!pk || !tr || pk->item_id.empty())
          continue;
        ItemLightEmission em;
        if (!SubterraItemLight::Lookup(pk->item_id, em))
          continue;
        const float lit = carrierLightLitMul(em, day_amb, dn.outdoorFactor);
        if (lit <= 0.f)
          continue;
        const float wx = tr->x + pk->width * 0.5f;
        const float wy = tr->y + pk->height * 0.5f;
        criogenio::Vec2 sc = criogenio::WorldToScreen2D({wx, wy}, *cam, vpW, vpH);
        drawEmitterStackScreen(r, sc, wx, wy, em, dn.effectTime, *cam, lit);
      }
    }
  }
}

} // namespace subterra
