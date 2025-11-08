#include "websrv/buffer.hpp"
#include <iostream>
#include <string>

using namespace websrv;

int main() {
  Buffer buf;

  // Example 1: Append some data
  std::string hello = "Hello, World!";
  buf.append(hello.data(), hello.size());

  // Example 2: Read data byte by byte
  std::cout << "Reading data byte by byte:\n";
  for (size_t i = 0; i < buf.size(); ++i) {
    std::cout << buf.getAt(i);
  }
  std::cout << "\n\n";

  // Example 3: Modify some data
  buf.setAt(7, 'C');
  buf.setAt(8, 'o');
  buf.setAt(9, 'd');
  buf.setAt(10, 'e');

  // Example 4: Read modified data
  std::cout << "After modification:\n";
  for (size_t i = 0; i < buf.size(); ++i) {
    std::cout << buf.getAt(i);
  }
  std::cout << "\n\n";

  // Example 5: Append more data to demonstrate block creation
  std::string long_string(2000, 'X'); // This will create multiple blocks
  buf.append(long_string.data(), long_string.size());

  std::cout << "Total buffer size: " << buf.size() << " bytes\n";

  return 0;
}