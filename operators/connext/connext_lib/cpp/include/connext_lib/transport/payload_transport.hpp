#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace connext_lib {
struct PayloadBufferView {
  const std::uint8_t* data{nullptr};
  std::size_t size_bytes{0};
};
struct PayloadWriterOptions {
  std::string channel;
  std::size_t max_payload_bytes{0};
};
struct PayloadReaderOptions {
  std::string channel;
  std::size_t expected_payload_bytes{0};
  std::string buffer_id;
};
class PayloadWriterInterface {
 public:
  virtual ~PayloadWriterInterface() = default;
  virtual void setBuffer(const PayloadBufferView& buffer) = 0;
  virtual bool writeTo(const std::string& destination_reference) = 0;
};
class PayloadReaderInterface {
 public:
  virtual ~PayloadReaderInterface() = default;
  virtual bool readNext(std::vector<std::uint8_t>& destination,
                        std::chrono::milliseconds timeout) = 0;
};
class PayloadTransport {
 public:
  virtual ~PayloadTransport() = default;
  virtual std::unique_ptr<PayloadWriterInterface> createWriter(
      const PayloadWriterOptions& options) = 0;
  virtual std::unique_ptr<PayloadReaderInterface> createReader(
      const PayloadReaderOptions& options) = 0;
};
}  // namespace connext_lib
