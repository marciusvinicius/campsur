#include "application_window.h"
#include "box3d/fp_camera.h"
#include "box3d/gl_box_renderer.h"
#include "input.h"
#include "keys.h"
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace criogenio;
using namespace criogenio::box3d;

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  WindowConfig winCfg;
  winCfg.title = "FP plane walk";
  winCfg.width = 1280;
  winCfg.height = 720;
  winCfg.features = static_cast<uint32_t>(WindowFeature::OpenGL) |
                    static_cast<uint32_t>(WindowFeature::Resizable) |
                    static_cast<uint32_t>(WindowFeature::HighPixelDensity);

  ApplicationWindow app(winCfg);
  if (!app.IsValid()) {
    std::fprintf(stderr, "Window: %s\n", app.GetInitError());
    return 1;
  }

  GlBoxRenderer renderer;
  if (!renderer.Init(app)) {
    std::fprintf(stderr, "GlBoxRenderer::Init failed\n");
    return 1;
  }

  renderer.SetDirectionalLight({0.45f, 0.82f, 0.35f}, {0.08f, 0.09f, 0.12f}, 0.92f);

  FPCamera camera;
  constexpr float kEye = 1.65f;
  float playerX = 0.f;
  float playerZ = 2.f;
  camera.position = {playerX, kEye, playerZ};

  app.SetRelativeMouseMode(true);

  using clock = std::chrono::steady_clock;
  auto prev = clock::now();
  bool skipMouse = true;

  constexpr float kBound = 36.f;

  while (!app.QuitRequested()) {
    app.PollEvents();
    if (Input::IsKeyPressed(static_cast<int>(Key::Escape)))
      break;

    auto now = clock::now();
    float dt = std::chrono::duration<float>(now - prev).count();
    prev = now;
    if (dt > 0.1f)
      dt = 0.1f;

    float mx = 0.f, my = 0.f;
    app.GetRelativeMouse(mx, my);
    if (skipMouse) {
      skipMouse = false;
      mx = my = 0.f;
    }
    camera.AddLookDelta(mx, my, 0.0025f);

    const float moveSpeed = 6.5f;
    Vec3 forward = camera.ForwardOnXZPlane();
    Vec3 right = camera.RightOnXZPlane();
    float dx = 0.f, dz = 0.f;
    if (Input::IsKeyDown(static_cast<int>(Key::W))) {
      dx += forward.x;
      dz += forward.z;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::S))) {
      dx -= forward.x;
      dz -= forward.z;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::D))) {
      dx += right.x;
      dz += right.z;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::A))) {
      dx -= right.x;
      dz -= right.z;
    }
    float len = std::sqrt(dx * dx + dz * dz);
    if (len > 1e-4f) {
      dx = (dx / len) * moveSpeed * dt;
      dz = (dz / len) * moveSpeed * dt;
      playerX += dx;
      playerZ += dz;
      if (playerX > kBound)
        playerX = kBound;
      if (playerX < -kBound)
        playerX = -kBound;
      if (playerZ > kBound)
        playerZ = kBound;
      if (playerZ < -kBound)
        playerZ = -kBound;
    }

    camera.position.x = playerX;
    camera.position.y = kEye;
    camera.position.z = playerZ;

    int vw = 0, vh = 0;
    app.GetDrawableSize(vw, vh);
    renderer.Resize(vw, vh);
    renderer.ClearFrame(0.52f, 0.62f, 0.78f);
    renderer.SetCamera(camera);

    renderer.DrawBox({0.f, -0.04f, 0.f}, {40.f, 0.04f, 40.f}, 0.32f, 0.38f, 0.28f);
    renderer.DrawBox({-8.f, 1.1f, -6.f}, {0.35f, 1.1f, 0.35f}, 0.55f, 0.52f, 0.48f);
    renderer.DrawBox({10.f, 0.85f, 4.f}, {0.3f, 0.85f, 0.3f}, 0.48f, 0.45f, 0.5f);
    renderer.DrawBox({-4.f, 0.65f, 12.f}, {1.2f, 0.65f, 0.25f}, 0.42f, 0.4f, 0.38f);

    renderer.Present(app);
    Input::EndFrame();
  }

  renderer.Shutdown();
  return 0;
}
