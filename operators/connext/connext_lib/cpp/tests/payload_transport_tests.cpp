#include "connext_lib/transport/payload_transport.hpp"

#include <array>
#include <chrono>
#include <string>
#include <vector>

#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {

using namespace std::chrono_literals;

class FakePayloadWriter : public connext_lib::PayloadWriterInterface {
 public:
  void setBuffer(const connext_lib::PayloadBufferView& buffer) override {
    lastBuffer_ = buffer;
  }

  bool writeTo(const std::string& destination_reference) override {
    lastDestination_ = destination_reference;
    return lastBuffer_.device_ptr != nullptr && lastBuffer_.size_bytes > 0;
  }

  const connext_lib::PayloadBufferView& lastBuffer() const {
    return lastBuffer_;
  }

  const std::string& lastDestination() const { return lastDestination_; }

 private:
    connext_lib::PayloadBufferView lastBuffer_{};
    std::string lastDestination_;
};

class FakePayloadReader : public connext_lib::PayloadReaderInterface {
 public:
  explicit FakePayloadReader(std::vector<std::uint8_t> canned_payload)
      : cannedPayload_(std::move(canned_payload)) {}

  bool readNext(void*& data_ptr, std::size_t& size,
                std::chrono::milliseconds timeout) override {
    (void)timeout;
    if (cannedPayload_.empty()) {
      data_ptr = nullptr;
      size = 0;
      return false;
    }
    uint8_t* buf = new uint8_t[cannedPayload_.size()];
    std::memcpy(buf, cannedPayload_.data(), cannedPayload_.size());
    data_ptr = static_cast<void*>(buf);
    size = cannedPayload_.size();
    return true;
  }

  void freeData(void* data_ptr) override {
    if (!data_ptr) return;
    auto buf = static_cast<uint8_t*>(data_ptr);
    delete[] buf;
  }

 private:
  std::vector<std::uint8_t> cannedPayload_;
};

class FakePayloadTransport : public connext_lib::PayloadTransport {
 public:
  std::unique_ptr<connext_lib::PayloadWriterInterface> createWriter(
      const connext_lib::PayloadWriterOptions& options) override {
    lastWriterOptions_ = options;
    return std::make_unique<FakePayloadWriter>();
  }

  std::unique_ptr<connext_lib::PayloadReaderInterface> createReader(
      const connext_lib::PayloadReaderOptions& options) override {
    lastReaderOptions_ = options;
    std::vector<std::uint8_t> canned = {1, 2, 3};
    return std::make_unique<FakePayloadReader>(std::move(canned));
  }

  const connext_lib::PayloadWriterOptions& lastWriterOptions() const {
    return lastWriterOptions_;
  }
  const connext_lib::PayloadReaderOptions& lastReaderOptions() const {
    return lastReaderOptions_;
  }

 private:
  connext_lib::PayloadWriterOptions lastWriterOptions_;
  connext_lib::PayloadReaderOptions lastReaderOptions_;
};

class PayloadTransportTester : public rti::test::Tester,
                               public rti::test::Singleton<PayloadTransportTester> {
 public:
  /// Ensure the light-weight view simply aliases caller-owned memory.
  void payloadBufferViewRetainsPointer() {
    std::array<std::uint8_t, 4> buffer{{0x01, 0x02, 0x03, 0x04}};
    void* raw_ptr = static_cast<void*>(buffer.data());
    connext_lib::PayloadBufferView view;
    view.device_ptr = nullptr;
    view.size_bytes = buffer.size();
    view.data = static_cast<const std::uint8_t*>(raw_ptr);
    RTI_TEST_ASSERT(view.data == raw_ptr);
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(buffer.size()),
                   static_cast<int>(view.size_bytes));
  }

  /// Validate struct defaults and setters before transports consume them.
  void payloadWriterAndReaderOptionDefaults() {
    connext_lib::PayloadWriterOptions writer_opts;
    RTI_TEST_ASSERT(writer_opts.channel.empty());
    RTI_TEST_ASSERT_EQUALS_INT(0, static_cast<int>(writer_opts.max_payload_bytes));

    writer_opts.channel = "topic";
    writer_opts.max_payload_bytes = 4096;
    RTI_TEST_ASSERT(writer_opts.channel == "topic");
    RTI_TEST_ASSERT_EQUALS_INT(4096,
                               static_cast<int>(writer_opts.max_payload_bytes));

    connext_lib::PayloadReaderOptions reader_opts;
    RTI_TEST_ASSERT(reader_opts.channel.empty());
    RTI_TEST_ASSERT_EQUALS_INT(
        0, static_cast<int>(reader_opts.expected_payload_bytes));

    reader_opts.channel = "topic";
    reader_opts.expected_payload_bytes = 512;
    RTI_TEST_ASSERT(reader_opts.channel == "topic");
    RTI_TEST_ASSERT_EQUALS_INT(
        512, static_cast<int>(reader_opts.expected_payload_bytes));
  }

  /// Writers must cache payload bytes and propagate destination metadata.
  void payloadWriterInterfaceStagesAndSends() {
    FakePayloadWriter writer;
    std::array<std::uint8_t, 2> data{{0xAA, 0x55}};
    connext_lib::PayloadBufferView view;
    view.data = data.data();
    view.size_bytes = data.size();
    view.device_ptr = const_cast<void*>(static_cast<const void*>(data.data()));
    writer.setBuffer(view);
    RTI_TEST_ASSERT(writer.lastBuffer().data == data.data());
    RTI_TEST_ASSERT_EQUALS_INT(
      static_cast<int>(data.size()),
      static_cast<int>(writer.lastBuffer().size_bytes));
    RTI_TEST_ASSERT(writer.writeTo("receiver"));
    RTI_TEST_ASSERT(writer.lastDestination() == "receiver");
  }

  /// Readers surface buffered payloads into caller-provided storage.
  void payloadReaderInterfaceDeliversPayload() {
    std::vector<std::uint8_t> canned{0x0A, 0x0B};
    FakePayloadReader reader(canned);
    void* data_ptr = nullptr;
    std::size_t size = 0;
    RTI_TEST_ASSERT(reader.readNext(data_ptr, size, 10ms));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(canned.size()),
                               static_cast<int>(size));
    auto buf = static_cast<uint8_t*>(data_ptr);
    RTI_TEST_ASSERT(buf[0] == canned[0]);
    RTI_TEST_ASSERT(buf[1] == canned[1]);
    reader.freeData(data_ptr);
  }

  /// Factories should keep a copy of option structs for diagnostics.
  void payloadTransportPropagatesOptions() {
    FakePayloadTransport transport;
    connext_lib::PayloadWriterOptions writer_opts;
    writer_opts.channel = "dds_topic";
    writer_opts.max_payload_bytes = 2048;
    auto writer = transport.createWriter(writer_opts);
    RTI_TEST_ASSERT(writer != nullptr);
    RTI_TEST_ASSERT(transport.lastWriterOptions().channel == "dds_topic");
    RTI_TEST_ASSERT_EQUALS_INT(
        2048,
        static_cast<int>(transport.lastWriterOptions().max_payload_bytes));

    connext_lib::PayloadReaderOptions reader_opts;
    reader_opts.channel = "dds_topic";
    reader_opts.expected_payload_bytes = 2048;
    auto reader = transport.createReader(reader_opts);
    RTI_TEST_ASSERT(reader != nullptr);
    RTI_TEST_ASSERT(transport.lastReaderOptions().channel == "dds_topic");
    RTI_TEST_ASSERT_EQUALS_INT(
        2048,
        static_cast<int>(transport.lastReaderOptions().expected_payload_bytes));
  }

 private:
  PayloadTransportTester() : rti::test::Tester("connext_lib_payload_transport_tests") {
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payloadBufferViewRetainsPointer);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payloadWriterAndReaderOptionDefaults);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payloadWriterInterfaceStagesAndSends);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payloadReaderInterfaceDeliversPayload);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payloadTransportPropagatesOptions);
  }

  friend class rti::test::Singleton<PayloadTransportTester>;
};

class PayloadTransportTestContainer
    : public rti::test::TesterContainer,
      public rti::test::Singleton<PayloadTransportTestContainer> {
 private:
  PayloadTransportTestContainer()
      : rti::test::TesterContainer("connext_lib_payload_transport") {
    add_tester<PayloadTransportTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<PayloadTransportTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return PayloadTransportTestContainer::get_instance().run_tests(argc, argv);
}
