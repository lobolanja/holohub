#include "connext_lib/connext_lib.hpp"

#include <cstdlib>
#include <iostream>

int main() {
  const int expected_version = 100;  // Mirrors 0.1.0 placeholder.
  const int reported_version = connext_lib::version();

  if (reported_version != expected_version) {
    std::cerr << "connext_lib::version() returned " << reported_version
              << ", expected " << expected_version << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "connext_lib::version() = " << reported_version << '\n';
  return EXIT_SUCCESS;
}

