# connext_lib — Connext Support Library

Lightweight C++ support library providing RTI Connext DDS and ANO helpers for Holoscan applications.

This library exposes a small, stable public API for configuring transports, managing sender/receiver
resources, and sending/receiving raw payload buffers. It is intended to be linked by native
operators and applications that need to integrate DDS-based discovery/announcements with a
high-performance ANO (Advanced Network Objects) payload transport.

**Key features**
- Simple configuration types for DDS and ANO: `DdsConfig`, `AnoConfig`
- Generic transport abstractions: `PayloadTransport`, `PayloadWriterInterface`, `PayloadReaderInterface`
- Resource management and discovery helpers for senders/receivers
- High-level reader/writer wrappers used by tests and example operators (`ConnextDDSWriter`, `ConnextDDSReader`, `ConnextANOWriter`, `ConnextANOReader`)

**Public header**: include `connext_lib.hpp` (see `cpp/include/connext_lib.hpp`).

**Quick note on licensing and prerequisites**
- The library is licensed Apache-2.0. Many source files include SPDX headers indicating this.
- RTI Connext DDS SDK is required to build and run DDS-enabled components. Ensure `NDDSHOME` points
	to the RTI SDK and `RTI_LICENSE_FILE` is set to a valid RTI license file.

--

**Architecture and important headers**

- `cpp/include/connext_lib/config/config.hpp` — `DdsConfig` and `AnoConfig` types and defaults.
- `cpp/include/connext_lib/transport/payload_transport.hpp` — transport interfaces, `PayloadBufferView`, writer/reader interfaces.
- `cpp/include/connext_lib/resource/resource_managers.hpp` — sender/receiver resource manager interfaces and base classes.
- `cpp/include/connext_lib/comm/connext_writers.hpp` — high-level writer classes: `ConnextANOWriter`, `ConnextDDSWriter`.
- `cpp/include/connext_lib/comm/connext_readers.hpp` — high-level reader classes: `ConnextANOReader`, `ConnextDDSReader`.

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
	connext_lib::DdsConfig dds_config(true, /*enabled*/ 0, /*domain_id*/ 0, /*topic_name*/ "HWTopic", /*topic_type_name*/ "BytesTopicType");

	// 2) Create writer and reader (poll interval used by reader for discovery loops)
	std::chrono::milliseconds poll_interval(100);
	connext_lib::ConnextDDSWriter writer(dds_config, /*max_payload_bytes*/ 1024);
	connext_lib::ConnextDDSReader reader(dds_config, poll_interval);

	// 3) Allow discovery to complete (simple sleep for demo purposes)
	std::this_thread::sleep_for( std::chrono::seconds(2) );

	// 4) Prepare payload and send
	const std::string msg = "Hello from connext_lib!";
	connext_lib::PayloadBufferView buffer{ reinterpret_cast<const std::uint8_t*>(msg.data()), msg.size() };
	std::size_t sent = writer.broadcast(buffer);

	// sent > 0 indicates the writer had at least one destination to send to
	if (sent == 0) {
		// No discovered destinations — in tests this typically means discovery hasn't finished or
		// no peer was started. For production, add retries or handle this case appropriately.
	}

	// 5) Poll reader for the message
	std::vector<std::uint8_t> samples;
	const int max_attempts = 30;
	for (int i = 0; i < max_attempts; ++i) {
		samples = reader.readSamples();
		if (!samples.empty()) break;
		std::this_thread::sleep_for(poll_interval);
	}

	if (!samples.empty()) {
		std::string received(samples.begin(), samples.end());
		// Verify or process 'received'
	}

	return 0;
}
```

Notes:
- The code above is a minimal demo; the library's tests show the same pattern with small refinements
	around timing and test harness integration.
- For ANO (RDMA/zero-copy) usage, use `AnoConfig` and `ConnextANOWriter` / `ConnextANOReader`.

--

Building and tests

See the parent `operators/connext/README.md` for full build prerequisites and examples. 

--

Where to look next

- `cpp/tests/connext_lib_public_api_tests.cpp` — executable documentation for the public API.
- `cpp/include/connext_lib.hpp` — the single public header for users linking against the library.
- `cpp/src/` — implementation files for transport, resource managers, and comm helpers.

If you want, I can: (a) add a short Troubleshooting section with common RTI errors, or (b) add
an ANO-specific hello-world example. You previously requested a single simple hello-world, so I
kept the README focused and concise.

