#include "utils/window.h"

#include <windows.h>
#include <windowsx.h>
#include <winrt/base.h>

using winrt::check_bool;

namespace utils {

Window::Window(HWND hwnd) : m_hwnd(hwnd) {
  m_windowAliveFlag = std::make_shared<bool>(true);

  RECT windowRect{};
  check_bool(GetWindowRect(m_hwnd, &windowRect));

  m_width = windowRect.right - windowRect.left;
  m_height = windowRect.bottom - windowRect.top;
}

Window::~Window() {
  *m_windowAliveFlag = false;
}

ListenerHandle Window::AddKeyPressListener(uint8_t keyCode, std::function<void()> keyDownCallback,
                                           std::function<void()> keyUpCallback) {
  KeyPressListener listener{};
  listener.KeyCode = keyCode;
  listener.KeyDownCallback = keyDownCallback;
  listener.KeyUpCallback = keyUpCallback;

  return CreateListener(
      listener,
      [=,this](ListenerId id) {
        if (auto it = m_keyPressStates.find(keyCode); it != m_keyPressStates.end()) {
          it->second.Listeners.insert(id);
        } else {
          KeyPressState state{};
          state.Listeners.insert(id);

          m_keyPressStates[keyCode] = state;
        }
      },
      [=,this](ListenerId id) {
        KeyPressState& keyPressState = m_keyPressStates[keyCode];

        keyPressState.Listeners.erase(id);
        if (keyPressState.Listeners.empty())
          m_keyPressStates.erase(listener.KeyCode);
      });
}

ListenerHandle Window::AddMouseMoveListener(std::function<void(int, int)> callback) {
  MouseMoveListener listener{};
  listener.Callback = callback;

  return CreateListener(
      listener,
      [this](ListenerId id) { m_mouseMoveListeners.insert(id); },
      [this](ListenerId id) { m_mouseMoveListeners.erase(id); });
}

void Window::Tick() {
  if (m_savedTime.time_since_epoch() == std::chrono::steady_clock::duration::zero()) {
    m_savedTime = std::chrono::steady_clock::now();
    return;
  }
  auto currentTime = std::chrono::steady_clock::now();

  std::chrono::duration<float, std::milli> durationMs = currentTime - m_savedTime;
  m_savedTime = currentTime;

  m_timeDeltaMs = durationMs.count();
}

std::optional<LRESULT> Window::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_MOUSEMOVE: {
      int mouseX = GET_X_LPARAM(lparam);
      int mouseY = GET_Y_LPARAM(lparam);

      for (ListenerId id : m_mouseMoveListeners) {
        auto listener = std::get<MouseMoveListener>(m_listeners[id]);
        listener.Callback(mouseX, mouseY);
      }
      return 0;
    }
    case WM_KEYDOWN: {
      uint8_t keyCode = static_cast<uint8_t>(wparam);

      if (auto it = m_keyPressStates.find(keyCode); it != m_keyPressStates.end()) {
        KeyPressState& state = it->second;

        if (!state.HasTriggered) {
          state.HasTriggered = true;

          for (ListenerId id : state.Listeners) {
            auto listener = std::get<KeyPressListener>(m_listeners[id]);
            listener.KeyDownCallback();
          }
        }
      }
      return 0;
    }
    case WM_KEYUP: {
      uint8_t keyCode = static_cast<uint8_t>(wparam);

      if (auto it = m_keyPressStates.find(keyCode); it != m_keyPressStates.end()) {
        KeyPressState& state = it->second;

        state.HasTriggered = false;

        for (ListenerId id : state.Listeners) {
          auto listener = std::get<KeyPressListener>(m_listeners[id]);
          if (listener.KeyUpCallback)
            listener.KeyUpCallback();
        }
      }

      return 0;
    }
  }

  return std::nullopt;
}

ListenerHandle Window::CreateListener(ListenerVariant variant,
                                      std::function<void(ListenerId)> addListenerFn,
                                      std::function<void(ListenerId)> removeListenerFn) {
  static ListenerId currentId = 1;

  m_listeners[currentId] = variant;

  ListenerHandle handle(currentId, [=,this](ListenerId id) {
        removeListenerFn(id);
        m_listeners.erase(id);
      },
      m_windowAliveFlag);

  addListenerFn(currentId);

  ++currentId;

  return handle;
}

ListenerHandle::ListenerHandle(ListenerHandle&& other) {
  std::swap(m_id, other.m_id);
  std::swap(m_destroyFn, other.m_destroyFn);
  std::swap(m_windowAliveFlag, other.m_windowAliveFlag);
}

ListenerHandle& ListenerHandle::operator=(ListenerHandle&& other) {
  std::swap(m_id, other.m_id);
  std::swap(m_destroyFn, other.m_destroyFn);
  std::swap(m_windowAliveFlag, other.m_windowAliveFlag);
  return *this;
}

ListenerHandle::ListenerHandle(ListenerId id, std::function<void(ListenerId)> destroyFn,
                               std::shared_ptr<bool>& windowAliveFlag)
  : m_id(id), m_destroyFn(destroyFn),  m_windowAliveFlag(windowAliveFlag) {}

ListenerHandle::~ListenerHandle() {
  if (m_destroyFn && *m_windowAliveFlag)
    m_destroyFn(m_id);
}

} // namespace utils
