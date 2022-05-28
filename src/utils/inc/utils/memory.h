#pragma once

#include <d3d12.h>

#include <vector>

namespace utils {

constexpr size_t GetAlignedSize(size_t size, size_t alignment) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}

template<typename T>
size_t GetAlignedSize(const T&, size_t alignment) {
  return GetAlignedSize(sizeof(T), alignment);
}

void CreateBuffersAndUpload(ID3D12GraphicsCommandList* cmdList, const std::vector<uint8_t>& data,
                            ID3D12Device* device, ID3D12Resource** outputBuffer,
                            ID3D12Resource** outputUploadBuffer);

} // namespace utils
