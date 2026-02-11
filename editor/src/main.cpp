#include "editor.h"

int main() {
  int width = 1280;
  int height = 720;
  EditorApp app(width, height);
  app.Run();
  return 0;
}
