#include "connext_lib/payload_transport.hpp"

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
  void SetBuffer(const connext_lib::PayloadBufferView& buffer) override {
    last_buffer_ = buffer;
  }

  bool WriteTo(const std::string& destination_reference) override {
    last_destination_ = destination_reference;
    return last_buffer_.data != nullptr && last_buffer_.size_bytes > 0;
  }

  const connext_lib::PayloadBufferView& last_buffer() const {
    return last_buffer_;
  }

  const std::string& last_destination() const { return last_destination_; }

 private:
    connext_lib::PayloadBufferView last_buffer_{};
    std::string last_destination_;
};

class FakePayloadReader : public connext_lib::PayloadReaderInterface {
 public:
  explicit FakePayloadReader(std::vector<std::uint8_t> canned_payload)
      : canned_payload_(std::move(canned_payload)) {}

  bool ReadNext(std::vector<std::uint8_t>& destination,
                std::chrono::milliseconds timeout) override {
    (void)timeout;
    destination = canned_payload_;
    return !destination.empty();
  }

 private:
  std::vector<std::uint8_t> canned_payload_;
};

class FakePayloadTransport : public connext_lib::PayloadTransport {
 public:
  std::unique_ptr<connext_lib::PayloadWriterInterface> CreateWriter(
      const connext_lib::PayloadWriterOptions& options) override {
    last_writer_options_ = options;
    return std::make_unique<FakePayloadWriter>();
  }

  std::unique_ptr<connext_lib::PayloadReaderInterface> CreateReader(
      const connext_lib::PayloadReaderOptions& options) override {
    last_reader_options_ = options;
    std::vector<std::uint8_t> canned = {1, 2, 3};
    return std::make_unique<FakePayloadReader>(std::move(canned));
  }

  const connext_lib::PayloadWriterOptions& last_writer_options() const {
    return last_writer_options_;
  }
  const connext_lib::PayloadReaderOptions& last_reader_options() const {
    return last_reader_options_;
  }

 private:
  connext_lib::PayloadWriterOptions last_writer_options_;
  connext_lib::PayloadReaderOptions last_reader_options_;
};

class PayloadTransportTester : public rti::test::Tester,
                               public rti::test::Singleton<PayloadTransportTester> {
 public:
  /// Ensure the light-weight view simply aliases caller-owned memory.
  void payload_buffer_view_retains_pointer() {
    std::array<std::uint8_t, 4> buffer{{0x01, 0x02, 0x03, 0x04}};
    connext_lib::PayloadBufferView view{buffer.data(), buffer.size()};
    RTI_TEST_ASSERT(view.data == buffer.data());
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(buffer.size()),
                               static_cast<int>(view.size_bytes));
  }

  /// Validate struct defaults and setters before transports consume them.
  void payload_writer_and_reader_option_defaults() {
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
  void payload_writer_interface_stages_and_sends() {
    FakePayloadWriter writer;
    std::array<std::uint8_t, 2> data{{0xAA, 0x55}};
    const connext_lib::PayloadBufferView view{data.data(), data.size()};
    writer.SetBuffer(view);
    RTI_TEST_ASSERT(writer.last_buffer().data == data.data());
    RTI_TEST_ASSERT_EQUALS_INT(
        static_cast<int>(data.size()),
        static_cast<int>(writer.last_buffer().size_bytes));
    RTI_TEST_ASSERT(writer.WriteTo("receiver"));
    RTI_TEST_ASSERT(writer.last_destination() == "receiver");
  }

  /// Readers surface buffered payloads into caller-provided storage.
  void payload_reader_interface_delivers_payload() {
    std::vector<std::uint8_t> canned{0x0A, 0x0B};
    FakePayloadReader reader(canned);
    std::vector<std::uint8_t> destination;
    RTI_TEST_ASSERT(reader.ReadNext(destination, 10ms));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(canned.size()),
                               static_cast<int>(destination.size()));
    RTI_TEST_ASSERT(destination == canned);
  }

  /// Factories should keep a copy of option structs for diagnostics.
  void payload_transport_propagates_options() {
    FakePayloadTransport transport;
    connext_lib::PayloadWriterOptions writer_opts;
    writer_opts.channel = "dds_topic";
    writer_opts.max_payload_bytes = 2048;
    auto writer = transport.CreateWriter(writer_opts);
    RTI_TEST_ASSERT(writer != nullptr);
    RTI_TEST_ASSERT(transport.last_writer_options().channel == "dds_topic");
    RTI_TEST_ASSERT_EQUALS_INT(
        2048,
        static_cast<int>(transport.last_writer_options().max_payload_bytes));

    connext_lib::PayloadReaderOptions reader_opts;
    reader_opts.channel = "dds_topic";
    reader_opts.expected_payload_bytes = 2048;
    auto reader = transport.CreateReader(reader_opts);
    RTI_TEST_ASSERT(reader != nullptr);
    RTI_TEST_ASSERT(transport.last_reader_options().channel == "dds_topic");
    RTI_TEST_ASSERT_EQUALS_INT(
        2048,
        static_cast<int>(transport.last_reader_options().expected_payload_bytes));
  }

 private:
  PayloadTransportTester() : rti::test::Tester("connext_lib_payload_transport_tests") {
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payload_buffer_view_retains_pointer);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payload_writer_and_reader_option_defaults);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payload_writer_interface_stages_and_sends);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payload_reader_interface_delivers_payload);
    RTI_TEST_FUNCTION_ADD(PayloadTransportTester,
                          payload_transport_propagates_options);
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
