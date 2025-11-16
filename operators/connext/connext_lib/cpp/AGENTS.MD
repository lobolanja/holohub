## Connext C++ Library – Development Guide

### Progress
- Defined payload writer/reader interfaces plus the `PayloadTransport` factory in
  `include/connext_lib/payload_transport.hpp` to bootstrap module 2 (DDS payload
  transport). Next step is to back these interfaces with a DDS implementation.
- Added a concrete DDS payload transport (`payload_transport_dds.hpp/.cpp`) that
  writes raw bytes via `dds::core::BytesTopicType` topics along with integration tests.
- Implemented DDS-oriented resource manager interfaces (`resource_managers.hpp`)
  along with boilerplate polling logic and unit tests so endpoint managers can
  reuse the same abstraction as the Python stack.

This library mirrors the Python `connext_lib` feature set, but all payload transport currently runs over DDS so we can emulate ANO until RDMA hardware is available. Keep these rules in mind while implementing modules:

1. **Module order**
   1. Config layer (`DdsConfig`, `AnoConfig`).
   2. DDS payload transport (replaces shared memory for now). Build writer/reader interfaces that are future‑proof for ANO.
   3. Resource manager interfaces (`ReceiverResourcesManagerInterface`, `AbstractSenderResourcesManager` threading and registration logic).
   4. DDS endpoint managers (topic and discovery modes) built on RTI C++11 API.
   5. `ConnextTx`/`ConnextRx` wrappers that glue endpoint managers to the payload transport.
   6. Top-level readers/writers (Ano vs DDS fallback) and integration glue.
   Each step requires unit/regression tests before moving on.

2. **Testing**
   - Use the RTI Unit Test Framework (`librtitest`) for C++ tests (`connext_lib_tests` binary). Guard tests with proper setup (`RTITestSetting_setupStandalone()`).
   - Keep tests self-contained—DDS resources (participants, topics) must be created and torn down within the test.
   - DDS-based tests require `NDDSHOME` and `RTI_LICENSE_FILE`. Source `.vscode/connext.env` (or load its contents in your IDE launch config) before running `ctest` so the environment is populated.

3. **DDS payload transport**
   - Emulate ANO by writing payloads over DDS topics (one writer, fan-out readers). Provide an interface that can later swap in real ANO transport.
   - All fallbacks rely on DDS until hardware is available; don’t reintroduce shared memory.

4. **Resource managers**
   - Receivers announce buffer references; senders maintain a lightweight map that is filled with the holoscan.discovery properties that are sent in the DDS discovery message as DDS custom properties.
   - For the native implementation we will convey receiver information via DDS QoS properties instead of discovery userdata, and we do **not** need additional thread-safety primitives—the DDS reader/writer instances are already serialized per participant, which keeps the mock implementations simple.
   - Expose hook points so `ConnextTx`/`ConnextRx` can start/stop threads cleanly.

5. **Coding standards**
   - Match existing C++ style: modern C++17, RAII, `std::thread`, `std::mutex`.
   - Keep logging consistent (use the project’s logger facility when available).
   - Document intent using Doxygen-style comments (`/** ... */` or `///`) near non-obvious logic paths so external contributors can follow the design quickly.
   - Keep commentary self-contained—avoid referencing the Python codebase so the C++ library stands on its own.

6. **Documentation**
   - Update this file whenever the plan changes.
   - Ensure README instructions stay in sync with new targets or test commands.

Following this order keeps dependencies manageable, ensures every layer is covered by tests, and allows us to substitute the real ANO transport later without rearchitecting the stack.
