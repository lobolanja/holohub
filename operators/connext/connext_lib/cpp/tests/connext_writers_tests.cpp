#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/config/config.hpp"
#include "connext_lib/comm/connext_readers.hpp"
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
    const std::string topic = "writers_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "*";

    // Receiver side (announce resources then prepare payload reader)
    dds::domain::DomainParticipant dp_rx(domain);
    auto payload_reader = std::make_unique<connext_lib::DdsPayloadReader>(
        dp_rx, topic, buffer_id);

    // Sender side (SUT)
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    connext_lib::ConnextDDSWriter writer(dds_config, 1025);

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

 private:
  ConnextWritersTester() : rti::test::Tester("connext_lib_writers_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextWritersTester, dds_writer_broadcasts_to_single_receiver);
    RTI_TEST_FUNCTION_ADD(ConnextWritersTester, ano_writer_broadcast_destination);
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
