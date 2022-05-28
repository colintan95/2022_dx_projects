#include <windows.h>

#include "app.h"

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
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

  HWND hwnd = CreateWindow(windowClass.lpszClassName, L"Raytracing", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                           CW_USEDEFAULT, 1084, 768, nullptr, nullptr, hinstance, nullptr);
  ShowWindow(hwnd, cmdShow);

  App app(hwnd);

  MSG msg = {};
  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    app.RenderFrame();
  }

  return 0;
}
