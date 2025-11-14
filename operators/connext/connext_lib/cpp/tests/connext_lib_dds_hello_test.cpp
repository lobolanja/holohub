#include "connext_lib/dds_hello.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
  const std::string payload = "Holohub DDS hello";
  if (!connext_lib::dds_hello_world_roundtrip(payload)) {
    std::cerr << "Failed to complete DDS hello world roundtrip" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "DDS hello world roundtrip succeeded" << std::endl;
  return EXIT_SUCCESS;
}

