# Connext Operator Refactoring Plan

**Status:** In Progress  
**Branch:** `dev/demo-ano-example`  
**Last Updated:** February 5, 2026

## Completed ✅

### Step 1: Fix Critical Memory Safety Issues (DONE)

#### 1.1 Lambda Deleter Lifetime Fix - `connext_rx.cpp` ✅
- **Issue:** Raw pointer capture in lambda could cause use-after-free when tensor outlives operator
- **Location:** `operators/connext/connext_ops/cpp/src/connext_rx.cpp:132-147`
- **Fix Applied:** Replaced `ano_reader_.get()` with `std::shared_ptr` using aliasing constructor
- **Result:** Lambda now safely extends `ConnextANOReader` lifetime until all tensors destroyed
- **Tests:** All 12 tests passing

#### 1.2 ANOPayloadWriter Destructor - Simplified ✅
- **Issue:** TODO comment suggested GPU memory leak, but writer never owns `staged_gpu_ptr_`
- **Location:** `operators/connext/connext_lib/cpp/src/transport/payload_transport_ano.cpp:29-32`
- **Fix Applied:** Simplified destructor to empty implementation with clarifying comment
- **Rationale:** `staged_gpu_ptr_` is always borrowed from `setBuffer()` - caller owns it
- **Tests:** All tests passing

---

## Pending High Priority 🔴

### Step 2: Extract Methods from `ConnextRxOp::compute()` (95 lines → ≤20 per method)

**Severity:** HIGH (SRP violation)  
**File:** `operators/connext/connext_ops/cpp/src/connext_rx.cpp:97-189`  
**Estimated Time:** 2-3 hours

#### Current Problems:
- Single method handles 5 responsibilities:
  1. Receives data from transport (DDS or ANO)
  2. Creates GXF entities
  3. Manages memory allocation
  4. Wraps tensors with custom deleters
  5. Handles different memory types (CPU vs GPU)
- Difficult to test each concern independently
- 95 lines violates ≤20 line guideline

#### Proposed Refactoring:

```cpp
// Extract to:
private:
  MemoryBufferView receiveBuffer();
  nvidia::gxf::Entity createTensorEntity(ExecutionContext& context);
  void wrapGpuMemory(nvidia::gxf::Tensor& tensor, const MemoryBufferView& buffer);
  void wrapCpuMemory(nvidia::gxf::Tensor& tensor, const MemoryBufferView& buffer);
```

#### Implementation Plan:
1. Extract `receiveBuffer()` - transport agnostic receive logic
2. Extract `createTensorEntity()` - entity/tensor creation
3. Extract `wrapGpuMemory()` - ANO zero-copy wrapping with shared_ptr lifetime
4. Extract `wrapCpuMemory()` - DDS copy-to-vector wrapping
5. Update `compute()` to orchestrate extracted methods
6. Add unit tests for each extracted method

#### Success Criteria:
- `compute()` ≤ 20 lines
- Each extracted method has single responsibility
- All existing tests pass
- Add 4+ new unit tests for extracted methods

---

### Step 3: Implement Transport Factory Pattern

**Severity:** HIGH (OCP violation)  
**Files:** 
- `operators/connext/connext_ops/cpp/src/connext_rx.cpp:67-78`
- `operators/connext/connext_ops/cpp/src/connext_tx.cpp:72-83`  
**Estimated Time:** 4-6 hours

#### Current Problems:
- Long if-else chains for transport selection in both RX and TX
- Adding new transport requires modifying operator code
- Not open for extension
- Duplicated validation logic

#### Proposed Design:

```cpp
// New interfaces
class ITransportReader {
public:
  virtual ~ITransportReader() = default;
  virtual MemoryBufferView readSamples() = 0;
  virtual void freeBuffer(const MemoryBufferView& buffer) = 0;
};

class ITransportWriter {
public:
  virtual ~ITransportWriter() = default;
  virtual void setBuffer(const MemoryBufferView& buffer) = 0;
  virtual size_t broadcast() = 0;
};

class TransportReaderFactory {
public:
  static std::unique_ptr<ITransportReader> create(
    const DdsConfig& dds_config,
    const AnoConfig& ano_config,
    std::chrono::milliseconds poll_interval
  );
};

class TransportWriterFactory {
public:
  static std::unique_ptr<ITransportWriter> create(
    const DdsConfig& dds_config,
    const AnoConfig& ano_config,
    std::chrono::milliseconds poll_interval
  );
};
```

#### Implementation Plan:
1. Create `ITransportReader` and `ITransportWriter` interfaces
2. Implement adapter classes wrapping existing `ConnextDDSReader` and `ConnextANOReader`
3. Create factory classes with transport validation logic
4. Update `ConnextRxOp::start()` to use factory
5. Update `ConnextTxOp::start()` to use factory
6. Remove if-else chains from operators
7. Add factory unit tests with mock transports

#### Success Criteria:
- No transport-specific if-else in operator classes
- Factory handles all validation logic
- Easy to add new transports without modifying operators
- All existing tests pass
- Add 6+ factory unit tests

---

### Step 4: Apply Dependency Injection to Constructors

**Severity:** HIGH (DIP violation)  
**Files:**
- `operators/connext/connext_lib/cpp/src/comm/connext_writers.cpp:15-20`
- `operators/connext/connext_lib/cpp/src/comm/connext_readers.cpp:42-60`  
**Estimated Time:** 3-4 hours

#### Current Problems:
- Constructors create their own dependencies
- Cannot inject mock implementations for testing
- Tightly coupled to concrete DDS types
- Untestable without real DDS infrastructure

#### Example Problem:
```cpp
ConnextANOWriter::ConnextANOWriter(const AnoConfig& ano_config, /*...*/) {
  int domain_id = dds_config.domain_id();
  dds::domain::DomainParticipant dp(domain_id);  // ❌ Hardcoded dependency
  auto sender_manager = std::make_unique<DdsSenderResourcesManager>(dp, /*...*/);
  // ...
}
```

#### Proposed Refactoring:
```cpp
ConnextANOWriter::ConnextANOWriter(
    const AnoConfig& ano_config,
    const DdsConfig& dds_config,
    std::unique_ptr<SenderResourcesManagerInterface> sender_manager,
    std::unique_ptr<PayloadWriterInterface> payload_writer,
    std::chrono::milliseconds poll_interval
) : sender_manager_(std::move(sender_manager)),
    payload_writer_(std::move(payload_writer)) {
  // No dependency creation - pure initialization
}

// Factory function for production use
static std::unique_ptr<ConnextANOWriter> createProductionWriter(
    const AnoConfig& ano_config,
    const DdsConfig& dds_config,
    std::chrono::milliseconds poll_interval
) {
  auto dp = std::make_shared<dds::domain::DomainParticipant>(dds_config.domain_id());
  auto sender_manager = std::make_unique<DdsSenderResourcesManager>(dp, ano_config);
  auto payload_writer = MakeANOPayloadWriter(ano_config);
  return std::make_unique<ConnextANOWriter>(
      ano_config, dds_config, 
      std::move(sender_manager), 
      std::move(payload_writer), 
      poll_interval
  );
}
```

#### Implementation Plan:
1. Update `ConnextANOWriter` constructor to accept interfaces
2. Update `ConnextDDSWriter` constructor similarly
3. Update `ConnextANOReader` constructor to accept interfaces
4. Update `ConnextDDSReader` constructor similarly
5. Create factory functions for production use
6. Update operator usage to use factories
7. Add unit tests with mock dependencies

#### Success Criteria:
- All constructors accept dependencies as parameters
- Zero dependency creation in constructors
- Factory functions provided for production use
- All existing integration tests pass
- Add 8+ unit tests with mock dependencies

---

## Pending Medium Priority 🟡

### Step 5: Standardize Error Handling

**Severity:** MEDIUM (Inconsistency)  
**Files:** Multiple  
**Estimated Time:** 4-5 hours

#### Current Problems:
- Inconsistent error handling across codebase:
  - Some functions throw exceptions
  - Others return `bool`
  - Others return `std::optional`
  - Some swallow exceptions silently
- No documented error handling strategy

#### Examples:

**Silent Failures:**
```cpp
// connext_rx.cpp:107-110
auto entity_expected = input.receive<gxf::Entity>("input");
if (!entity_expected) { return; }  // ❌ Silent failure

auto tensor = entity_expected.value().get<Tensor>("payload");
if (!tensor) { return; }  // ❌ Silent failure
```

**Exception Swallowing:**
```cpp
// payload_transport_ano.cpp:73-90
try {
  sender->send(staged_gpu_ptr_, staged_size_);
} catch (const std::exception& e) {
  HOLOSCAN_LOG_WARN(/* ... */);
  return false;  // ❌ Exception swallowed, caller can't distinguish errors
}
```

#### Proposed Strategy:

**Error Handling Guidelines:**
1. **Programming errors** (contract violations) → Throw exceptions
2. **Recoverable errors** (transient failures) → Return `Result<T, Error>` or `std::expected<T, E>` (C++23)
3. **Optional data** (not an error) → Return `std::optional<T>`
4. **Always log errors** before returning/throwing

#### Implementation Plan:
1. Document error handling strategy in `README.md`
2. Add `Result<T, E>` type (or use `std::expected` if C++23 available)
3. Update silent failures to log errors
4. Convert exception-swallowing to proper error returns
5. Standardize method signatures across codebase
6. Add error handling tests

#### Success Criteria:
- Documented error handling strategy
- No silent failures in codebase
- Consistent return types for error conditions
- All error paths logged
- Add 10+ error handling tests

---

### Step 6: Fix Interface Segregation Violation

**Severity:** MEDIUM (ISP violation)  
**File:** `operators/connext/connext_lib/cpp/include/connext_lib/transport/payload_transport.hpp:48-61`  
**Estimated Time:** 2-3 hours

#### Current Problem:
```cpp
class PayloadWriterInterface {
  virtual void setBuffer(const MemoryBufferView& buffer) = 0;
  virtual bool writeTo(const std::string& destination_reference) = 0;
  virtual int flush(int timeout_ms = 1000) = 0;  // ❌ DDS doesn't need this
};
```

- DDS implementation doesn't need `flush()` (writes are synchronous)
- Forces DDS to implement no-op method
- Mixed synchronous/asynchronous interface

#### Proposed Refactoring:
```cpp
class PayloadWriterInterface {
  virtual void setBuffer(const MemoryBufferView& buffer) = 0;
  virtual bool writeTo(const std::string& destination_reference) = 0;
  virtual ~PayloadWriterInterface() = default;
};

class FlushableWriterInterface : public PayloadWriterInterface {
  virtual int flush(int timeout_ms = 1000) = 0;
};

// ANO uses FlushableWriterInterface
class ANOPayloadWriter : public FlushableWriterInterface { /* ... */ };

// DDS uses base PayloadWriterInterface
class DdsPayloadWriter : public PayloadWriterInterface { /* ... */ };
```

#### Implementation Plan:
1. Create `FlushableWriterInterface` extending base interface
2. Update `ANOPayloadWriter` to implement `FlushableWriterInterface`
3. Update `DdsPayloadWriter` to implement base interface only
4. Update code using `flush()` to check interface type or handle optional
5. Add tests for both interface types

#### Success Criteria:
- DDS writer not forced to implement `flush()`
- ANO writer properly implements flushable interface
- All existing tests pass
- Add 4+ interface tests

---

### Step 7: Refactor `ReceiverConfig` God Object

**Severity:** MEDIUM (SRP violation)  
**File:** `operators/connext/connext_ano_lib/cpp/include/connext_ano_lib/receiver_config.h`  
**Estimated Time:** 3-4 hours

#### Current Problem:
`ReceiverConfig` has 10 configuration fields with mixed concerns:
- Network addressing: `ip`, `mac`, `port`
- Packet parameters: `header_size`, `max_packet_size`
- Hardware config: `gpu_device_id`, `queue_id`
- Transmission mode: `send_mode`

#### Proposed Refactoring:
```cpp
struct NetworkAddress {
  std::string ip;
  std::string mac;
  uint16_t port;
};

struct PacketConfig {
  uint32_t header_size;
  uint32_t max_packet_size;
};

struct HardwareConfig {
  int gpu_device_id;
  int queue_id;
  SendMode send_mode;
};

struct ReceiverConfig {
  NetworkAddress network;
  PacketConfig packet;
  HardwareConfig hardware;
  
  // Validation moved to respective structs
};
```

#### Implementation Plan:
1. Create `NetworkAddress`, `PacketConfig`, `HardwareConfig` structs
2. Add validation methods to each struct
3. Update `ReceiverConfig` to compose these structs
4. Update all usage sites
5. Add validation tests for each struct

#### Success Criteria:
- Clear separation of concerns
- Validation logic in appropriate structs
- All existing tests pass
- Add 6+ validation tests

---

### Step 8: Extract Magic Numbers to Named Constants

**Severity:** MEDIUM (Maintainability)  
**Files:** Multiple  
**Estimated Time:** 2 hours

#### Examples:
```cpp
// connext_rx.cpp:76
constexpr std::chrono::milliseconds kAnoReaderPollInterval{100};  // ✅ Good

// payload_transport_ano.cpp:182
std::this_thread::sleep_for(std::chrono::milliseconds(5));  // ❌ Magic number

// gpu_direct_network_sender.cpp:16
#define MAX_PACKET_SIZE 9000  // ❌ Magic number without rationale
```

#### Implementation Plan:
1. Identify all magic numbers in codebase
2. Create constants header file or config class
3. Document rationale for each constant
4. Replace magic numbers with named constants
5. Update tests

#### Success Criteria:
- No unexplained numeric literals
- All constants documented with rationale
- Centralized configuration
- All tests pass

---

## Pending Low Priority 🟢

### Step 9: Const Correctness Review

**Severity:** LOW (Code quality)  
**Files:** Multiple  
**Estimated Time:** 2-3 hours

#### Examples:
```cpp
// Methods that don't modify state but aren't const
void setBuffer(const MemoryBufferView& buffer);  // Could be const?
bool sendTo(const std::string& destination_reference);  // Reads only?

// Parameters that should be const
void send(void* gpu_data, size_t size);  // Should be const void*
```

#### Implementation Plan:
1. Review all methods for const correctness
2. Add const to non-mutating methods
3. Add const to pointer parameters that aren't modified
4. Update tests

#### Success Criteria:
- All read-only methods marked const
- All non-modified pointers marked const
- Better compiler optimization opportunities
- All tests pass

---

### Step 10: Modernize Code Style

**Severity:** LOW (Polish)  
**Files:** Multiple  
**Estimated Time:** 1-2 hours

#### Changes:
1. Replace `(void)param` with `[[maybe_unused]]` attribute
2. Standardize include guards to `#pragma once`
3. Unify comment styles (Doxygen vs regular)
4. Standardize naming conventions per guidelines

#### Example:
```cpp
// Before:
void compute(InputContext& input, OutputContext& output, ExecutionContext& context) {
  (void)input;
  (void)context;
  // ...
}

// After:
void compute([[maybe_unused]] InputContext& input, 
             OutputContext& output, 
             [[maybe_unused]] ExecutionContext& context) {
  // ...
}
```

---

### Step 11: Add Missing Documentation

**Severity:** LOW (Documentation)  
**Files:** All headers  
**Estimated Time:** 3-4 hours

#### Missing Documentation:
1. **Lifetime contracts** for `MemoryBufferView`
2. **Thread safety** guarantees for all public APIs
3. **Precondition behaviors** for interface methods
4. **Example usage** in class headers

#### Example Addition:
```cpp
/**
 * @brief Non-owning view into memory buffer
 * 
 * Lifetime contract:
 * - Writer: Caller must keep buffer alive during broadcast()
 * - Reader: Caller MUST call freeBuffer() to prevent leaks
 * 
 * @warning Buffer remains valid only until freeBuffer() is called
 * 
 * @threading Not thread-safe. Use from single thread only.
 */
struct MemoryBufferView {
  void* ptr;
  std::size_t size_bytes;
  bool is_device;
};
```

---

### Step 12: Add Missing Test Coverage

**Severity:** LOW (Testing)  
**Estimated Time:** 4-5 hours

#### Missing Coverage:
1. Error handling paths (CUDA failures, network errors)
2. Edge cases (zero-size payloads, max payload sizes)
3. Lifecycle management (start/stop/restart sequences)
4. Memory leak verification (valgrind/sanitizers)
5. Multi-threaded scenarios
6. Performance benchmarks

#### Implementation Plan:
1. Add error injection tests
2. Add edge case tests
3. Add lifecycle tests
4. Add memory leak detection tests
5. Add thread safety tests
6. Add performance benchmarks

#### Success Criteria:
- 90%+ code coverage
- All error paths tested
- All edge cases covered
- No memory leaks detected
- Thread safety verified
- Performance baselines established

---

## Testing Strategy

### Unit Tests
- Test each extracted method independently
- Use mock dependencies for isolation
- Focus on single responsibility
- Fast execution (< 1ms per test)

### Integration Tests
- Test transport roundtrips (writer → reader)
- Test multi-node scenarios (Docker compose)
- Test error recovery (network failures)
- Medium execution (< 5s per test)

### Performance Tests
- Baseline throughput measurements
- Latency under load
- Memory usage profiling
- GPU utilization metrics

---

## Implementation Order Recommendation

**Sprint 1 (High Priority - Week 1-2):**
1. Step 2: Extract methods from `compute()` 
2. Step 3: Implement transport factory pattern

**Sprint 2 (High Priority - Week 3-4):**
3. Step 4: Apply dependency injection
4. Step 5: Standardize error handling

**Sprint 3 (Medium Priority - Week 5-6):**
5. Step 6: Fix ISP violation
6. Step 7: Refactor `ReceiverConfig` 
7. Step 8: Extract magic numbers

**Sprint 4 (Low Priority - Week 7-8):**
8. Step 9: Const correctness review
9. Step 10: Modernize code style
10. Step 11: Add documentation
11. Step 12: Expand test coverage

---

## Success Metrics

### Code Quality Metrics:
- [ ] Average method length ≤ 20 lines
- [ ] Cyclomatic complexity ≤ 10 per method
- [ ] Zero SOLID principle violations
- [ ] Code coverage ≥ 90%
- [ ] Zero memory leaks (valgrind clean)
- [ ] Zero data races (thread sanitizer clean)

### Maintainability Metrics:
- [ ] All public APIs documented
- [ ] All error paths tested
- [ ] Consistent naming conventions
- [ ] Consistent error handling strategy
- [ ] All magic numbers removed

### Performance Metrics:
- [ ] Baseline throughput: ≥ XX GB/s (to be measured)
- [ ] Baseline latency: ≤ XX μs (to be measured)
- [ ] Zero performance regression vs original
- [ ] GPU utilization: ≥ 90% (for GPU-bound workloads)

---

## Notes

### Architecture Decisions:
- Zero-copy design for ANO transport (GPU Direct)
- Copy-to-vector for DDS transport (DDS requires host memory)
- RAII for all resource management
- Factory pattern for transport abstraction
- Dependency injection for testability

### Known Limitations:
- Currently supports only one transport at a time (DDS OR ANO)
- ANO requires NVIDIA ConnectX NIC with GPUDirect capability
- DDS transport copies data to host memory (not zero-copy)

### Future Enhancements:
- Multi-transport support (DDS + ANO simultaneously)
- Dynamic transport switching at runtime
- Transport load balancing
- Automatic failover between transports

---

## References

- Original Analysis: Code Quality Analysis Report (February 5, 2026)
- SOLID Principles: https://en.wikipedia.org/wiki/SOLID
- Holoscan SDK: https://github.com/nvidia-holoscan/holoscan-sdk
- RTI Connext DDS: https://www.rti.com/products/connext-dds-professional
- .ia Coding Guidelines: `applications/connext/.ia/COPILOT.md`
- Test Anti-patterns: `applications/connext/.ia/skills/writing-tests/anti-patterns.md`
