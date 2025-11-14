#pragma once

#include <string>

namespace connext_lib {

/**
 * Runs a local DDS "Hello World" roundtrip using the RTI Connext C++11 API.
 *
 * @param message Payload written to and read from the DDS topic.
 * @param domain_id DDS domain used for the transient writer/reader.
 * @return true if the roundtrip succeeded; false otherwise (e.g., no DDS runtime).
 */
bool dds_hello_world_roundtrip(const std::string& message = "Hello from RTI Connext",
                               int domain_id = 0);

}  // namespace connext_lib
