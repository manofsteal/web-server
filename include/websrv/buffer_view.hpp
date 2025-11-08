#pragma once
#include <cstddef>

namespace websrv {

struct BufferView {
  char* data;
  size_t size;
  
  BufferView() : data(nullptr), size(0) {}
  
  BufferView(char* ptr, size_t len) : data(ptr), size(len) {}
  
  BufferView(const char* ptr, size_t len) : data(const_cast<char*>(ptr)), size(len) {}
  
  bool empty() const { return size == 0 || data == nullptr; }
  
  char& operator[](size_t index) { return data[index]; }
  const char& operator[](size_t index) const { return data[index]; }
  
  char* begin() { return data; }
  const char* begin() const { return data; }
  
  char* end() { return data + size; }
  const char* end() const { return data + size; }
};

} // namespace websrv