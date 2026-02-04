/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/comm/connext_readers.hpp"
#include "connext_lib/config/config.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "ndds/ddsctesthelpers/test_context.h"
#include "dds/dds.hpp"
#include <chrono>
#include <thread>
#include <string>
#include <set>
#include <vector>

namespace {
using namespace std::chrono_literals;

class ConnextDdsIntegrationTester : public rti::test::Tester,
                                    public rti::test::Singleton<ConnextDdsIntegrationTester> {
public:
  void test_dds_writer_to_reader_roundtrip() {
    const int domain = 78;
    const std::string topic = "dds_integration_" + std::to_string(++channel_counter_);
    const std::string test_message = "DDS integration test message - CPU memory roundtrip";
    
    // Create DDS configuration
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);

    // Create reader first
    connext_lib::ConnextDDSReader reader(dds_config, poll_interval_ms);

    // Create writer
    connext_lib::ConnextDDSWriter writer(dds_config, test_message.size() + 256);

    // Allow time for DDS discovery
    std::this_thread::sleep_for(300ms);

    // Prepare test data in CPU memory
    connext_lib::MemoryBufferView send_buffer{
      const_cast<void*>(static_cast<const void*>(test_message.data())),
      test_message.size(),
      false  // is_device = false (CPU memory)
    };

    // Send data via DDS writer - retry until discovery completes
    size_t sent = 0;
    for (int attempt = 0; attempt < 40 && sent < 1; ++attempt) {
      sent = writer.broadcast(send_buffer);
      if (sent < 1) {
        std::this_thread::sleep_for(50ms);
      }
    }
    RTI_TEST_ASSERT(sent == 1);

    // Receive data via DDS reader - poll until data available
    connext_lib::MemoryBufferView recv_buffer{nullptr, 0, false};
    const int max_attempts = 40;  // 4 seconds total
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      recv_buffer = reader.readSamples();
      if (recv_buffer.ptr != nullptr) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    
    RTI_TEST_ASSERT(recv_buffer.ptr != nullptr);
    RTI_TEST_ASSERT(recv_buffer.size_bytes == test_message.size());
    RTI_TEST_ASSERT(!recv_buffer.is_device);  // DDS uses CPU memory

    // Validate received data
    auto byte_ptr = static_cast<const std::uint8_t*>(recv_buffer.ptr);
    std::string received_message(byte_ptr, byte_ptr + recv_buffer.size_bytes);
    RTI_TEST_ASSERT(received_message == test_message);

    // Free the buffer
    reader.freeBuffer(recv_buffer);
  }

  void test_dds_multiple_messages() {
    const int domain = 79;
    const std::string topic = "dds_multi_msg_" + std::to_string(++channel_counter_);
    
    // Create DDS configuration
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);

    // Create reader and writer
    connext_lib::ConnextDDSReader reader(dds_config, poll_interval_ms);
    connext_lib::ConnextDDSWriter writer(dds_config, 1024);

    // Allow time for DDS discovery
    std::this_thread::sleep_for(300ms);

    // Send first message
    std::string msg1 = "First DDS message";
    connext_lib::MemoryBufferView send_buffer1{
      const_cast<void*>(static_cast<const void*>(msg1.data())),
      msg1.size(),
      false
    };

    size_t sent = 0;
    for (int attempt = 0; attempt < 20 && sent < 1; ++attempt) {
      sent = writer.broadcast(send_buffer1);
      if (sent < 1) {
        std::this_thread::sleep_for(50ms);
      }
    }
    RTI_TEST_ASSERT(sent == 1);

    // Receive first message
    connext_lib::MemoryBufferView recv_buffer1{nullptr, 0, false};
    const int max_attempts = 50;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      recv_buffer1 = reader.readSamples();
      if (recv_buffer1.ptr != nullptr) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    
    RTI_TEST_ASSERT(recv_buffer1.ptr != nullptr);
    auto byte_ptr1 = static_cast<const std::uint8_t*>(recv_buffer1.ptr);
    std::string received1(byte_ptr1, byte_ptr1 + recv_buffer1.size_bytes);
    RTI_TEST_ASSERT(received1 == msg1);
    reader.freeBuffer(recv_buffer1);

    // Send second message
    std::string msg2 = "Second DDS message - different content";
    connext_lib::MemoryBufferView send_buffer2{
      const_cast<void*>(static_cast<const void*>(msg2.data())),
      msg2.size(),
      false
    };

    sent = 0;
    for (int attempt = 0; attempt < 20 && sent < 1; ++attempt) {
      sent = writer.broadcast(send_buffer2);
      if (sent < 1) {
        std::this_thread::sleep_for(50ms);
      }
    }
    RTI_TEST_ASSERT(sent == 1);

    // Receive second message
    connext_lib::MemoryBufferView recv_buffer2{nullptr, 0, false};
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      recv_buffer2 = reader.readSamples();
      if (recv_buffer2.ptr != nullptr) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    
    RTI_TEST_ASSERT(recv_buffer2.ptr != nullptr);
    auto byte_ptr2 = static_cast<const std::uint8_t*>(recv_buffer2.ptr);
    std::string received2(byte_ptr2, byte_ptr2 + recv_buffer2.size_bytes);
    RTI_TEST_ASSERT(received2 == msg2);
    reader.freeBuffer(recv_buffer2);
  }

  void test_dds_high_frequency_samples() {
    const int domain = 78;
    const std::string topic = "dds_hf_" + std::to_string(++channel_counter_);
    const int num_samples = 10;
    
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(50);  // 50ms polling timeout

    connext_lib::ConnextDDSReader reader(dds_config, poll_interval_ms);
    connext_lib::ConnextDDSWriter writer(dds_config, 1024);

    // Use DDS test helper for reliable discovery
    auto* dds_writer_ptr = writer.get_dds_writer();
    RTI_TEST_ASSERT(dds_writer_ptr != nullptr);
    DDSCTestContext_waitForReaders(1, (*dds_writer_ptr)->native_writer(), 5);

    // Send and receive 10 samples at 100Hz (10ms interval)
    for (int i = 0; i < num_samples; ++i) {
      std::string test_message = "DDS_Sample_" + std::to_string(i);
      connext_lib::MemoryBufferView send_buffer{
        const_cast<void*>(static_cast<const void*>(test_message.data())),
        test_message.size(),
        false  // CPU memory
      };

      size_t sent = writer.broadcast(send_buffer);
      RTI_TEST_ASSERT(sent == 1);

      // 100Hz = 10ms period
      std::this_thread::sleep_for(10ms);

      // Receive with 50ms polling timeout
      connext_lib::MemoryBufferView recv_buffer{nullptr, 0, false};
      const int max_attempts = 10;  // 500ms total
      for (int attempt = 0; attempt < max_attempts; ++attempt) {
        recv_buffer = reader.readSamples();
        if (recv_buffer.ptr != nullptr) break;
        std::this_thread::sleep_for(poll_interval_ms);
      }
      
      RTI_TEST_ASSERT(recv_buffer.ptr != nullptr);
      RTI_TEST_ASSERT(!recv_buffer.is_device);
      
      auto byte_ptr = static_cast<const std::uint8_t*>(recv_buffer.ptr);
      std::string received_message(byte_ptr, byte_ptr + recv_buffer.size_bytes);
      RTI_TEST_ASSERT(received_message == test_message);
      
      reader.freeBuffer(recv_buffer);
    }
  }

  void test_dds_multiple_receivers() {
    const int domain = 78;
    const std::string topic = "dds_multi_rx_" + std::to_string(++channel_counter_);
    const int num_samples = 5;
    
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(50);

    // Create 3 DDS readers (DDS supports multiple subscribers on same topic)
    std::vector<std::unique_ptr<connext_lib::ConnextDDSReader>> readers;
    for (int i = 0; i < 3; ++i) {
      readers.push_back(std::make_unique<connext_lib::ConnextDDSReader>(dds_config, poll_interval_ms));
    }

    // Create writer
    connext_lib::ConnextDDSWriter writer(dds_config, 1024);

    // Wait for all 3 readers to be discovered
    auto* dds_writer_ptr = writer.get_dds_writer();
    RTI_TEST_ASSERT(dds_writer_ptr != nullptr);
    DDSCTestContext_waitForReaders(3, (*dds_writer_ptr)->native_writer(), 5);

    // Track received messages per reader
    std::vector<std::set<std::string>> reader_messages(3);

    // Send samples one at a time, allowing readers to receive
    for (int i = 0; i < num_samples; ++i) {
      std::string test_message = "DDS_MultiRx_Sample_" + std::to_string(i);
      connext_lib::MemoryBufferView send_buffer{
        const_cast<void*>(static_cast<const void*>(test_message.data())),
        test_message.size(),
        false
      };

      // Note: DDS writer returns 1 (writes to topic, not per-reader)
      size_t sent = writer.broadcast(send_buffer);
      RTI_TEST_ASSERT(sent == 1);

      // Give time for propagation and allow each reader to receive
      std::this_thread::sleep_for(100ms);
      
      // Read from all readers
      for (size_t reader_idx = 0; reader_idx < readers.size(); ++reader_idx) {
        connext_lib::MemoryBufferView recv_buffer = readers[reader_idx]->readSamples();
        if (recv_buffer.ptr != nullptr) {
          RTI_TEST_ASSERT(!recv_buffer.is_device);
          
          auto byte_ptr = static_cast<const std::uint8_t*>(recv_buffer.ptr);
          std::string received_message(byte_ptr, byte_ptr + recv_buffer.size_bytes);
          reader_messages[reader_idx].insert(received_message);
          
          readers[reader_idx]->freeBuffer(recv_buffer);
        }
      }
    }

    // Verify all expected messages were received by each reader
    for (size_t reader_idx = 0; reader_idx < readers.size(); ++reader_idx) {
      RTI_TEST_ASSERT(reader_messages[reader_idx].size() == static_cast<size_t>(num_samples));
      for (int i = 0; i < num_samples; ++i) {
        std::string expected_message = "DDS_MultiRx_Sample_" + std::to_string(i);
        RTI_TEST_ASSERT(reader_messages[reader_idx].count(expected_message) == 1);
      }
    }
  }

private:
  ConnextDdsIntegrationTester() : rti::test::Tester("connext_lib_dds_integration_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextDdsIntegrationTester, test_dds_writer_to_reader_roundtrip);
    RTI_TEST_FUNCTION_ADD(ConnextDdsIntegrationTester, test_dds_multiple_messages);
    RTI_TEST_FUNCTION_ADD(ConnextDdsIntegrationTester, test_dds_high_frequency_samples);
    RTI_TEST_FUNCTION_ADD(ConnextDdsIntegrationTester, test_dds_multiple_receivers);
  }

  std::atomic<int> channel_counter_{0};

  friend class rti::test::Singleton<ConnextDdsIntegrationTester>;
};

class ConnextDdsIntegrationTestContainer : public rti::test::TesterContainer,
                                           public rti::test::Singleton<ConnextDdsIntegrationTestContainer> {
private:
  ConnextDdsIntegrationTestContainer() : rti::test::TesterContainer("connext_lib_dds_integration") {
    add_tester<ConnextDdsIntegrationTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConnextDdsIntegrationTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ConnextDdsIntegrationTestContainer::get_instance().run_tests(argc, argv);
}
