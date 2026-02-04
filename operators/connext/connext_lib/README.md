# connext_lib — Connext Support Library

Lightweight C++ support library providing RTI Connext DDS and ANO helpers for Holoscan applications.

This library exposes a small, stable public API for configuring transports, managing sender/receiver
resources, and sending/receiving raw payload buffers. It is intended to be linked by native
operators and applications that need to integrate DDS-based discovery/announcements with a
high-performance ANO (Advanced Network Objects) payload transport.

**Key features**
- Simple configuration types for DDS and ANO: `DdsConfig`, `AnoConfig`, `AnoNetworkConfig`
- Memory buffer abstraction: `MemoryBufferView` (supports both CPU and GPU memory)
- High-level reader/writer interfaces: `ConnextDDSWriter`, `ConnextDDSReader`, `ConnextANOWriter`, `ConnextANOReader`
- Automatic buffer lifetime management with `readSamples()` / `freeBuffer()` pattern
- Transport-agnostic payload handling with unified API

**Public header**: include `connext_lib.hpp` (see `cpp/include/connext_lib.hpp`).

**Quick note on licensing and prerequisites**
- The library is licensed Apache-2.0. Many source files include SPDX headers indicating this.
- RTI Connext DDS SDK is required to build and run DDS-enabled components. Ensure `NDDSHOME` points
	to the RTI SDK and `RTI_LICENSE_FILE` is set to a valid RTI license file.

--

**Architecture and important headers**

- `cpp/include/connext_lib.hpp` — Single public header providing complete API access with version info.
- `cpp/include/connext_lib/config/config.hpp` — `DdsConfig`, `AnoConfig`, and `AnoNetworkConfig` types.
- `cpp/include/connext_lib/transport/payload_transport.hpp` — `MemoryBufferView` and internal transport interfaces.
- `cpp/include/connext_lib/comm/connext_writers.hpp` — High-level writer classes: `ConnextANOWriter`, `ConnextDDSWriter`.
- `cpp/include/connext_lib/comm/connext_readers.hpp` — High-level reader classes: `ConnextANOReader`, `ConnextDDSReader`.

Refer to the `cpp/` sources and tests for complete usage examples and behavioral expectations.

--

Simple Hello World (DDS roundtrip)

This minimal example demonstrates the typical flow for a DDS-only roundtrip using the
public API. It mirrors the pattern used in the library's public API tests.

```cpp
#include "connext_lib.hpp"
#include <chrono>
#include <string>
#include <thread>

int main() {
	using namespace std::chrono_literals;

	// 1) Configure DDS
	// Domain 0 is commonly used for local testing; pick a unique domain for multi-app tests
	connext_lib::DdsConfig dds_config(
		true,                    // enabled
		0,                       // domain_id
		"HWTopic",              // topic_name
		"BytesTopicType"        // topic_type_name
	);

	// 2) Create writer and reader (poll interval used by reader for discovery loops)
	std::chrono::milliseconds poll_interval(100);
	const std::size_t max_payload_bytes = 1024;
	connext_lib::ConnextDDSWriter writer(dds_config, max_payload_bytes);
	connext_lib::ConnextDDSReader reader(dds_config, poll_interval);

	// 3) Allow discovery to complete (simple sleep for demo purposes)
	std::this_thread::sleep_for(std::chrono::seconds(2));

	// 4) Prepare payload and send
	const std::string msg = "Hello from connext_lib!";
	connext_lib::MemoryBufferView buffer_to_send;
	buffer_to_send.ptr = const_cast<void*>(reinterpret_cast<const void*>(msg.data()));
	buffer_to_send.size_bytes = msg.size();
	buffer_to_send.is_device = false;  // CPU memory

	std::size_t sent = writer.broadcast(buffer_to_send);

	// sent > 0 indicates the writer had at least one destination to send to
	if (sent == 0) {
		// No discovered destinations — in tests this typically means discovery hasn't finished or
		// no peer was started. For production, add retries or handle this case appropriately.
	}

	// 5) Poll reader for the message
	connext_lib::MemoryBufferView received_buffer{nullptr, 0, false};
	const int max_attempts = 30;
	for (int i = 0; i < max_attempts; ++i) {
		received_buffer = reader.readSamples();
		if (received_buffer.ptr != nullptr) break;
		std::this_thread::sleep_for(poll_interval);
	}

	if (received_buffer.ptr != nullptr) {
		auto byte_ptr = static_cast<const std::uint8_t*>(received_buffer.ptr);
		std::string received(byte_ptr, byte_ptr + received_buffer.size_bytes);
		// Verify or process 'received'
		
		// IMPORTANT: Free the buffer after use
		reader.freeBuffer(received_buffer);
	}

	return 0;
}
```

Notes:
- The code above is a minimal demo; the library's tests show the same pattern with small refinements
	around timing and test harness integration.
- `MemoryBufferView` supports both CPU (`is_device=false`) and GPU (`is_device=true`) memory.
- Always call `freeBuffer()` after processing data returned by `readSamples()`.
- For ANO (RDMA/zero-copy) usage, use `AnoConfig` and `ConnextANOWriter` / `ConnextANOReader`.

--

Building and tests

See the parent `operators/connext/README.md` for full build prerequisites and examples. 

--

Where to look next

- `cpp/tests/connext_lib_public_api_tests.cpp` — executable documentation for the public API.
- `cpp/include/connext_lib.hpp` — the single public header providing complete API access.
- `cpp/include/connext_lib/config/config.hpp` — configuration types with detailed field documentation.
- `cpp/src/` — implementation files for transport, resource managers, and comm helpers.

API design notes:
- `readSamples()` returns `MemoryBufferView` which does not own memory; caller must call `freeBuffer()`.
- `MemoryBufferView` replaces the older concept of `PayloadBufferView` and supports both CPU and GPU buffers.
- Writers use `broadcast()` to send to all discovered destinations.
- Configuration objects should be initialized before creating writers/readers (not thread-safe during construction).

