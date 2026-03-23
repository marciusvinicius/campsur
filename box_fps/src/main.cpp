#include "application_window.h"
#include "box3d/box_fps_scene.h"
#include "box3d/gl_box_renderer.h"
#include "input.h"
#include "keys.h"
#include <box2d/math_functions.h>
#include <chrono>
#include <cstdio>

using namespace criogenio;
using namespace criogenio::box3d;

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  WindowConfig winCfg;
  winCfg.title = "Box FPS — Box2D + Box3D";
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

  BoxFpsScene scene;
  FPCamera camera;
  camera.position = {0.f, 1.65f, 0.f};
  scene.SyncCamera(camera, camera.position.y);

  app.SetRelativeMouseMode(true);

  using clock = std::chrono::steady_clock;
  auto prev = clock::now();
  bool skipMouse = true;

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

    const float moveSpeed = 9.f;
    Vec3 forward = camera.ForwardOnXZPlane();
    Vec3 right = camera.RightOnXZPlane();
    b2Vec2 wish{0.f, 0.f};
    if (Input::IsKeyDown(static_cast<int>(Key::W))) {
      wish.x += forward.x;
      wish.y += forward.z;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::S))) {
      wish.x -= forward.x;
      wish.y -= forward.z;
    }
    // Strafe signs match the view basis: screen-right is (f×up), which is -RightOnXZPlane on the ground.
    if (Input::IsKeyDown(static_cast<int>(Key::D))) {
      wish.x -= right.x;
      wish.y -= right.z;
    }
    if (Input::IsKeyDown(static_cast<int>(Key::A))) {
      wish.x += right.x;
      wish.y += right.z;
    }
    if (b2LengthSquared(wish) > 1e-6f)
      wish = b2MulSV(moveSpeed, b2Normalize(wish));
    else
      wish = {0.f, 0.f};

    scene.Step(dt, wish);
    scene.SyncCamera(camera, camera.position.y);

    int vw = 0, vh = 0;
    app.GetDrawableSize(vw, vh);
    renderer.Resize(vw, vh);
    renderer.ClearFrame(0.07f, 0.08f, 0.11f);
    renderer.SetCamera(camera);
    scene.DrawWorld(renderer);
    renderer.Present(app);

    Input::EndFrame();
  }

  renderer.Shutdown();
  return 0;
}
