#include <windows.h>

#include <utils/window.h>

#include "app.h"

static utils::Window* s_window = nullptr;

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  if (s_window) {
    if (std::optional<LRESULT> result = s_window->HandleMessage(message, wparam, lparam))
      return *result;
  }

  switch (message) {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR, int cmdShow) {
  WNDCLASSEX windowClass{};
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = hinstance;
  windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  windowClass.lpszClassName = L"Raytracing";
  RegisterClassEx(&windowClass);

  HWND hwnd = CreateWindow(windowClass.lpszClassName, L"Raytracing", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, 1084, 768, nullptr, nullptr, hinstance,
                           nullptr);
  ShowWindow(hwnd, cmdShow);

  s_window = new utils::Window(hwnd);

  App app(s_window);

  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    app.RenderFrame();
  }

  delete s_window;

  return 0;
}
