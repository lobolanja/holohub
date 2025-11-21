#pragma once

#include <string>

namespace connext_lib {
bool dds_hello_world_roundtrip(const std::string& message = "Hello from RTI Connext",
                               int domain_id = 0);
}  // namespace connext_lib
