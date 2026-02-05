// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0

/**
 * @file connext_lib_public_api_tests.cpp
 * @brief Tests for the public API of connext_lib
 *
 * This test file validates that:
 * 1. The public API header (connext_lib.hpp) is self-contained
 * 2. All necessary types and functions are accessible
 * 3. The API provides a clean, usable interface for external users
 *
 * This test serves as both validation and executable documentation for
 * library users.
 */

// Only include the public API header - this validates it's self-contained
#include "connext_lib.hpp"
#include "cuda_test_utils.hpp"
#include "net_test_config_helper.hpp"

#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "dds/dds.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;

/**
 * Test fixture for public API validation.
 * These tests demonstrate typical usage patterns that external users would follow.
 */
class ConnextLibPublicAPITester : public rti::test::Tester,
                                  public rti::test::Singleton<ConnextLibPublicAPITester> {
 public:
  /**
   * Test: Library version is accessible and returns expected format.
   * Demonstrates how to query library version information.
   */
  void test_version_accessible() {
    int ver = connext_lib::version();
    RTI_TEST_ASSERT(ver > 0);
    RTI_TEST_ASSERT(ver == 100);  // Expected: 0.1.0
  }

  /**
   * Test: Configuration objects can be constructed and queried.
   * Demonstrates how to create and configure DDS and ANO settings.
   */
  void test_configuration_types() {
    // DDS Configuration
    connext_lib::DdsConfig dds_config(
        true,                    // enabled
        42,                      // domain_id
        "TestTopic",            // topic_name
        "BytesTopicType"        // topic_type_name
    );

    RTI_TEST_ASSERT(dds_config.enabled() == true);
    RTI_TEST_ASSERT(dds_config.domain_id() == 42);
    RTI_TEST_ASSERT(dds_config.topic_name() == "TestTopic");
    RTI_TEST_ASSERT(dds_config.topic_type_name() == "BytesTopicType");

    // ANO Configuration
    connext_lib::AnoConfig ano_config(
        "test_channel",         // channel_name
        "buffer_test",          // buffer_id
        2048,                   // max_payload_bytes
        true                    // enabled
    );

    RTI_TEST_ASSERT(ano_config.enabled() == true);
    RTI_TEST_ASSERT(ano_config.channel_name() == "test_channel");
    RTI_TEST_ASSERT(ano_config.buffer_id() == "buffer_test");
    RTI_TEST_ASSERT(ano_config.max_payload_bytes() == 2048);
  }

  /**
   * Test: MemoryBufferView can be created and used.
   * Demonstrates how to wrap user data for transmission.
   */
  void test_memory_buffer_view() {
    std::string test_data = "Public API test payload";

    connext_lib::MemoryBufferView buffer;
    buffer.ptr = static_cast<void*>(test_data.data());
    buffer.size_bytes = test_data.size();
    buffer.is_device = false;

    RTI_TEST_ASSERT(buffer.ptr != nullptr);
    RTI_TEST_ASSERT(buffer.size_bytes == test_data.size());
  }

  /**
 * Test: DDS Writer and Reader roundtrip communication.
 * Demonstrates complete end-to-end usage of the public API:
 * 1. Configure DDS settings
 * 2. Create writer and reader
 * 3. Send data through writer
 * 4. Receive data through reader
 * 5. Verify data integrity
 */
void test_dds_writer_reader_roundtrip() {
  const std::string topic = "PublicAPIDDSTopic";
  const int domain = domain_id();
  const std::string test_message = "Hello from DDS public API!";
  const std::size_t max_payload_bytes = 1024;

  // Step 1: Configure DDS
  connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
  std::chrono::milliseconds poll_interval_ms(100);

  // Step 2: Create writer and reader
  connext_lib::ConnextDDSWriter writer(dds_config, max_payload_bytes);
  connext_lib::ConnextDDSReader reader(dds_config, poll_interval_ms);

  // Allow DDS discovery to complete
  std::this_thread::sleep_for(std::chrono::seconds(3));

  // Step 3: Prepare and send payload
  connext_lib::MemoryBufferView buffer_to_send;
  buffer_to_send.ptr = const_cast<void*>(reinterpret_cast<const void*>(test_message.data()));
  buffer_to_send.size_bytes = test_message.size();
  buffer_to_send.is_device = false; // CPU memory

  std::size_t sent_count = writer.broadcast(buffer_to_send);
  RTI_TEST_ASSERT(sent_count > 0);

  // Step 4: Poll for received samples
  connext_lib::MemoryBufferView received_buffer{nullptr, 0, false};
  const int max_attempts = 30;  // 3 seconds total
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    received_buffer = reader.readSamples();
    if (received_buffer.ptr != nullptr) {
      break;
    }
    std::this_thread::sleep_for(poll_interval_ms);
  }

  // Step 5: Verify received data
  RTI_TEST_ASSERT(received_buffer.ptr != nullptr);
  auto byte_ptr = static_cast<const std::uint8_t*>(received_buffer.ptr);
  std::string received(byte_ptr, byte_ptr + received_buffer.size_bytes);
  RTI_TEST_ASSERT(received == test_message);
  reader.freeBuffer(received_buffer);
}

  /**
   * Test: ANO Writer and Reader roundtrip communication.
   * Demonstrates complete end-to-end usage of the public API:
   * 1. Configure DDS and ANO settings
   * 2. Create writer and reader
   * 3. Send data through writer (using GPU memory)
   * 4. Receive data through reader
   * 5. Verify data integrity
   * 
   * Note: This test runs in loopback mode and always executes.
   */
  void test_ano_writer_reader_roundtrip() {
    const std::string channel_name = "PublicAPITestChannel";
    const std::string buffer_id = "public_api_buffer";
    const std::string topic = "PublicAPITestTopic";
    const int domain = domain_id();
    const std::string test_message = "Hello from public API!";
    const std::size_t max_payload_bytes = 1024;

    // Step 1: Configure DDS and ANO with proper network configuration
    auto reader_net_config = connext_lib::test::AnoTestConfigHelper::CreateDefaultReaderNetworkConfig();
    auto writer_net_config = connext_lib::test::AnoTestConfigHelper::CreateDefaultWriterNetworkConfig();
    
    connext_lib::AnoConfig reader_ano_config = 
      connext_lib::test::AnoTestConfigHelper::CreateStandardAnoConfig(
        channel_name, buffer_id, max_payload_bytes, reader_net_config);
    connext_lib::AnoConfig writer_ano_config = 
      connext_lib::test::AnoTestConfigHelper::CreateStandardAnoConfig(
        channel_name, buffer_id, max_payload_bytes, writer_net_config);
    
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);

    // Step 2: Create writer and reader
    connext_lib::ConnextANOWriter writer(writer_ano_config, dds_config, poll_interval_ms);
    connext_lib::ConnextANOReader reader(reader_ano_config, dds_config, poll_interval_ms);

    // Step 3: Allow DDS discovery to complete
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Step 4: Prepare and send payload (using GPU memory)
    auto gpu_mem = connext_lib::test::copyToGpu(test_message.data(), test_message.size());
    
    connext_lib::MemoryBufferView buffer_to_send;
    buffer_to_send.ptr = gpu_mem.get();
    buffer_to_send.size_bytes = test_message.size();
    buffer_to_send.is_device = true; // GPU memory

    // Wait for ANO discovery, then attempt a single broadcast
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::size_t sent_count = writer.broadcast(buffer_to_send);
    RTI_TEST_ASSERT(sent_count > 0);

    // Step 5: Poll for received samples
    connext_lib::MemoryBufferView received_buffer{nullptr, 0, false};
    const int max_attempts = 30;  // 3 seconds total
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      received_buffer = reader.readSamples();
      if (received_buffer.ptr != nullptr) {
        break;
      }
      std::this_thread::sleep_for(poll_interval_ms);
    }

    // Step 6: Verify received data
    RTI_TEST_ASSERT(received_buffer.ptr != nullptr);
    std::string received = connext_lib::test::copyStringFromGpu(received_buffer.ptr, received_buffer.size_bytes);
    RTI_TEST_ASSERT(received == test_message);
    reader.freeBuffer(received_buffer);
  }

 private:
  ConnextLibPublicAPITester() : rti::test::Tester("connext_lib_public_api_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextLibPublicAPITester, test_version_accessible);
    RTI_TEST_FUNCTION_ADD(ConnextLibPublicAPITester, test_configuration_types);
    RTI_TEST_FUNCTION_ADD(ConnextLibPublicAPITester, test_memory_buffer_view);
    RTI_TEST_FUNCTION_ADD(ConnextLibPublicAPITester, test_dds_writer_reader_roundtrip);
    RTI_TEST_FUNCTION_ADD(ConnextLibPublicAPITester, test_ano_writer_reader_roundtrip);
  }
  friend class rti::test::Singleton<ConnextLibPublicAPITester>;
};

class ConnextLibPublicAPITestContainer : public rti::test::TesterContainer,
                                         public rti::test::Singleton<ConnextLibPublicAPITestContainer> {
 private:
  ConnextLibPublicAPITestContainer() : rti::test::TesterContainer("connext_lib_public_api") {
    add_tester<ConnextLibPublicAPITester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    
    // Initialize ANO if hardware is available
    if (!connext_lib::test::AnoInitializer::Initialize()) {
      std::cout << "Warning: ANO initialization failed or hardware not available. "
                << "Tests may fail if they require ANO hardware." << std::endl;
    }
    
    return rti::test::TesterContainer::on_tests_begin(setting);
  }
  
  bool on_tests_end(const RTITestSetting& setting) override {
    bool result = rti::test::TesterContainer::on_tests_end(setting);
    connext_lib::test::AnoInitializer::Shutdown();
    return result;
  }

  friend class rti::test::Singleton<ConnextLibPublicAPITestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ConnextLibPublicAPITestContainer::get_instance().run_tests(argc, argv);
}

