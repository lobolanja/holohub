#include "connext_lib/comm/connext_rx.hpp"
#include "connext_lib/comm/connext_tx.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "dds/dds.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "ndds/ddsctesthelpers/test_context.h"

namespace {

using namespace std::chrono_literals;

class ConnextCommTester : public rti::test::Tester,
                          public rti::test::Singleton<ConnextCommTester> {
 public:
  void rx_announces_and_reads() {
    const std::string channel =
        "comm_channel_" + std::to_string(++channel_counter_);
    dds::domain::DomainParticipant dp_read(domain_id());
    auto receiver = std::make_unique<connext_lib::DdsReceiverResourcesManager>(
        dp_read, "rx_buffer", channel);
    auto reader = std::make_unique<connext_lib::DdsPayloadReader>(
        dp_read, "GPU/"+channel, "rx_buffer");
    // Use DDSCTestContext_waitForReaders for discovery instead of sleep
    dds::domain::DomainParticipant dp_write(domain_id());
    auto writer = std::make_unique<connext_lib::DdsPayloadWriter>(dp_write, "GPU/"+channel, 16);
    DDSCTestContext_waitForReaders(1, writer->get_dds_writer()->native_writer(), 5); // 5 seconds timeout
    const std::array<std::uint8_t, 3> payload{1, 2, 3};
    connext_lib::PayloadBufferView view{payload.data(), payload.size()};
    writer->setBuffer(view);
    RTI_TEST_ASSERT(writer->writeTo("rx_buffer"));
    connext_lib::ConnextRx rx(std::move(receiver), std::move(reader));
    std::vector<std::uint8_t> buffer;
    RTI_TEST_ASSERT(rx.receive(buffer, 2s));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(3), static_cast<int>(buffer.size()));
    RTI_TEST_ASSERT_EQUALS_INT(1, buffer[0]);
    RTI_TEST_ASSERT_EQUALS_INT(2, buffer[1]);
    RTI_TEST_ASSERT_EQUALS_INT(3, buffer[2]);
  }

  void tx_broadcasts_to_destinations() {
    const std::string channel =
        "comm_channel_" + std::to_string(++channel_counter_);
    dds::domain::DomainParticipant dp_reader1(domain_id());
    auto receiver_mgr_a = std::make_unique<connext_lib::DdsReceiverResourcesManager>(dp_reader1, "buffer_a", channel);
    auto reader_a = std::make_unique<connext_lib::DdsPayloadReader>(dp_reader1, "GPU/"+channel, "buffer_a");
    dds::domain::DomainParticipant dp_reader2(domain_id());
    auto receiver_mgr_b = std::make_unique<connext_lib::DdsReceiverResourcesManager>(dp_reader2, "buffer_b", channel);
    auto reader_b = std::make_unique<connext_lib::DdsPayloadReader>(dp_reader2, "GPU/"+channel, "buffer_b");
    dds::domain::DomainParticipant sender_participant(domain_id());
    auto sender = std::make_unique<connext_lib::DdsSenderResourcesManager>(sender_participant, channel);
    auto writer = std::make_unique<connext_lib::DdsPayloadWriter>(sender_participant, "GPU/"+channel, 1024);
    DDSCTestContext_waitForReaders(2, writer->get_dds_writer()->native_writer(), 5); // Wait for 2 readers
    DDSCTestContext_waitForWriters(1, reader_a->get_dds_reader()->native_reader(), 5); // Wait for 1 writer
    DDSCTestContext_waitForWriters(1, reader_b->get_dds_reader()->native_reader(), 5); // Wait for 1 writer
    connext_lib::ConnextRx rx_a(std::move(receiver_mgr_a), std::move(reader_a));
    connext_lib::ConnextRx rx_b(std::move(receiver_mgr_b), std::move(reader_b));
    connext_lib::ConnextTx tx(std::move(sender), std::move(writer), 10ms);
    const std::array<std::uint8_t, 4> payload1{5, 4, 3, 2};
    const std::array<std::uint8_t, 3> payload2{9, 8, 7};
    const std::vector<std::pair<const std::uint8_t*, std::size_t>> payloads = {
      {payload1.data(), payload1.size()},
      {payload2.data(), payload2.size()}
    };
    // Broadcast all payloads first
    for (const auto& [payload_data, payload_size] : payloads) {
      connext_lib::PayloadBufferView view{payload_data, payload_size};
      tx.setBuffer(view);
      std::size_t sent = 0;
      for (int attempt = 0; attempt < 80 && sent < 2; ++attempt) {
        sent = tx.broadcast();
        if (sent < 2) {
          std::this_thread::sleep_for(100ms);
        }
      }
      RTI_TEST_ASSERT_EQUALS_INT(2, static_cast<int>(sent));
      std::vector<std::uint8_t> buf_a;
      std::vector<std::uint8_t> buf_b;
      RTI_TEST_ASSERT(rx_a.receive(buf_a, 5s));
      RTI_TEST_ASSERT(rx_b.receive(buf_b, 5s));
      RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(payload_size),
                                 static_cast<int>(buf_a.size()));
      RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(payload_size),
                                 static_cast<int>(buf_b.size()));
      for (std::size_t i = 0; i < payload_size; ++i) {
        RTI_TEST_ASSERT_EQUALS_INT(payload_data[i], buf_a[i]);
        RTI_TEST_ASSERT_EQUALS_INT(payload_data[i], buf_b[i]);
      }
    }
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
