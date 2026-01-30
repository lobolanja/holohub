#include "connext_lib/transport/payload_transport_dds.hpp"
#include "ndds/ddsctesthelpers/test_context.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {
using namespace std::chrono_literals;

// Each test run uses a unique topic name to avoid crosstalk with stale entities.
std::string UniqueChannel() {
  static std::atomic<int> counter{0};
  return "connext_lib_payload_" + std::to_string(++counter);
}

int domain_id() {
  // You may want to use a fixed domain or get from environment/config
  return 0;
}

// Helper to setup DDS transport, writer, and reader
struct DdsTransportTestContext {
  dds::domain::DomainParticipant dp_reader;
  dds::domain::DomainParticipant dp_writer;
  connext_lib::DdsPayloadTransport transport_reader;
  connext_lib::DdsPayloadTransport transport_writer;
  connext_lib::PayloadWriterOptions writer_opts;
  connext_lib::PayloadReaderOptions reader_opts;
  std::unique_ptr<connext_lib::PayloadReaderInterface> reader;
  std::unique_ptr<connext_lib::PayloadWriterInterface> writer;
  std::string channel;

  DdsTransportTestContext()
      : dp_reader(domain_id()),
        dp_writer(domain_id()),
        transport_reader(dp_reader),
        transport_writer(dp_writer),
        writer_opts(),
        reader_opts(),
        reader(nullptr),
        writer(nullptr),
        channel(UniqueChannel()) {
    writer_opts.channel = channel;
    writer_opts.max_payload_bytes = 1024;
    reader_opts.channel = channel;
    reader = transport_reader.createReader(reader_opts);
    writer = transport_writer.createWriter(writer_opts);
  }

  // Wait for DDS discovery using helper (1 reader, 5s timeout)
  void wait_for_discovery() {
    // Downcast to DdsPayloadWriter to access get_dds_writer()
    auto* dds_writer = dynamic_cast<connext_lib::DdsPayloadWriter*>(writer.get());
    if (dds_writer) {
      DDSCTestContext_waitForReaders(1, dds_writer->get_dds_writer()->native_writer(), 5);
    } else {
      // Fallback: sleep if downcast fails
      std::this_thread::sleep_for(200ms);
    }
  }
};

class PayloadTransportDdsTester
    : public rti::test::Tester,
      public rti::test::Singleton<PayloadTransportDdsTester> {
 public:
  /**
   * @brief Tests the roundtrip of a payload using DDS transport.
   *
   * This test verifies that a payload can be sent and received correctly using the
   * DDS-based payload transport mechanism provided by connext_lib.
   *
   * Workflow:
   * 1. Create a DDS payload transport instance for the current domain.
   * 2. Configure writer and reader options, including a unique channel and maximum payload size.
   * 3. Instantiate a payload reader and writer using the configured options.
   * 4. Prepare a test message ("dds_payload_roundtrip") and wrap it in a PayloadBufferView.
   * 5. Set the buffer on the writer and write the payload to the "broadcast" channel.
   * 6. Use the reader to read the next available payload within a 2-second timeout.
   * 7. Convert the received payload to a string.
   * 8. Assert that the received message matches the original test message.
   *
   * Expected Result:
   * - The payload is successfully transmitted from the writer to the reader via DDS.
   * - The received message is identical to the sent message, confirming correct roundtrip behavior.
   */
  void payload_roundtrip_uses_dds() {
    // Test that payload is sent and received correctly via DDS
    DdsTransportTestContext ctx;
    ctx.wait_for_discovery();
    const std::string message = "dds_payload_roundtrip";
    connext_lib::PayloadBufferView buffer;
    buffer.data = reinterpret_cast<const std::uint8_t*>(message.data());
    buffer.size_bytes = message.size();
    ctx.writer->setBuffer(buffer);
    RTI_TEST_ASSERT(ctx.writer->writeTo("*"));
    void* data_ptr = nullptr;
    std::size_t size = 0;
    RTI_TEST_ASSERT(ctx.reader->readNext(data_ptr, size, 2s));
    const std::string received = data_ptr && size > 0
      ? std::string(reinterpret_cast<const char*>(data_ptr), size)
      : std::string();
    ctx.reader->freeData(data_ptr);
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(message.size()), static_cast<int>(received.size()));
    RTI_TEST_ASSERT(received == message);
  }

  void payload_read_times_out_if_no_data() {
    // Negative test: reader should time out if no payload is sent
    DdsTransportTestContext ctx;
    ctx.wait_for_discovery();
    void* data_ptr = nullptr;
    std::size_t size = 0;
    RTI_TEST_ASSERT(!ctx.reader->readNext(data_ptr, size, 500ms));
    RTI_TEST_ASSERT(size == 0);
  }

 private:
  PayloadTransportDdsTester()
      : rti::test::Tester("connext_lib_payload_transport_dds_tests") {
    RTI_TEST_FUNCTION_ADD(PayloadTransportDdsTester,
                          payload_roundtrip_uses_dds);
    RTI_TEST_FUNCTION_ADD(PayloadTransportDdsTester,
                          payload_read_times_out_if_no_data);
  }

  friend class rti::test::Singleton<PayloadTransportDdsTester>;
};

class PayloadTransportDdsTestContainer
    : public rti::test::TesterContainer,
      public rti::test::Singleton<PayloadTransportDdsTestContainer> {
 private:
  PayloadTransportDdsTestContainer()
      : rti::test::TesterContainer("connext_lib_payload_transport_dds") {
    add_tester<PayloadTransportDdsTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<PayloadTransportDdsTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return PayloadTransportDdsTestContainer::get_instance().run_tests(argc, argv);
}
