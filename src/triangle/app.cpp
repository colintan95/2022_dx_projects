#include "app.h"

#include <d3dx12.h>

#include "gen/shader_ps.h"
#include "gen/shader_vs.h"

using winrt::check_bool;
using winrt::check_hresult;
using winrt::com_ptr;

App::App(HWND hwnd) : m_hwnd(hwnd) {
  RECT windowRect{};
  check_bool(GetWindowRect(m_hwnd, &windowRect));

  m_windowWidth = windowRect.right - windowRect.left;
  m_windowHeight = windowRect.bottom - windowRect.top;

  CreateDevice();
  CreateCommandQueueAndSwapChain();
  CreateCommandListAndFence();

  CreatePipeline();
  CreateDescriptorHeaps();

  CreateDepthTexture();
  CreateVertexBuffers();
}

void App::CreateDevice() {
  com_ptr<ID3D12Debug1> debugController;
  check_hresult(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put())));

  debugController->EnableDebugLayer();
  debugController->SetEnableGPUBasedValidation(true);

  check_hresult(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(m_factory.put())));

  static constexpr D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_12_1;

  com_ptr<IDXGIAdapter1> adapter;

  for (uint32_t adapterIndex = 0;
       m_factory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                             IID_PPV_ARGS(adapter.put())) != DXGI_ERROR_NOT_FOUND;
       ++adapterIndex) {
    DXGI_ADAPTER_DESC1 desc;
    adapter->GetDesc1(&desc);

    if (SUCCEEDED(D3D12CreateDevice(adapter.get(), minFeatureLevel, _uuidof(ID3D12Device),
                                    nullptr))) {
      break;
    }
  }

  check_hresult(D3D12CreateDevice(adapter.get(), minFeatureLevel, IID_PPV_ARGS(m_device.put())));
}

void App::CreateCommandQueueAndSwapChain() {
  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

  check_hresult(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_cmdQueue.put())));

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
  swapChainDesc.BufferCount = k_numFrames;
  swapChainDesc.Width = m_windowWidth;
  swapChainDesc.Height = m_windowHeight;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.SampleDesc.Count = 1;

  winrt::com_ptr<IDXGISwapChain1> swapChain;
  check_hresult(m_factory->CreateSwapChainForHwnd(m_cmdQueue.get(), m_hwnd, &swapChainDesc, nullptr,
                                                  nullptr, swapChain.put()));
  swapChain.as(m_swapChain);

  for (int i = 0; i < k_numFrames; ++i) {
    check_hresult(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_frames[i].SwapChainBuffer)));
  }

  m_viewport = CD3DX12_VIEWPORT(0.f, 0.f, static_cast<float>(m_windowWidth),
                                static_cast<float>(m_windowHeight));
  m_scissorRect = CD3DX12_RECT(0, 0, m_windowWidth, m_windowHeight);
}

void App::CreateCommandListAndFence() {
  check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 IID_PPV_ARGS(m_cmdAlloc.put())));

  for (int i = 0; i < k_numFrames; ++i) {
    check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                   IID_PPV_ARGS(m_frames[i].CmdAlloc.put())));
  }

  check_hresult(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAlloc.get(),
                                            nullptr, IID_PPV_ARGS(m_cmdList.put())));
  check_hresult(m_cmdList->Close());

  check_hresult(m_device->CreateFence(m_nextFenceValue, D3D12_FENCE_FLAG_NONE,
                                      IID_PPV_ARGS(m_fence.put())));
  ++m_nextFenceValue;

  m_fenceEvent.reset(CreateEvent(nullptr, false, false, nullptr));
  check_bool(m_fenceEvent.is_valid());
}

void App::CreatePipeline() {
  CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init_1_1(0, nullptr, 0, nullptr,
                       D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  com_ptr<ID3DBlob> signatureBlob;
  com_ptr<ID3DBlob> errorBlob;
  check_hresult(D3D12SerializeVersionedRootSignature(&rootSigDesc, signatureBlob.put(),
                                                     errorBlob.put()));
  check_hresult(m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                              signatureBlob->GetBufferSize(),
                                              IID_PPV_ARGS(m_rootSig.put())));

  D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
     0},
  };

  D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
  inputLayoutDesc.pInputElementDescs = inputElementDescs;
  inputLayoutDesc.NumElements = _countof(inputElementDescs);

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
  pipelineDesc.InputLayout = inputLayoutDesc;
  pipelineDesc.pRootSignature = m_rootSig.get();
  pipelineDesc.VS = CD3DX12_SHADER_BYTECODE(g_shaderVS, ARRAYSIZE(g_shaderVS));
  pipelineDesc.PS = CD3DX12_SHADER_BYTECODE(g_shaderPS, ARRAYSIZE(g_shaderPS));;
  pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  pipelineDesc.SampleMask = UINT_MAX;
  pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipelineDesc.NumRenderTargets = 1;
  pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
  pipelineDesc.SampleDesc.Count = 1;

  check_hresult(m_device->CreateGraphicsPipelineState(&pipelineDesc,
                                                      IID_PPV_ARGS(m_pipeline.put())));
}

void App::CreateDescriptorHeaps() {
  {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = k_numFrames;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    check_hresult(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.put())));
    m_rtvHandleSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < k_numFrames; ++i) {
      m_device->CreateRenderTargetView(m_frames[i].SwapChainBuffer.get(), nullptr, handle);
      m_frames[i].RtvHandle = handle;

      handle.Offset(m_rtvHandleSize);
    }
  }

  {
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

    check_hresult(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.put())));
    m_dsvHandleSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    m_dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
  }
}

void App::CreateDepthTexture() {
  CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC textureDesc =
      CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_windowWidth, m_windowHeight, 1, 0, 1, 0,
                                   D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
                                   D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
  CD3DX12_CLEAR_VALUE clearValue(DXGI_FORMAT_D32_FLOAT, 1.f, 0);

  check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                                  D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                                  IID_PPV_ARGS(m_depthTexture.put())));

  D3D12_DEPTH_STENCIL_VIEW_DESC depthViewDesc{};
  depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
  depthViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

  m_device->CreateDepthStencilView(m_depthTexture.get(), &depthViewDesc, m_dsvHandle);
}

void App::CreateVertexBuffers() {
  static constexpr float posData[] = {
    -0.5f, -0.5f, 0.f,
    0.f, 0.5f, 0.f,
    0.5f, -0.5f, 0.f
  };

  check_hresult(m_cmdAlloc->Reset());
  check_hresult(m_cmdList->Reset(m_cmdAlloc.get(), nullptr));

  size_t bufferSize = sizeof(posData);

  com_ptr<ID3D12Resource> uploadBuffer;

  CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

  check_hresult(m_device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
                                                  &uploadBufferDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(uploadBuffer.put())));

  void* ptr;
  check_hresult(uploadBuffer->Map(0, nullptr, &ptr));

  memcpy(ptr, reinterpret_cast<const void*>(posData), bufferSize);

  uploadBuffer->Unmap(0, nullptr);

  CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

  check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(m_vertexBuffer.put())));

  m_cmdList->CopyBufferRegion(m_vertexBuffer.get(), 0, uploadBuffer.get(), 0, bufferSize);

  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      m_vertexBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

  m_cmdList->ResourceBarrier(1, &barrier);

  check_hresult(m_cmdList->Close());

  ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
  m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

  WaitForGpu();

  m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
  m_vertexBufferView.SizeInBytes = bufferSize;
  m_vertexBufferView.StrideInBytes = sizeof(float) * 3;
}

App::~App() {
  WaitForGpu();
}

void App::RenderFrame() {
  check_hresult(m_frames[m_currentFrame].CmdAlloc->Reset());
  check_hresult(m_cmdList->Reset(m_frames[m_currentFrame].CmdAlloc.get(), nullptr));

  {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_frames[m_currentFrame].SwapChainBuffer.get(), D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_cmdList->ResourceBarrier(1, &barrier);
  }

  m_cmdList->SetPipelineState(m_pipeline.get());
  m_cmdList->SetGraphicsRootSignature(m_rootSig.get());

  m_cmdList->RSSetViewports(1, &m_viewport);
  m_cmdList->RSSetScissorRects(1, &m_scissorRect);

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_frames[m_currentFrame].RtvHandle;
  m_cmdList->OMSetRenderTargets(1, &rtvHandle, false, &m_dsvHandle);

  static constexpr float clearColor[] = { 0.f, 0.f, 0.f, 1.f };
  m_cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_cmdList->ClearDepthStencilView(m_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

  m_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_cmdList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

  m_cmdList->DrawInstanced(3, 1, 0, 0);

  {
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_frames[m_currentFrame].SwapChainBuffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_cmdList->ResourceBarrier(1, &barrier);
  }

  check_hresult(m_cmdList->Close());

  ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
  m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

  check_hresult(m_swapChain->Present(1, 0));

  MoveToNextFrame();
}

void App::MoveToNextFrame() {
  check_hresult(m_cmdQueue->Signal(m_fence.get(), m_nextFenceValue));
  m_frames[m_currentFrame].FenceWaitValue = m_nextFenceValue;

  ++m_nextFenceValue;

  m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();

  if (m_fence->GetCompletedValue() < m_frames[m_currentFrame].FenceWaitValue) {
    check_hresult(m_fence->SetEventOnCompletion(m_frames[m_currentFrame].FenceWaitValue,
                                                m_fenceEvent.get()));
    WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, false);
  }
}

void App::WaitForGpu() {
  uint64_t waitValue = m_nextFenceValue;

  check_hresult(m_cmdQueue->Signal(m_fence.get(), waitValue));
  ++m_nextFenceValue;

  check_hresult(m_fence->SetEventOnCompletion(waitValue, m_fenceEvent.get()));
  WaitForSingleObjectEx(m_fenceEvent.get(), INFINITE, false);
}
