#include "connext_lib/comm/connext_readers.hpp"
#include "connext_lib/comm/connext_writers.hpp"
#include "cuda_test_utils.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {

class ConnextReadersTester : public rti::test::Tester,
                            public rti::test::Singleton<ConnextReadersTester> {
 public:
  void basic_construction_and_read_samples() {
    const std::string topic = "TestTopic";
    const int domain = domain_id();
    const std::string test_message = "dds_reader_writer_roundtrip";
    const std::size_t max_payload_bytes = 1024;
    dds::domain::DomainParticipant dp(domain);

    // Create DDS writer
    connext_lib::DdsPayloadWriter writer(dp, topic, max_payload_bytes);
    connext_lib::MemoryBufferView writer_buffer;
    writer_buffer.ptr = const_cast<void*>(static_cast<const void*>(test_message.data()));
    writer_buffer.size_bytes = test_message.size();
    writer_buffer.is_device = false;
    writer.setBuffer(writer_buffer);
    RTI_TEST_ASSERT(writer.writeTo("broadcast"));

    // Create DDS reader (SUT)
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);
    connext_lib::ConnextDDSReader reader(dds_config, poll_interval_ms);

    // Poll for sample availability up to timeout
    connext_lib::MemoryBufferView received_buffer{nullptr, 0, false};
    const int max_attempts = 30; // 3 seconds total
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      received_buffer = reader.readSamples();
      if (received_buffer.ptr != nullptr) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    
    RTI_TEST_ASSERT(received_buffer.ptr != nullptr);
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(test_message.size()), static_cast<int>(received_buffer.size_bytes));
    
    // Cast to CPU pointer (DDS uses CPU memory)
    auto byte_ptr = static_cast<const std::uint8_t*>(received_buffer.ptr);
    std::string received(byte_ptr, byte_ptr + received_buffer.size_bytes);
    RTI_TEST_ASSERT(received == test_message);
    
    // Free the buffer
    reader.freeBuffer(received_buffer);
  }

  void ano_reader_construction_and_read_samples() {
    const std::string channel_name = "TestAnoChannel";
    const std::string buffer_id = "ano_buffer_test";
    const std::string topic = "TestAnoTopic";
    const int domain = domain_id();
    const std::string test_message = "ano_reader_writer_roundtrip";
    const std::size_t max_payload_bytes = 1024;

    // Create ANO and DDS configs
    connext_lib::AnoConfig ano_config(channel_name, buffer_id, max_payload_bytes, true);
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);

    // Create ANO reader (SUT)
    connext_lib::ConnextANOReader reader(ano_config, dds_config, poll_interval_ms);

    // Create ANO writer with GPU memory
    connext_lib::ConnextANOWriter writer(ano_config, dds_config, poll_interval_ms);

    // Allow discovery
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Allocate GPU memory and copy test message to GPU
    auto gpu_mem = connext_lib::test::copyToGpu(test_message.data(), test_message.size());

    // Create buffer pointing to GPU memory
    connext_lib::MemoryBufferView send_buffer;
    send_buffer.ptr = gpu_mem.get();
    send_buffer.size_bytes = test_message.size();
    send_buffer.is_device = true;  // GPU memory

    // Send via ANO writer
    std::size_t sent = 0;
    for (int attempt = 0; attempt < 50 && sent < 1; ++attempt) {
      sent = writer.broadcast(send_buffer);
      if (sent < 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    
    // Skip test if ANO hardware not available
    if (sent == 0) {
      std::cout << "Skipping ANO test - hardware not available or not configured" << std::endl;
      return;
    }
    
    RTI_TEST_ASSERT_EQUALS_INT(1, static_cast<int>(sent));

    // Poll for sample availability up to timeout
    connext_lib::MemoryBufferView received_buffer{nullptr, 0, false};
    const int max_attempts = 30; // 3 seconds total
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      received_buffer = reader.readSamples();
      if (received_buffer.ptr != nullptr) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    
    RTI_TEST_ASSERT(received_buffer.ptr != nullptr);
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(test_message.size()), static_cast<int>(received_buffer.size_bytes));
    
    // Copy from GPU to CPU for validation
    std::string received = connext_lib::test::copyStringFromGpu(received_buffer.ptr, received_buffer.size_bytes);
    RTI_TEST_ASSERT(received == test_message);
    
    // Free the buffer
    reader.freeBuffer(received_buffer);
  }

 private:
  ConnextReadersTester() : rti::test::Tester("connext_lib_readers_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextReadersTester, basic_construction_and_read_samples);
    RTI_TEST_FUNCTION_ADD(ConnextReadersTester, ano_reader_construction_and_read_samples);
  }

  friend class rti::test::Singleton<ConnextReadersTester>;
};

class ConnextReadersTestContainer : public rti::test::TesterContainer,
                                    public rti::test::Singleton<ConnextReadersTestContainer> {
 private:
  ConnextReadersTestContainer() : rti::test::TesterContainer("connext_lib_readers") {
    add_tester<ConnextReadersTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConnextReadersTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ConnextReadersTestContainer::get_instance().run_tests(argc, argv);
}
