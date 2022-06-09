#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <winrt/base.h>
#include <wil/resource.h>

#include <vector>

#include <utils/gltf_loader.h>
#include <utils/window.h>

#include "shader.h"

inline constexpr int k_numFrames = 3;

inline constexpr uint32_t k_maxSamples = 1000;
inline constexpr uint32_t k_sampleIncrement = 10;
inline constexpr uint32_t k_numBounces = 8;

class App {
public:
  App(utils::Window* window);
  ~App();

  void RenderFrame();

private:
  void CreateDevice();
  void CreateCommandQueueAndSwapChain();
  void CreateCommandListAndFence();

  void CreateAssets();

  void CreatePipeline();
  void CreateDescriptorHeap();

  void CreateResources();
  void CreateConstantBuffers();
  void CreateGeometryBuffers();

  void CreateShaderTables();
  void CreateAccelerationStructures();

  void CreateBlas(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
                  ID3D12Resource** blas);
  void CreateTlas(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
                  ID3D12Resource** tlas);

  void MoveToNextFrame();
  void WaitForGpu();

  utils::Window* m_window;

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

  struct QuadLight {
    shader::Quad Quad;
    D3D12_RAYTRACING_AABB Aabb;
  };
  QuadLight m_light;

  winrt::com_ptr<ID3D12RootSignature> m_globalRootSig;
  winrt::com_ptr<ID3D12RootSignature> m_closestHitRootSig;
  winrt::com_ptr<ID3D12RootSignature> m_quadIntersectRootSig;

  winrt::com_ptr<ID3D12StateObject> m_pipeline;

  winrt::com_ptr<ID3D12Resource> m_rayGenShaderTable;
  winrt::com_ptr<ID3D12Resource> m_hitGroupShaderTable;
  winrt::com_ptr<ID3D12Resource> m_missShaderTable;

  uint64_t m_hitGroupShaderRecordSize;
  uint64_t m_missShaderRecordStride;

  winrt::com_ptr<ID3D12DescriptorHeap> m_descriptorHeap;
  uint32_t m_cbvSrvUavHandleSize;

  D3D12_CPU_DESCRIPTOR_HANDLE m_filmUavCpuHandle;
  D3D12_GPU_DESCRIPTOR_HANDLE m_filmUavGpuHandle;

  winrt::com_ptr<ID3D12Resource> m_film;

  winrt::com_ptr<ID3D12Resource> m_matrixBuffer;
  winrt::com_ptr<ID3D12Resource> m_materialsBuffer;
  winrt::com_ptr<ID3D12Resource> m_lightQuadBuffer;

  std::vector<winrt::com_ptr<ID3D12Resource>> m_modelBuffers;
  winrt::com_ptr<ID3D12Resource> m_aabbBuffer;

  std::vector<winrt::com_ptr<ID3D12Resource>> m_scratchResources;

  winrt::com_ptr<ID3D12Resource> m_blas;
  winrt::com_ptr<ID3D12Resource> m_aabbBlas;
  winrt::com_ptr<ID3D12Resource> m_tlas;

  struct Frame {
    winrt::com_ptr<ID3D12Resource> SwapChainBuffer;
    winrt::com_ptr<ID3D12CommandAllocator> CmdAlloc;
    D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle;
    uint64_t FenceWaitValue = 0;
  };
  Frame m_frames[k_numFrames];

  int m_currentFrame = 0;

  uint32_t m_currentSample = 1;
};
