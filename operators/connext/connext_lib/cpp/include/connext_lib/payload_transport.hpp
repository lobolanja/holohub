#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace connext_lib {

/** Immutable view into a contiguous payload buffer.
 *
 * Writers copy the data into transport-managed memory, allowing callers to keep
 * lightweight stack allocations while readers reuse an existing buffer.
 */
struct PayloadBufferView {
  const std::uint8_t* data{nullptr};
  std::size_t size_bytes{0};
};

/** Parameters required to construct a payload writer for either DDS or ANO. */
struct PayloadWriterOptions {
  /// Logical channel name (DDS topic or ANO stream identifier).
  std::string channel;
  /// Maximum payload supported by the transport; used to size internal caches.
  std::size_t max_payload_bytes{0};
};

/** Parameters required to construct a payload reader for either DDS or ANO. */
struct PayloadReaderOptions {
  /// Logical channel name (DDS subscription or ANO stream identifier).
  std::string channel;
  /// Optional hint that lets transports pre-allocate their receive buffers.
  std::size_t expected_payload_bytes{0};
};

/** Interface shared by all payload writers (DDS or ANO). */
class PayloadWriterInterface {
 public:
  virtual ~PayloadWriterInterface() = default;

  /** Copies the supplied payload so the writer can reuse it during writeTo(). */
  virtual void setBuffer(const PayloadBufferView& buffer) = 0;

  /** Sends the previously staged payload to the specified destination. */
  virtual bool writeTo(const std::string& destination_reference) = 0;
};

/** Interface shared by all payload readers (DDS or ANO). */
class PayloadReaderInterface {
 public:
  virtual ~PayloadReaderInterface() = default;

  /** Blocks until data arrives or timeout expires and writes bytes into output. */
  virtual bool readNext(std::vector<std::uint8_t>& destination,
                        std::chrono::milliseconds timeout) = 0;
};

/** Factory/owner responsible for creating transport-specific writers/readers. */
class PayloadTransport {
 public:
  virtual ~PayloadTransport() = default;

  virtual std::unique_ptr<PayloadWriterInterface> createWriter(
      const PayloadWriterOptions& options) = 0;

  virtual std::unique_ptr<PayloadReaderInterface> createReader(
      const PayloadReaderOptions& options) = 0;
};

}  // namespace connext_lib
