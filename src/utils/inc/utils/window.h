#pragma once

#include <windows.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace utils {

// Forward declarations
class ListenerHandle;

using ListenerId = uint64_t;

class Window {
public:
  Window(HWND hwnd);
  ~Window();

  ListenerHandle AddKeyPressListener(uint8_t keyCode, std::function<void()> keyDownCallback,
                                     std::function<void()> keyUpCallback = nullptr);

  ListenerHandle AddMouseMoveListener(std::function<void(int, int)> callback);

  HWND GetHwnd() { return m_hwnd; }

  int GetWidth() { return m_width; }
  int GetHeight() { return m_height; }

  float GetTimeDeltaMs() { return m_timeDeltaMs; }

public:
  void Tick();

  std::optional<LRESULT> HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

private:
  friend class ListenerHandle;

  struct KeyPressListener {
    uint8_t KeyCode;
    std::function<void()> KeyDownCallback;
    std::function<void()> KeyUpCallback;
  };

  struct MouseMoveListener {
    std::function<void(int, int)> Callback;
  };

  using ListenerVariant = std::variant<KeyPressListener, MouseMoveListener>;

  ListenerHandle CreateListener(ListenerVariant variant,
                                std::function<void(ListenerId)> addListenerFn,
                                std::function<void(ListenerId)> removeListenerFn);

private:
  HWND m_hwnd;

  int m_width = 0;
  int m_height = 0;

  std::unordered_map<ListenerId, ListenerVariant> m_listeners;

  struct KeyPressState {
    bool HasTriggered = false;
    std::unordered_set<ListenerId> Listeners;
  };
  std::unordered_map<uint8_t, KeyPressState> m_keyPressStates;

  std::unordered_set<ListenerId> m_mouseMoveListeners;

  std::chrono::steady_clock::time_point m_savedTime;
  float m_timeDeltaMs = 0.f;

  std::shared_ptr<bool> m_windowAliveFlag;
};

class ListenerHandle {
public:
  ListenerHandle() = default;
  ~ListenerHandle();

  ListenerHandle(ListenerHandle&&);
  ListenerHandle& operator=(ListenerHandle&&);

  ListenerHandle(const ListenerHandle&) = delete;
  ListenerHandle& operator=(const ListenerHandle&) = delete;

private:
  friend class Window;

  ListenerHandle(ListenerId id, std::function<void(ListenerId)> destroyFn,
                 std::shared_ptr<bool>& windowAliveFlag);

  ListenerId m_id = 0;
  std::function<void(ListenerId)> m_destroyFn;
  std::shared_ptr<bool> m_windowAliveFlag = nullptr;
};

} // namespace utils
