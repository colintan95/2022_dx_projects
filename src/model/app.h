#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <winrt/base.h>
#include <wil/resource.h>

#include <vector>

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

  void CreatePipeline();
  void CreateDescriptorHeaps();

  void CreateDepthTexture();
  void CreateVertexBuffers();
  void CreateConstantBuffer();

  void MoveToNextFrame();
  void WaitForGpu();

  HWND m_hwnd;
  int m_windowWidth = 0;
  int m_windowHeight = 0;

  winrt::com_ptr<IDXGIFactory6> m_factory;
  winrt::com_ptr<ID3D12Device> m_device;
  winrt::com_ptr<ID3D12CommandQueue> m_cmdQueue;
  winrt::com_ptr<IDXGISwapChain3> m_swapChain;

  D3D12_VIEWPORT m_viewport;
  D3D12_RECT m_scissorRect;

  winrt::com_ptr<ID3D12CommandAllocator> m_cmdAlloc;
  winrt::com_ptr<ID3D12GraphicsCommandList> m_cmdList;

  winrt::com_ptr<ID3D12Fence> m_fence;
  uint64_t m_nextFenceValue = 0;
  wil::unique_handle m_fenceEvent;

  winrt::com_ptr<ID3D12RootSignature> m_rootSig;
  winrt::com_ptr<ID3D12PipelineState> m_pipeline;

  winrt::com_ptr<ID3D12DescriptorHeap> m_rtvHeap;
  uint32_t m_rtvHandleSize = 0;

  winrt::com_ptr<ID3D12DescriptorHeap> m_dsvHeap;
  uint32_t m_dsvHandleSize = 0;

  D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle;

  winrt::com_ptr<ID3D12Resource> m_depthTexture;

  std::vector<winrt::com_ptr<ID3D12Resource>> m_vertexBuffers;

  struct Primitive {
    D3D12_VERTEX_BUFFER_VIEW Positions;
    D3D12_VERTEX_BUFFER_VIEW Normals;
    D3D12_INDEX_BUFFER_VIEW Indices;
    uint32_t NumVertices;
  };
  std::vector<Primitive> m_primitives;

  struct MatrixBuffer {
    DirectX::XMFLOAT4X4 WorldMat;
    DirectX::XMFLOAT4X4 WorldViewProjMat;
  };
  MatrixBuffer m_matrixBuffer;

  winrt::com_ptr<ID3D12Resource> m_constantBuffer;

  struct Frame {
    winrt::com_ptr<ID3D12Resource> SwapChainBuffer;
    winrt::com_ptr<ID3D12CommandAllocator> CmdAlloc;
    D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle;
    uint64_t FenceWaitValue = 0;
  };
  Frame m_frames[k_numFrames];

  int m_currentFrame = 0;
};
