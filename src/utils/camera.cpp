#include "utils/camera.h"

#include <DirectXMath.h>

#include <cmath>

using namespace DirectX;

namespace utils {

Camera::Camera(float x, float y, float z, float yaw, float pitch, float roll)
  : m_x(x), m_y(y), m_z(z), m_yaw(yaw), m_pitch(pitch), m_roll(roll) {}

void Camera::Tick(float timeDeltaMs) {
  if (!m_enabled)
    return;

  float moveDist = (m_moveSpeed / 1000.f) * timeDeltaMs;
  float moveDiagDist = moveDist / sqrt(2.f);

  XMVECTOR moveVec = XMVectorSet(0.f, 0.f, 0.f, 0.f);

  if (m_moveForward && (!m_moveBackward || m_preferForward)) {
    if (m_moveLeft && (!m_moveRight || m_preferLeft)) {
      moveVec = XMVectorSet(-moveDiagDist, 0.f, moveDiagDist, 0.f);
    } else if (m_moveRight) {
      moveVec = XMVectorSet(moveDiagDist, 0.f, moveDiagDist, 0.f);
    } else {
      moveVec = XMVectorSet(0.f, 0.f, moveDist, 0.f);
    }
  } else if (m_moveBackward) {
    if (m_moveLeft && (!m_moveRight || m_preferLeft)) {
      moveVec = XMVectorSet(-moveDiagDist, 0.f, -moveDiagDist, 0.f);
    } else if (m_moveRight) {
      moveVec = XMVectorSet(moveDiagDist, 0.f, -moveDiagDist, 0.f);
    } else {
      moveVec = XMVectorSet(0.f, 0.f, -moveDist, 0.f);
    }
  } else if (m_moveLeft && (!m_moveRight || m_preferLeft)) {
    moveVec = XMVectorSet(-moveDist, 0.f, 0.f, 0.f);
  } else if (m_moveRight) {
    moveVec = XMVectorSet(moveDist, 0.f, 0.f, 0.f);
  }

  XMMATRIX rotateMat =
      XMMatrixRotationZ(m_roll) * XMMatrixRotationX(m_pitch) * XMMatrixRotationY(m_yaw);
  moveVec = XMVector3Transform(moveVec, rotateMat);

  m_x += XMVectorGetX(moveVec);
  m_y += XMVectorGetY(moveVec);
  m_z += XMVectorGetZ(moveVec);
}

void Camera::UpdateMousePosition(int mouseX, int mouseY) {
  if (m_currentMouseX == -1 || m_currentMouseY == -1) {
    m_currentMouseX = mouseX;
    m_currentMouseY = mouseY;
    return;
  }

  m_pitch += (mouseY - m_currentMouseY) * m_lookSpeed;
  m_yaw += (mouseX - m_currentMouseX) * m_lookSpeed;

  m_currentMouseX = mouseX;
  m_currentMouseY = mouseY;
}

void Camera::MoveForward(bool flag) {
  m_moveForward = flag;
  if (flag)
    m_preferForward = true;
}

void Camera::MoveBackward(bool flag) {
  m_moveBackward = flag;
  if (flag)
    m_preferForward = false;
}

void Camera::MoveLeft(bool flag) {
  m_moveLeft = flag;
  if (flag)
    m_preferLeft = true;
}

void Camera::MoveRight(bool flag) {
  m_moveRight = flag;
  if (flag)
    m_preferLeft = false;
}

void Camera::ToggleEnabled() {
  Enable(!m_enabled);
}

void Camera::Enable(bool flag) {
  m_enabled = flag;

  if (!flag) {
    m_currentMouseX = -1;
    m_currentMouseY = -1;
  }
}

} // namespace utils
