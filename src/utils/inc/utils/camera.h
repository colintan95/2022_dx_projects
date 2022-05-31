#pragma once

namespace utils {

class Camera {
public:
  Camera() = default;
  Camera(float x, float y, float z, float yaw, float pitch, float roll);

  float GetX() { return m_x; }
  float GetY() { return m_y; }
  float GetZ() { return m_z; }

  float GetYaw() { return m_yaw; }
  float GetPitch() { return m_pitch; }

  bool IsEnabled() { return m_enabled; }

public:
  void Tick(float timeDelta);

  void UpdateMousePosition(int mouseX, int mouseY);

  void MoveForward(bool flag);
  void MoveBackward(bool flag);
  void MoveLeft(bool flag);
  void MoveRight(bool flag);

  void ToggleEnabled();

  void Enable(bool flag);

private:
  float m_x = 0.f;
  float m_y = 0.f;
  float m_z = 0.f;

  float m_yaw = 0.f;
  float m_pitch = 0.f;
  float m_roll = 0.f;

  float m_lookSpeed = 0.003f; // radians per pixel
  float m_moveSpeed = 1.5f; // distance per second

  bool m_enabled = false;

  int m_currentMouseX = -1;
  int m_currentMouseY = -1;

  bool m_moveForward = false;
  bool m_moveBackward = false;
  bool m_moveLeft = false;
  bool m_moveRight = false;

  bool m_preferForward = true;
  bool m_preferLeft = true;
};

} // namespace utils
