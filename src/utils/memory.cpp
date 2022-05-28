#include "utils/memory.h"

#include <d3dx12.h>
#include <winrt/base.h>

using winrt::check_hresult;
using winrt::com_ptr;

namespace utils {

void CreateBuffersAndUpload(ID3D12GraphicsCommandList* cmdList, const std::vector<uint8_t>& data,
                            ID3D12Device* device, ID3D12Resource** outputBuffer,
                            ID3D12Resource** outputUploadBuffer) {
  size_t bufferSize = data.size();

  com_ptr<ID3D12Resource> uploadBuffer;

  CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

  check_hresult(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE,
                                                &uploadBufferDesc,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(uploadBuffer.put())));

  void* ptr;
  check_hresult(uploadBuffer->Map(0, nullptr, &ptr));

  memcpy(ptr, reinterpret_cast<const void*>(data.data()), bufferSize);

  uploadBuffer->Unmap(0, nullptr);

  com_ptr<ID3D12Resource> buffer;

  CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
  CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

  check_hresult(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                IID_PPV_ARGS(buffer.put())));

  cmdList->CopyBufferRegion(buffer.get(), 0, uploadBuffer.get(), 0, bufferSize);

  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      buffer.get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

  cmdList->ResourceBarrier(1, &barrier);

  *outputBuffer = buffer.detach();
  *outputUploadBuffer = uploadBuffer.detach();
}

} // namespace utils
