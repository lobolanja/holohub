#include "connext_lib/payload_transport_dds.hpp"

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
    connext_lib::DdsPayloadTransport transport(domain_id());
    connext_lib::PayloadWriterOptions writer_opts;
    writer_opts.channel = UniqueChannel();
    writer_opts.max_payload_bytes = 1024;
    connext_lib::PayloadReaderOptions reader_opts;
    reader_opts.channel = writer_opts.channel;

    auto reader = transport.CreateReader(reader_opts);
    auto writer = transport.CreateWriter(writer_opts);

    // Allow DDS discovery to complete before writing.
    std::this_thread::sleep_for(200ms);

    const std::string message = "dds_payload_roundtrip";
    connext_lib::PayloadBufferView buffer{
        reinterpret_cast<const std::uint8_t*>(message.data()),
        message.size()};
    writer->SetBuffer(buffer);
    RTI_TEST_ASSERT(writer->WriteTo("broadcast"));

    std::vector<std::uint8_t> destination;
    RTI_TEST_ASSERT(reader->ReadNext(destination, 2s));
    const std::string received(destination.begin(), destination.end());
    RTI_TEST_ASSERT(received == message);
  }

 private:
  PayloadTransportDdsTester()
      : rti::test::Tester("connext_lib_payload_transport_dds_tests") {
    RTI_TEST_FUNCTION_ADD(PayloadTransportDdsTester,
                          payload_roundtrip_uses_dds);
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
