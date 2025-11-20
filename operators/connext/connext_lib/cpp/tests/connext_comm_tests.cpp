#include "connext_lib/connext_rx.hpp"
#include "connext_lib/connext_tx.hpp"
#include "connext_lib/payload_transport_dds.hpp"
#include "connext_lib/resource_managers_dds.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "dds/dds.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {

using namespace std::chrono_literals;

class ConnextCommTester : public rti::test::Tester,
                          public rti::test::Singleton<ConnextCommTester> {
 public:
  void rx_announces_and_reads() {
    const std::string channel =
        "comm_channel_" + std::to_string(++channel_counter_);
    dds::domain::DomainParticipant receiver_participant(domain_id());
    auto receiver = std::make_unique<connext_lib::DdsReceiverResourcesManager>(
        receiver_participant, "rx_buffer", channel);
    auto reader = std::make_unique<connext_lib::DdsPayloadReader>(
        domain_id(), channel);
    connext_lib::ConnextRx rx(std::move(receiver), std::move(reader));

    // Allow discovery plus QoS propagation.
    std::this_thread::sleep_for(200ms);

    connext_lib::DdsPayloadWriter writer(domain_id(), channel, 16);
    const std::array<std::uint8_t, 3> payload{1, 2, 3};
    connext_lib::PayloadBufferView view{payload.data(), payload.size()};
    writer.setBuffer(view);
    RTI_TEST_ASSERT(writer.writeTo(""));

    std::vector<std::uint8_t> buffer;
    RTI_TEST_ASSERT(rx.receive(buffer, 2s));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(payload.size()),
                               static_cast<int>(buffer.size()));
    for (std::size_t i = 0; i < payload.size(); ++i) {
      RTI_TEST_ASSERT_EQUALS_INT(payload[i], buffer[i]);
    }
  }

  void tx_broadcasts_to_destinations() {
    const std::string channel =
        "comm_channel_" + std::to_string(++channel_counter_);

    auto rx_a = std::make_unique<connext_lib::ConnextRx>(
        std::make_unique<connext_lib::DdsReceiverResourcesManager>(
            dds::domain::DomainParticipant(domain_id()),
            "buffer_a",
            channel),
        std::make_unique<connext_lib::DdsPayloadReader>(domain_id(), channel));
    auto rx_b = std::make_unique<connext_lib::ConnextRx>(
        std::make_unique<connext_lib::DdsReceiverResourcesManager>(
            dds::domain::DomainParticipant(domain_id()),
            "buffer_b",
            channel),
        std::make_unique<connext_lib::DdsPayloadReader>(domain_id(), channel));

    dds::domain::DomainParticipant sender_participant(domain_id());
    auto sender = std::make_unique<connext_lib::DdsSenderResourcesManager>(
        sender_participant, channel);
    auto writer = std::make_unique<connext_lib::DdsPayloadWriter>(
        domain_id(), channel, 0);
    connext_lib::ConnextTx tx(std::move(sender), std::move(writer), 10ms);
    
    // TODO: sent 2 diferent payloads to verify both are received
    const std::array<std::uint8_t, 4> payload{5, 4, 3, 2};
    connext_lib::PayloadBufferView view{payload.data(), payload.size()};
    tx.setBuffer(view);

    std::size_t sent = 0;
    for (int attempt = 0; attempt < 80 && sent < 2; ++attempt) {
      sent = tx.broadcast();
      if (sent < 2) {
        std::this_thread::sleep_for(50ms);
      }
    }
    RTI_TEST_ASSERT_EQUALS_INT(2, static_cast<int>(sent));

    std::vector<std::uint8_t> buf_a;
    std::vector<std::uint8_t> buf_b;
    RTI_TEST_ASSERT(rx_a->receive(buf_a, 2s));
    RTI_TEST_ASSERT(rx_b->receive(buf_b, 2s));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(payload.size()),
                               static_cast<int>(buf_a.size()));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(payload.size()),
                               static_cast<int>(buf_b.size()));
  }

 private:
  ConnextCommTester() : rti::test::Tester("connext_comm_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextCommTester, rx_announces_and_reads);
    RTI_TEST_FUNCTION_ADD(ConnextCommTester, tx_broadcasts_to_destinations);
  }

  std::atomic<int> channel_counter_{0};

  friend class rti::test::Singleton<ConnextCommTester>;
};

class ConnextCommContainer : public rti::test::TesterContainer,
                             public rti::test::Singleton<ConnextCommContainer> {
 private:
  ConnextCommContainer()
      : rti::test::TesterContainer("connext_comm_tests_container") {
    add_tester<ConnextCommTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConnextCommContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ConnextCommContainer::get_instance().run_tests(argc, argv);
}
