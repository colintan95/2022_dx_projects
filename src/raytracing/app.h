#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <winrt/base.h>
#include <wil/resource.h>

#include <vector>

#include <utils/gltf_loader.h>

#include "shader.h"

inline constexpr int k_numFrames = 3;

class App {
public:
  App(HWND hwnd);
  ~App();

  void RenderFrame();

private:
  void CreateDevice();
  void CreateCommandQueueAndSwapChain();
  void CreateCommandListAndFence();

  void LoadModel();

  void CreatePipeline();
  void CreateDescriptorHeap();

  void CreateResources();
  void CreateConstantBuffers();
  void CreateModelBuffers();

  void CreateShaderTables();
  void CreateAccelerationStructures();

  void MoveToNextFrame();
  void WaitForGpu();

  HWND m_hwnd;
  int m_windowWidth = 0;
  int m_windowHeight = 0;

  winrt::com_ptr<IDXGIFactory6> m_factory;
  winrt::com_ptr<ID3D12Device5> m_device;
  winrt::com_ptr<ID3D12CommandQueue> m_cmdQueue;
  winrt::com_ptr<IDXGISwapChain3> m_swapChain;

  winrt::com_ptr<ID3D12Fence> m_fence;
  uint64_t m_nextFenceValue = 0;
  wil::unique_handle m_fenceEvent;

  winrt::com_ptr<ID3D12CommandAllocator> m_cmdAlloc;
  winrt::com_ptr<ID3D12GraphicsCommandList4> m_cmdList;

  utils::Scene m_model;

  winrt::com_ptr<ID3D12RootSignature> m_globalRootSig;
  winrt::com_ptr<ID3D12RootSignature> m_closestHitRootSig;

  winrt::com_ptr<ID3D12StateObject> m_pipeline;

  winrt::com_ptr<ID3D12Resource> m_rayGenShaderTable;
  winrt::com_ptr<ID3D12Resource> m_hitGroupShaderTable;
  winrt::com_ptr<ID3D12Resource> m_missShaderTable;

  uint64_t m_hitGroupShaderRecordSize;
  uint64_t m_missShaderRecordSize;

  winrt::com_ptr<ID3D12DescriptorHeap> m_descriptorHeap;
  uint32_t m_cbvSrvUavHandleSize;

  D3D12_CPU_DESCRIPTOR_HANDLE m_filmUavCpuHandle;
  D3D12_GPU_DESCRIPTOR_HANDLE m_filmUavGpuHandle;

  winrt::com_ptr<ID3D12Resource> m_film;

  winrt::com_ptr<ID3D12Resource> m_matrixBuffer;
  winrt::com_ptr<ID3D12Resource> m_materialsBuffer;

  std::vector<winrt::com_ptr<ID3D12Resource>> m_modelBuffers;

  winrt::com_ptr<ID3D12Resource> m_blas;
  winrt::com_ptr<ID3D12Resource> m_tlas;

  struct Frame {
    winrt::com_ptr<ID3D12Resource> SwapChainBuffer;
    winrt::com_ptr<ID3D12CommandAllocator> CmdAlloc;
    D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle;
    uint64_t FenceWaitValue = 0;
  };
  Frame m_frames[k_numFrames];

  int m_currentFrame = 0;
};
