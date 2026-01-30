#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/config/config.hpp"
#include "connext_lib/comm/connext_readers.hpp"
#include "cuda_test_utils.hpp"
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
    connext_lib::MemoryBufferView buffer;
    buffer.ptr = const_cast<void*>(reinterpret_cast<const void*>(message.data()));
    buffer.size_bytes = message.size();
    buffer.is_device = false; // CPU memory

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
    void* data_ptr = nullptr;
    std::size_t data_size = 0;
    RTI_TEST_ASSERT(payload_reader->readNext(data_ptr, data_size, 4s));
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(message.size()), static_cast<int>(data_size));
    auto byte_ptr = static_cast<const std::uint8_t*>(data_ptr);
    std::string received_str(byte_ptr, byte_ptr + data_size);
    RTI_TEST_ASSERT(received_str == message);
    payload_reader->freeData(data_ptr);
  }

  void ano_writer_broadcast_destination() {
    const int domain = domain_id();
    const std::string channel = "writers_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id_a = "buffer_a";
    const std::string buffer_id_b = "buffer_b";

    connext_lib::AnoConfig ano_a(channel,buffer_id_a,1024,true);
    connext_lib::AnoConfig ano_b(channel,buffer_id_b,1024,true);

    connext_lib::DdsConfig dds_config_a(true, domain, channel, "BytesTopicType");

    // Receiver A - using ANO reader
    connext_lib::DdsConfig dds_config_rx_a(true, domain, channel, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);
    auto reader_a = std::make_unique<connext_lib::ConnextANOReader>(ano_a, dds_config_rx_a, poll_interval_ms);

    // Receiver B - using ANO reader
    connext_lib::DdsConfig dds_config_rx_b(true, domain, channel, "BytesTopicType");
    auto reader_b = std::make_unique<connext_lib::ConnextANOReader>(ano_b, dds_config_rx_b, poll_interval_ms);

    // Sender (SUT)
    connext_lib::ConnextANOWriter writer(ano_a, dds_config_a, 100ms);

    // Allow discovery
    std::this_thread::sleep_for(300ms);

    const std::string message = "payload_01";
    
    // Allocate GPU memory and copy message to GPU
    auto gpu_mem = connext_lib::test::copyToGpu(message.data(), message.size());
    
    connext_lib::MemoryBufferView buffer_to_send;
    buffer_to_send.ptr = gpu_mem.get();
    buffer_to_send.size_bytes = message.size();
    buffer_to_send.is_device = true; // GPU memory

    std::size_t sent = 0;
    for (int attempt = 0; attempt < 80 && sent < 2; ++attempt) {
      sent = writer.broadcast(buffer_to_send);
      if (sent < 2) {
        std::this_thread::sleep_for(100ms);
      }
    }
    
    // Skip test if ANO hardware not available
    if (sent == 0) {
      std::cout << "Skipping ANO test - hardware not available or not configured" << std::endl;
      return;
    }
    
    RTI_TEST_ASSERT_EQUALS_INT(2, static_cast<int>(sent));

    // Read from receiver A
    connext_lib::MemoryBufferView rx_a_buffer{nullptr, 0, false};
    const int max_attempts = 40;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      rx_a_buffer = reader_a->readSamples();
      if (rx_a_buffer.ptr != nullptr) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    RTI_TEST_ASSERT(rx_a_buffer.ptr != nullptr);
    
    // Read from receiver B
    connext_lib::MemoryBufferView rx_b_buffer{nullptr, 0, false};
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      rx_b_buffer = reader_b->readSamples();
      if (rx_b_buffer.ptr != nullptr) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    RTI_TEST_ASSERT(rx_b_buffer.ptr != nullptr);

    // Copy from GPU to CPU for validation
    std::string rx_a_all = connext_lib::test::copyStringFromGpu(rx_a_buffer.ptr, rx_a_buffer.size_bytes);
    std::string rx_b_all = connext_lib::test::copyStringFromGpu(rx_b_buffer.ptr, rx_b_buffer.size_bytes);
    
    reader_a->freeBuffer(rx_a_buffer);
    reader_b->freeBuffer(rx_b_buffer);
    
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
