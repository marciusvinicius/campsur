#include "editor.h"

int main() {
  int monitor = 0; // Primary monitor
  int width = GetMonitorWidth(monitor);
  int height = GetMonitorHeight(monitor);
  EditorApp app(width, height);
  app.Run();
  return 0;
}
