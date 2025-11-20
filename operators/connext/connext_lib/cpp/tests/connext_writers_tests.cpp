#include "connext_lib/connext_writers.hpp"
#include "connext_lib/payload_transport_dds.hpp"
#include "connext_lib/resource_managers_dds.hpp"
#include "connext_lib/config.hpp"
#include "connext_lib/connext_readers.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "dds/dds.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

namespace {
using namespace std::chrono_literals;

class ConnextWritersTester : public rti::test::Tester,
                             public rti::test::Singleton<ConnextWritersTester> {
 public:
  void dds_writer_broadcasts_to_single_receiver() {
    const int domain = domain_id();
    const std::string channel = "writers_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "*";

    // Receiver side (announce resources then prepare payload reader)
    dds::domain::DomainParticipant dp_rx(domain);
    auto payload_reader = std::make_unique<connext_lib::DdsPayloadReader>(
        dp_rx, channel, buffer_id);

    // Sender side (SUT)
    dds::domain::DomainParticipant dp_tx(domain);
    auto payload_writer = std::make_unique<connext_lib::DdsPayloadWriter>(
        dp_tx, channel, 1024);
    connext_lib::ConnextDDSWriter writer(std::move(payload_writer));

    // Allow discovery
    //TODO: change for the test helper that ensures discovery is completed
    std::this_thread::sleep_for(200ms);

    // Stage payload
    const std::string message = "hello_dds_writer_broadcast";
    connext_lib::PayloadBufferView buffer{
        reinterpret_cast<const std::uint8_t*>(message.data()), message.size()};

    // Broadcast until receiver registered
    std::size_t sent = 0;
    for (int attempt = 0; attempt < 80 && sent < 1; ++attempt) {
      sent = writer.broadcast(buffer);
      if (sent < 1) {
          std::this_thread::sleep_for(50ms);
      }
    }
    RTI_TEST_ASSERT_EQUALS_INT(1, static_cast<int>(sent));

    // Receive payload
    std::vector<std::uint8_t> received;
    RTI_TEST_ASSERT(payload_reader->readNext(received, 4s));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(message.size()), static_cast<int>(received.size()));
    std::string received_str(received.begin(), received.end());
    RTI_TEST_ASSERT(received_str == message);
  }

  void ano_writer_broadcast_destination() {
    const int domain = domain_id();
    const std::string channel = "writers_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id_a = "buffer_a";
    const std::string buffer_id_b = "buffer_b";

    connext_lib::AnoConfig ano_a(channel,buffer_id_a,1024,true);
    connext_lib::AnoConfig ano_b(channel,buffer_id_b,1024,true);

    connext_lib::DdsConfig dds_config_a(true, domain, channel, "BytesTopicType");

    // Receiver A
    dds::domain::DomainParticipant dp_rx_a(domain);
    auto receiver_mgr_a = std::make_unique<connext_lib::DdsReceiverResourcesManager>(
        dp_rx_a, buffer_id_a, channel);
    auto reader_a = std::make_unique<connext_lib::DdsPayloadReader>(
        dp_rx_a, "GPU/" + channel, buffer_id_a);
    RTI_TEST_ASSERT(receiver_mgr_a->announce());

    // Receiver B
    dds::domain::DomainParticipant dp_rx_b(domain);
    auto receiver_mgr_b = std::make_unique<connext_lib::DdsReceiverResourcesManager>(
        dp_rx_b, buffer_id_b, channel);
    auto reader_b = std::make_unique<connext_lib::DdsPayloadReader>(
        dp_rx_b, "GPU/" + channel, buffer_id_b);
    RTI_TEST_ASSERT(receiver_mgr_b->announce());

    // Sender (SUT)
    connext_lib::ConnextANOWriter writer(ano_a, dds_config_a, 100ms);

    // Allow discovery
    std::this_thread::sleep_for(300ms);

    const std::string message = "payload_01";
    connext_lib::PayloadBufferView buffer_to_send{
        reinterpret_cast<const std::uint8_t*>(message.data()), message.size()};

    std::size_t sent = 0;
    for (int attempt = 0; attempt < 80 && sent < 2; ++attempt) {
      sent = writer.broadcast(buffer_to_send);
      if (sent < 2) {
        std::this_thread::sleep_for(100ms);
      }
    }
    RTI_TEST_ASSERT_EQUALS_INT(2, static_cast<int>(sent));

    std::vector<std::uint8_t> rx_a_data, rx_b_data;
    RTI_TEST_ASSERT(reader_a->readNext(rx_a_data, 4s));
    RTI_TEST_ASSERT(reader_b->readNext(rx_b_data, 4s));
    std::string rx_a_all(rx_a_data.begin(), rx_a_data.end());
    std::string rx_b_all(rx_b_data.begin(), rx_b_data.end());
    RTI_TEST_ASSERT(rx_a_all == message);
    RTI_TEST_ASSERT(rx_b_all == message);
  }

  //TODO: move this test to its own file so we create an integration test for testing ConnextANOReader and ConnextANOWriter together
  void ano_writer_roundtrip() {
    const std::string channel_name = "TestAnoChannel";
    const std::string buffer_id = "ano_buffer_test";
    const std::string topic = "TestAnoTopic";
    const int domain = domain_id();
    const std::string test_message = "ano_writer_roundtrip_message";
    const std::size_t max_payload_bytes = 1024;

    connext_lib::AnoConfig ano_config(channel_name, buffer_id, max_payload_bytes, true);
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);

    // Writer (SUT)
    connext_lib::ConnextANOWriter writer(ano_config, dds_config, poll_interval_ms);

    // Reader
    connext_lib::ConnextANOReader reader(ano_config, dds_config, poll_interval_ms);

    // Allow discovery
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Write payload
    std::vector<std::uint8_t> payload_v(test_message.begin(), test_message.end());
    connext_lib::PayloadBufferView buffer_to_send{
      reinterpret_cast<const std::uint8_t*>(test_message.data()), test_message.size()};
    auto res = writer.broadcast(buffer_to_send);
    RTI_TEST_ASSERT(res>0);

    // Poll for sample availability up to timeout
    std::vector<std::uint8_t> samples;
    const int max_attempts = 30; // 3 seconds total
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      samples = reader.readSamples();
      if (!samples.empty()) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    std::string received(samples.begin(), samples.end());
    RTI_TEST_ASSERT(received == test_message);
  }

 private:
  ConnextWritersTester() : rti::test::Tester("connext_lib_writers_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextWritersTester, dds_writer_broadcasts_to_single_receiver);
    RTI_TEST_FUNCTION_ADD(ConnextWritersTester, ano_writer_broadcast_destination);
    RTI_TEST_FUNCTION_ADD(ConnextWritersTester, ano_writer_roundtrip);
  }

  std::atomic<int> channel_counter_{0};
  friend class rti::test::Singleton<ConnextWritersTester>;
};

class ConnextWritersTestContainer : public rti::test::TesterContainer,
                                    public rti::test::Singleton<ConnextWritersTestContainer> {
 private:
  ConnextWritersTestContainer() : rti::test::TesterContainer("connext_lib_writers") {
    add_tester<ConnextWritersTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConnextWritersTestContainer>;
};

} // namespace

int main(int argc, char** argv) {
  return ConnextWritersTestContainer::get_instance().run_tests(argc, argv);
}
