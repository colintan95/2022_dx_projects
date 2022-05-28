#include "app.h"

#include <d3dx12.h>
#include <DirectXMath.h>

#include <vector>

#include <utils/gltf_loader.h>
#include <utils/memory.h>

#include "gen/shader.h"

using namespace DirectX;

using winrt::check_bool;
using winrt::check_hresult;
using winrt::com_ptr;

const wchar_t* k_rayGenShaderName = L"RayGenShader";
const wchar_t* k_closestHitShaderName = L"ClosestHitShader";
const wchar_t* k_missShaderName = L"MissShader";

const wchar_t* k_hitGroupName = L"HitGroup";

App::App(HWND hwnd) : m_hwnd(hwnd) {
  RECT windowRect{};
  check_bool(GetWindowRect(m_hwnd, &windowRect));

  m_windowWidth = windowRect.right - windowRect.left;
  m_windowHeight = windowRect.bottom - windowRect.top;

  CreateDevice();
  CreateCommandQueueAndSwapChain();
  CreateCommandListAndFence();

  CreatePipeline();
  CreateDescriptorHeap();

  CreateResources();
  CreateConstantBuffers();
  CreateModelBuffers();

  CreateShaderTables();
  CreateAccelerationStructures();
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
  // Global root signature
  {
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2] = {};
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParams[2] = {};
    rootParams[0].InitAsDescriptorTable(1, &ranges[0]);
    rootParams[1].InitAsShaderResourceView(0);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(_countof(rootParams), rootParams);

    com_ptr<ID3DBlob> signatureBlob;
    com_ptr<ID3DBlob> errorBlob;
    check_hresult(D3D12SerializeVersionedRootSignature(&rootSigDesc, signatureBlob.put(),
                                                       errorBlob.put()));
    check_hresult(m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                                signatureBlob->GetBufferSize(),
                                                IID_PPV_ARGS(m_globalRootSig.put())));
  }

  // Closest hit root signature
  {
    CD3DX12_ROOT_PARAMETER1 rootParams[4] = {};
    rootParams[0].InitAsShaderResourceView(1);
    rootParams[1].InitAsShaderResourceView(2);
    rootParams[2].InitAsConstantBufferView(0);
    rootParams[3].InitAsConstants(1, 1);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr,
                         D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

    com_ptr<ID3DBlob> signatureBlob;
    com_ptr<ID3DBlob> errorBlob;
    check_hresult(D3D12SerializeVersionedRootSignature(&rootSigDesc, signatureBlob.put(),
                                                       errorBlob.put()));
    check_hresult(m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                                signatureBlob->GetBufferSize(),
                                                IID_PPV_ARGS(m_closestHitRootSig.put())));
  }

  CD3DX12_STATE_OBJECT_DESC pipelineDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

  auto* dxilLib = pipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

  auto shaderSrc = CD3DX12_SHADER_BYTECODE(reinterpret_cast<const void*>(g_shader),
                                           sizeof(g_shader));
  dxilLib->SetDXILLibrary(&shaderSrc);

  const wchar_t* shaderNames[] = { k_rayGenShaderName, k_closestHitShaderName, k_missShaderName };
  dxilLib->DefineExports(shaderNames);

  auto* shaderConfig = pipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
  uint32_t payloadSize = sizeof(float) * 4;
  uint32_t attributesSize = sizeof(float) * 2;
  shaderConfig->Config(payloadSize, attributesSize);

  {
    auto* rootSigSubObj = pipelineDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    rootSigSubObj->SetRootSignature(m_globalRootSig.get());
  }

  auto* hitGroup = pipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
  hitGroup->SetClosestHitShaderImport(k_closestHitShaderName);
  hitGroup->SetHitGroupExport(k_hitGroupName);
  hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

  {
    auto* rootSigSubObj = pipelineDesc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    rootSigSubObj->SetRootSignature(m_closestHitRootSig.get());

    CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT* association =
        pipelineDesc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    association->SetSubobjectToAssociate(*rootSigSubObj);
    association->AddExport(k_closestHitShaderName);
  }

  auto* pipelineConfig =
      pipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
  uint32_t maxRecursionDepth = 2;
  pipelineConfig->Config(maxRecursionDepth);

  check_hresult(m_device->CreateStateObject(pipelineDesc, IID_PPV_ARGS(m_pipeline.put())));
}

void App::CreateDescriptorHeap() {
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
  heapDesc.NumDescriptors = 1;
  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_descriptorHeap.put()));

  m_cbvSrvUavHandleSize =
      m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
  CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

  m_filmUavCpuHandle = cpuHandle;
  m_filmUavGpuHandle = gpuHandle;
}

void App::CreateResources() {
  CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC bufferDesc =
      CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_windowWidth, m_windowHeight, 1, 1,
                                   1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

  check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                  IID_PPV_ARGS(m_film.put())));
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

  m_device->CreateUnorderedAccessView(m_film.get(), nullptr, &uavDesc, m_filmUavCpuHandle);
}

void App::CreateConstantBuffers() {
  XMFLOAT3X4 worldMat;
  XMStoreFloat3x4(&worldMat, XMMatrixIdentity());

  // Flip the z axis since gltf uses right-handed coordinates.
  worldMat.m[2][2] *= -1.f;

  size_t bufferSize = utils::GetAlignedSize(sizeof(DirectX::XMFLOAT3X4),
                                            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

  CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

  check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  IID_PPV_ARGS(m_matrixBuffer.put())));

  XMFLOAT3X4* ptr;
  check_hresult(m_matrixBuffer->Map(0, nullptr, reinterpret_cast<void**>(&ptr)));

  *ptr = worldMat;

  m_matrixBuffer->Unmap(0, nullptr);
}

void App::CreateModelBuffers() {
  utils::Scene scene = utils::LoadGltf("assets/cornell_box.gltf");

  check_hresult(m_cmdAlloc->Reset());
  check_hresult(m_cmdList->Reset(m_cmdAlloc.get(), nullptr));

  std::vector<com_ptr<ID3D12Resource>> uploadBuffers;

  for (auto& bufferData : scene.Buffers) {
    com_ptr<ID3D12Resource> buffer;
    com_ptr<ID3D12Resource> uploadBuffer;

    utils::CreateBuffersAndUpload(m_cmdList.get(), bufferData, m_device.get(), buffer.put(),
                                  uploadBuffer.put());

    m_modelBuffers.push_back(buffer);
    uploadBuffers.push_back(uploadBuffer);
  }

  check_hresult(m_cmdList->Close());

  ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
  m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

  WaitForGpu();

  for (auto& meshData : scene.Meshes) {
    for (auto& primData : meshData.Primitives) {
      utils::BufferView* posBufferView = primData.Positions->BufferView;
      utils::BufferView* normalBufferView = primData.Normals->BufferView;
      utils::BufferView* indexBufferView = primData.Indices->BufferView;

      ID3D12Resource* posBuffer = m_modelBuffers[posBufferView->BufferIndex].get();
      ID3D12Resource* normalBuffer = m_modelBuffers[normalBufferView->BufferIndex].get();
      ID3D12Resource* indexBuffer = m_modelBuffers[indexBufferView->BufferIndex].get();

      D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc{};
      geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
      geometryDesc.Triangles.Transform3x4 = m_matrixBuffer->GetGPUVirtualAddress();
      geometryDesc.Triangles.VertexBuffer.StartAddress = posBuffer->GetGPUVirtualAddress() +
                                                         posBufferView->Offset;
      geometryDesc.Triangles.VertexBuffer.StrideInBytes = *posBufferView->Stride;
      geometryDesc.Triangles.VertexCount = primData.Positions->Count;
      geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
      geometryDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() +
                                           indexBufferView->Offset;
      geometryDesc.Triangles.IndexCount = primData.Indices->Count;
      geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
      geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

      m_geometryDescs.push_back(geometryDesc);

      m_normalBuffers.push_back(normalBuffer->GetGPUVirtualAddress() + normalBufferView->Offset);
      m_indexBuffers.push_back(indexBuffer->GetGPUVirtualAddress() + indexBufferView->Offset);
      m_normalBufferStrides.push_back(*normalBufferView->Stride);
    }
  }
}

// Disable padding alignment warning.
#pragma warning(push)
#pragma warning(disable:4324)

struct alignas(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) RayGenShaderRecord {
  uint8_t ShaderId[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
};

struct alignas(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) HitGroupShaderRecord {
  uint8_t ShaderId[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
  alignas(sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) D3D12_GPU_VIRTUAL_ADDRESS NormalBuffer;
  alignas(sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
  alignas(sizeof(D3D12_GPU_VIRTUAL_ADDRESS)) D3D12_GPU_VIRTUAL_ADDRESS MatrixBuffer;
  alignas(uint32_t) uint32_t NormalBufferStride;

};

struct alignas(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT) MissShaderRecord {
  uint8_t ShaderId[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
};

#pragma warning(pop)

void App::CreateShaderTables() {
  com_ptr<ID3D12StateObjectProperties> pipelineProps;
  m_pipeline.as(pipelineProps);

  {
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(RayGenShaderRecord));

    check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                    &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    nullptr,
                                                    IID_PPV_ARGS(m_rayGenShaderTable.put())));

    void* shaderId = pipelineProps->GetShaderIdentifier(k_rayGenShaderName);

    RayGenShaderRecord* ptr;
    check_hresult(m_rayGenShaderTable->Map(0, nullptr, reinterpret_cast<void**>(&ptr)));

    memcpy(ptr->ShaderId, shaderId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    m_rayGenShaderTable->Unmap(0, nullptr);
  }

  {
    m_hitGroupShaderRecordSize = sizeof(HitGroupShaderRecord);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(m_hitGroupShaderRecordSize * m_geometryDescs.size());

    check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                    &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    nullptr,
                                                    IID_PPV_ARGS(m_hitGroupShaderTable.put())));

    void* shaderId = pipelineProps->GetShaderIdentifier(k_hitGroupName);

    HitGroupShaderRecord* ptr;
    check_hresult(m_hitGroupShaderTable->Map(0, nullptr, reinterpret_cast<void**>(&ptr)));

    for (int i = 0; i < m_geometryDescs.size(); ++i) {
      memcpy(ptr->ShaderId, shaderId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
      ptr->NormalBuffer = m_normalBuffers[i];
      ptr->IndexBuffer = m_indexBuffers[i];
      ptr->MatrixBuffer = m_matrixBuffer->GetGPUVirtualAddress();
      ptr->NormalBufferStride = m_normalBufferStrides[i];
      ++ptr;
    }

    m_hitGroupShaderTable->Unmap(0, nullptr);
  }

  {
    m_missShaderRecordSize = sizeof(MissShaderRecord);

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(m_missShaderRecordSize);

    check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                                    &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    nullptr,
                                                    IID_PPV_ARGS(m_missShaderTable.put())));

    void* shaderId = pipelineProps->GetShaderIdentifier(k_missShaderName);

    MissShaderRecord* ptr;
    check_hresult(m_missShaderTable->Map(0, nullptr, reinterpret_cast<void**>(&ptr)));

    memcpy(ptr->ShaderId, shaderId, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    m_missShaderTable->Unmap(0, nullptr);
  }
}

void App::CreateAccelerationStructures() {
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
  tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
  tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  tlasInputs.NumDescs = 1;

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo{};
  m_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs{};
  blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  blasInputs.NumDescs = static_cast<uint32_t>(m_geometryDescs.size());
  blasInputs.pGeometryDescs = m_geometryDescs.data();

  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo{};
  m_device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuildInfo);

  com_ptr<ID3D12Resource> scratchResource;

  {
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(max(blasPrebuildInfo.ResultDataMaxSizeInBytes,
                                          tlasPrebuildInfo.ResultDataMaxSizeInBytes),
                                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    check_hresult(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                    IID_PPV_ARGS(scratchResource.put())));
  }

  {
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(blasPrebuildInfo.ResultDataMaxSizeInBytes,
                                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    check_hresult(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
        IID_PPV_ARGS(m_blas.put())));
  }

  {
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(tlasPrebuildInfo.ResultDataMaxSizeInBytes,
                                      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    check_hresult(m_device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
        IID_PPV_ARGS(m_tlas.put())));
  }

  D3D12_RAYTRACING_INSTANCE_DESC instanceDesc{};
  instanceDesc.Transform[0][0] = 1;
  instanceDesc.Transform[1][1] = 1;
  instanceDesc.Transform[2][2] = 1;
  instanceDesc.InstanceMask = 1;
  // Gltf uses counter-clockwise winding order.
  instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
  instanceDesc.AccelerationStructure = m_blas->GetGPUVirtualAddress();

  com_ptr<ID3D12Resource> instanceDescBuffer;

  {
    CD3DX12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(instanceDesc));

    check_hresult(m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &buffer_desc,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                    IID_PPV_ARGS(instanceDescBuffer.put())));

    D3D12_RAYTRACING_INSTANCE_DESC* ptr;
    instanceDescBuffer->Map(0, nullptr, reinterpret_cast<void**>(&ptr));

    *ptr = instanceDesc;

    instanceDescBuffer->Unmap(0, nullptr);
  }

  check_hresult(m_cmdAlloc->Reset());
  check_hresult(m_cmdList->Reset(m_cmdAlloc.get(), nullptr));

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc{};
  blasBuildDesc.Inputs = blasInputs;
  blasBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
  blasBuildDesc.DestAccelerationStructureData = m_blas->GetGPUVirtualAddress();

  m_cmdList->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);

  {
    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(m_blas.get());
    m_cmdList->ResourceBarrier(1, &barrier);
  }

  tlasInputs.InstanceDescs = instanceDescBuffer->GetGPUVirtualAddress();

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc{};
  tlasBuildDesc.Inputs = tlasInputs;
  tlasBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
  tlasBuildDesc.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();

  m_cmdList->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);

  check_hresult(m_cmdList->Close());

  ID3D12CommandList* cmdLists[] = { m_cmdList.get() };
  m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

  WaitForGpu();
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

  m_cmdList->SetComputeRootSignature(m_globalRootSig.get());

  ID3D12DescriptorHeap* descriptorHeaps[] = { m_descriptorHeap.get() };
  m_cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

  m_cmdList->SetComputeRootDescriptorTable(0, m_filmUavGpuHandle);
  m_cmdList->SetComputeRootShaderResourceView(1, m_tlas->GetGPUVirtualAddress());

  D3D12_DISPATCH_RAYS_DESC dispatchDesc{};

  dispatchDesc.RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
  dispatchDesc.RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;

  dispatchDesc.HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
  dispatchDesc.HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
  dispatchDesc.HitGroupTable.StrideInBytes = m_hitGroupShaderRecordSize;

  dispatchDesc.MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
  dispatchDesc.MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
  dispatchDesc.MissShaderTable.StrideInBytes = m_missShaderRecordSize;

  dispatchDesc.Width = m_windowWidth;
  dispatchDesc.Height = m_windowHeight;
  dispatchDesc.Depth = 1;

  m_cmdList->SetPipelineState1(m_pipeline.get());
  m_cmdList->DispatchRays(&dispatchDesc);

  {
    D3D12_RESOURCE_BARRIER barriers[2] = {};

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_frames[m_currentFrame].SwapChainBuffer.get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_DEST);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_film.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE);

    m_cmdList->ResourceBarrier(_countof(barriers), barriers);
  }

  m_cmdList->CopyResource(m_frames[m_currentFrame].SwapChainBuffer.get(), m_film.get());

  {
    D3D12_RESOURCE_BARRIER barriers[2] = {};

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_frames[m_currentFrame].SwapChainBuffer.get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PRESENT);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
        m_film.get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    m_cmdList->ResourceBarrier(_countof(barriers), barriers);
  }

  m_cmdList->Close();

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
