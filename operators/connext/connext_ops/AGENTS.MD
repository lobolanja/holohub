## Connext C++ Operators – Development Guide

### Goal
- Recreate the ANO Python operators (`ConnextRx`/`ConnextTx`) in C++ Holoscan operators that rely on `operators/connext/connext_lib/cpp` helpers.
- Provide RX/TX operator nodes that host the endpoint/resource manager lifecycle and payload transport plumbing without duplicating logic already present in `connext_lib`.
- Keep feature parity with `connext_ano` applications so the rest of Holoscan can swap between ANO (Python) and native C++ operators without API changes.

### Milestones
1. **Scaffold**
   - Define the operator directory layout (src/include/tests) that mirrors Holoscan’s other C++ operators.
   - Provide Bazel/CMake glue that links the C++ operators against `connext_lib`.
2. **Operator interfaces**
   - Wrap `ConnextTx`/`ConnextRx` classes from the library into Holoscan operators with clear configuration knobs (participant name, QoS profile paths, channel names).
   - Surface minimal metadata + statistics (payload sizes, throughput) to match `connext_ano`.
3. **Lifecycle integration**
   - Ensure the operators manage DDS initialization/cleanup, thread start/stop, and error propagation using RAII patterns.
   - Expose tracing hooks so Python apps and future diagnostics can reuse them.
4. **Testing + docs**
   - Port existing Python integration tests to C++/gtests where possible, validating DDS publish/subscribe flows with synthetic datasets.
   - Update application READMEs to point to the new operators and document how to select them.

### Coding + Testing Rules
Use the exact standards defined for `operators/connext/connext_lib/cpp` (see `operators/connext/connext_lib/cpp/AGENTS.MD`):
1. **Module order:** follow the layered approach—config plumbing, payload transport usage, resource manager integration, then the actual RX/TX wrappers. Each step must land with unit/integration tests before moving on.
2. **Testing:** rely on RTI’s Unit Test Framework (`librtitest`) for C++ cases, keep tests self-contained, and require `NDDSHOME` + `RTI_LICENSE_FILE` via `.vscode/connext.env`.
3. **Payload transport:** all payload movement runs over DDS until ANO hardware is available—no shared memory fallbacks. Operators should pull/push data via `PayloadTransport` interfaces only.
4. **Resource managers:** use the existing DDS resource manager abstractions; do not add bespoke synchronization beyond what DDS already provides, and expose clean start/stop hooks.
5. **Coding standards:** modern C++17, RAII, Holoscan logging, concise Doxygen comments for non-trivial paths so external contributors can follow along without diving into Python code.
6. **Documentation:** update this file whenever scope or sequencing changes and keep README instructions/test commands in sync with the implemented features.

Sticking to these rules ensures the new C++ operators stay aligned with the library stack and can replace the ANO Python operators seamlessly once fully validated.
