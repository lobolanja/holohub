/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/comm/connext_readers.hpp"
#include "connext_lib/config/config.hpp"
#include "test_config_helpers.hpp"
#include "cuda_test_utils.hpp"
#include "net_test_config_helper.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "ndds/ddsctesthelpers/test_context.h"
#include "dds/dds.hpp"
#include "advanced_network/common.h"
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <cstdlib>
#include <mutex>
#include <atomic>

namespace {
using namespace std::chrono_literals;
using namespace connext_lib::test;

class ConnextAnoIntegrationTester : public rti::test::Tester,
                                    public rti::test::Singleton<ConnextAnoIntegrationTester> {
public:
  static constexpr int kTestDomain = 77;
  static constexpr size_t kTestBufferSize = 1024;
  static constexpr int kDiscoveryTimeoutMs = 1000;
  static constexpr int kReceiveTimeoutMs = 2000;
  static constexpr int kMaxRetries = 10;
  static constexpr int kMinExpectedMultiRx = 4;

  // Helper: Send with retry for discovery
  size_t sendWithRetry(connext_lib::ConnextANOWriter* writer,
                       const connext_lib::MemoryBufferView& buffer,
                       size_t expected_destinations = 1,
                       std::chrono::milliseconds retry_interval = 200ms) {
    size_t sent = 0;
    for (int attempt = 0; attempt < kMaxRetries && sent < expected_destinations; ++attempt) {
      sent = writer->broadcast(buffer);
      if (sent < expected_destinations) {
        std::this_thread::sleep_for(retry_interval);
      }
    }
    return sent;
  }

  // Helper: Poll until packet received or timeout
  connext_lib::MemoryBufferView pollUntilReceived(connext_lib::ConnextANOReader* reader,
                                                   int max_attempts = 25,
                                                   std::chrono::milliseconds interval = 5ms) {
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      auto buffer = reader->readSamples();
      if (buffer.ptr != nullptr) {
        return buffer;
      }
      std::this_thread::sleep_for(interval);
    }
    return {nullptr, 0, false};
  }

  // Helper: Validate message matches expected pattern
  bool validateMessage(const connext_lib::MemoryBufferView& buffer,
                       const std::string& expected) {
    if (buffer.size_bytes != expected.size()) return false;
    std::string received = connext_lib::test::copyStringFromGpu(
      buffer.ptr, buffer.size_bytes);
    return received == expected;
  }

  // Helper: Create payload of specific size with identifiable pattern
  std::string createPayload(int sample_index, size_t target_size) {
    std::string header = "ANO_Sample_" + std::to_string(sample_index) + "_";
    if (header.size() >= target_size) {
      return header.substr(0, target_size);
    }
    // Fill remaining space with padding pattern
    std::string payload = header;
    payload.reserve(target_size);
    while (payload.size() < target_size - 1) {
      payload += 'X';
    }
    // Add terminator to reach exact size
    if (payload.size() < target_size) {
      payload += '#';
    }
    return payload;
  }

  // Helper: Start receiver thread that collects samples in parallel
  std::thread startReceiverThread(
      connext_lib::ConnextANOReader* reader,
      std::vector<connext_lib::MemoryBufferView>& received_samples,
      std::atomic<bool>& receiving,
      std::atomic<size_t>& recv_count,
      int max_consecutive_nulls = 200,
      std::chrono::milliseconds poll_interval = 5ms) {
    return std::thread([&, reader, max_consecutive_nulls, poll_interval]() {
      int consecutive_nulls = 0;
      
      while (receiving && consecutive_nulls < max_consecutive_nulls) {
        auto recv_buffer = reader->readSamples();
        if (recv_buffer.ptr != nullptr) {
          received_samples.push_back(recv_buffer);
          recv_count++;
          consecutive_nulls = 0;
        } else {
          consecutive_nulls++;
          std::this_thread::sleep_for(poll_interval);
        }
      }
    });
  }

  // Helper: Wait for all samples with timeout
  bool waitForSamples(
      std::atomic<size_t>& recv_count,
      size_t expected_count,
      std::chrono::seconds timeout = 5s) {
    auto wait_start = std::chrono::high_resolution_clock::now();
    while (recv_count < expected_count) {
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - wait_start);
      if (elapsed >= timeout) {
        std::cout << "Timeout waiting for samples. Received: " << recv_count 
                  << "/" << expected_count << std::endl;
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
  }
  void test_full_ano_writer_to_reader_gpu_roundtrip() {
    const std::string channel = "ano_integration_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "integration_buffer_" + std::to_string(channel_counter_.load());
    
    // Create configurations
    auto reader_net_config = AnoTestConfigHelper::CreateDefaultReaderNetworkConfig();
    auto writer_net_config = AnoTestConfigHelper::CreateDefaultWriterNetworkConfig();
    auto reader_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, buffer_id, kTestBufferSize, reader_net_config);
    auto writer_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, buffer_id, kTestBufferSize, writer_net_config);
    auto dds_config = AnoTestConfigHelper::CreateStandardDdsConfig(kTestDomain, channel);
    auto poll_interval_ms = AnoTestConfigHelper::GetStandardPollInterval();

    auto reader = std::make_unique<connext_lib::ConnextANOReader>(reader_ano_config, dds_config, poll_interval_ms);
    auto writer = std::make_unique<connext_lib::ConnextANOWriter>(writer_ano_config, dds_config, poll_interval_ms);

    std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryTimeoutMs));

    // Prepare test data
    std::string test_message = "Hello from GPU-direct ANO integration test!";
    connext_lib::test::CudaMemoryGuard gpu_guard = connext_lib::test::copyToGpu(
      test_message.c_str(), test_message.size());
    connext_lib::MemoryBufferView send_buffer{gpu_guard.get(), test_message.size(), true};

    // Send with retry
    size_t sent = sendWithRetry(writer.get(), send_buffer, 1, poll_interval_ms);
    RTI_TEST_ASSERT(sent >= 1);

    // Receive with polling
    auto recv_buffer = pollUntilReceived(reader.get(), 25, 50ms);

    // Verify
    RTI_TEST_ASSERT(recv_buffer.ptr != nullptr);
    RTI_TEST_ASSERT(recv_buffer.is_device);
    RTI_TEST_ASSERT(validateMessage(recv_buffer, test_message));
    
    reader->freeBuffer(recv_buffer);
  }

  void test_ano_high_frequency_samples() {
    const std::string channel = "ano_hf_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "hf_buffer_" + std::to_string(channel_counter_.load());
    const int num_samples = 2000;
    const size_t payload_size = 1000;
    
    // Create configurations
    auto reader_net_config = AnoTestConfigHelper::CreateDefaultReaderNetworkConfig();
    auto writer_net_config = AnoTestConfigHelper::CreateDefaultWriterNetworkConfig();
    
    // Use IMMEDIATE mode for maximum throughput (blocks per-send to ensure GPU completion)
    // This provides better throughput than BATCH for single-threaded scenarios
    writer_net_config.set_send_mode(holoscan::ops::SendMode::IMMEDIATE);
    
    auto reader_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, buffer_id, kTestBufferSize, reader_net_config);
    auto writer_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, buffer_id, kTestBufferSize, writer_net_config);
    auto dds_config = AnoTestConfigHelper::CreateStandardDdsConfig(kTestDomain, channel);
    auto poll_interval_ms = AnoTestConfigHelper::GetStandardPollInterval();

    auto reader = std::make_unique<connext_lib::ConnextANOReader>(reader_ano_config, dds_config, poll_interval_ms);
    auto writer = std::make_unique<connext_lib::ConnextANOWriter>(writer_ano_config, dds_config, poll_interval_ms);

    std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryTimeoutMs));

    std::cout << "\n=== High Frequency Throughput Test ===" << std::endl;
    std::cout << "Samples: " << num_samples << ", Payload size: " << payload_size << " bytes" << std::endl;
    
    // Pre-allocate a single GPU buffer (reused for all sends)
    std::string test_message = createPayload(0, payload_size);
    connext_lib::test::CudaMemoryGuard gpu_guard = connext_lib::test::copyToGpu(
      test_message.c_str(), test_message.size());
    connext_lib::MemoryBufferView send_buffer{gpu_guard.get(), test_message.size(), true};
    
    // Start receiver thread to process samples in parallel
    std::vector<connext_lib::MemoryBufferView> received_samples;
    std::atomic<bool> receiving{true};
    std::atomic<size_t> recv_count{0};
    
    auto recv_start = std::chrono::high_resolution_clock::now();
    std::thread receiver_thread = startReceiverThread(
      reader.get(), received_samples, receiving, recv_count);
    
    // Start throughput measurement
    auto send_start = std::chrono::high_resolution_clock::now();
    
    // Send all samples as fast as possible
    // In IMMEDIATE mode, each send() blocks until GPU completes and burst is transmitted
    for (int i = 0; i < num_samples; ++i) {
      // Send with retry on first sample for discovery
      size_t sent = (i == 0) ? sendWithRetry(writer.get(), send_buffer) : writer->broadcast(send_buffer);
      RTI_TEST_ASSERT(sent >= 1);
    }
    
    auto send_end = std::chrono::high_resolution_clock::now();
    auto send_duration = std::chrono::duration_cast<std::chrono::microseconds>(send_end - send_start);
    
    std::cout << "Send phase completed in " << (send_duration.count() / 1000.0) << " ms" << std::endl;
    
    // Wait for all samples to be received
    auto wait_start = std::chrono::high_resolution_clock::now();
    while (recv_count < num_samples) {
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - wait_start);
      if (elapsed.count() > 5) {
        std::cout << "Timeout waiting for samples. Received: " << recv_count << "/" << num_samples << std::endl;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    receiving = false;
    receiver_thread.join();
    
    auto recv_end = std::chrono::high_resolution_clock::now();
    auto recv_duration = std::chrono::duration_cast<std::chrono::microseconds>(recv_end - recv_start);
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(recv_end - send_start);
    
    // Calculate throughput
    size_t total_bytes = received_samples.size() * payload_size;
    double total_seconds = total_duration.count() / 1000000.0;
    double send_seconds = send_duration.count() / 1000000.0;
    double recv_seconds = recv_duration.count() / 1000000.0;
    
    double throughput_mbps = (total_bytes * 8.0) / (total_seconds * 1000000.0);
    double send_rate = num_samples / send_seconds;
    double recv_rate = received_samples.size() / recv_seconds;
    
    std::cout << "\n=== Throughput Results ===" << std::endl;
    std::cout << "Sent: " << num_samples << " samples in " << send_seconds << " s (" 
              << send_rate << " samples/s)" << std::endl;
    std::cout << "Received: " << received_samples.size() << " samples in " << recv_seconds << " s (" 
              << recv_rate << " samples/s)" << std::endl;
    std::cout << "Total time: " << total_seconds << " s" << std::endl;
    std::cout << "Total data: " << (total_bytes / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "Throughput: " << throughput_mbps << " Mbps (" 
              << (total_bytes / 1024.0 / 1024.0 / total_seconds) << " MB/s)" << std::endl;
    std::cout << "Loss rate: " << ((num_samples - received_samples.size()) * 100.0 / num_samples) 
              << "%" << std::endl;
    std::cout << "=========================\n" << std::endl;

    // Verify we received at least 95% of samples
    RTI_TEST_ASSERT(received_samples.size() >= num_samples * 0.95);

    // Validate a subset of received samples (all should match the same pattern)
    std::string expected_message = createPayload(0, payload_size);
    int validation_step = std::max(1, static_cast<int>(received_samples.size() / 10));
    for (size_t idx = 0; idx < received_samples.size(); idx += validation_step) {
      auto& recv_buffer = received_samples[idx];
      RTI_TEST_ASSERT(recv_buffer.ptr != nullptr);
      RTI_TEST_ASSERT(recv_buffer.is_device);
      RTI_TEST_ASSERT(validateMessage(recv_buffer, expected_message));
    }
    
    // Free all buffers
    for (auto& buffer : received_samples) {
      reader->freeBuffer(buffer);
    }
  }

  void test_ano_batch_mode() {
    const std::string channel = "ano_batch_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "batch_buffer_" + std::to_string(channel_counter_.load());
    const int num_samples = 500;
    const size_t payload_size = 1000;
    
    // Create configurations
    auto reader_net_config = AnoTestConfigHelper::CreateDefaultReaderNetworkConfig();
    auto writer_net_config = AnoTestConfigHelper::CreateDefaultWriterNetworkConfig();
    
    // Use BATCH mode - demonstrates enqueue without blocking, requires explicit flush
    writer_net_config.set_send_mode(holoscan::ops::SendMode::BATCH);
    
    auto reader_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, buffer_id, kTestBufferSize, reader_net_config);
    auto writer_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, buffer_id, kTestBufferSize, writer_net_config);
    auto dds_config = AnoTestConfigHelper::CreateStandardDdsConfig(kTestDomain, channel);
    auto poll_interval_ms = AnoTestConfigHelper::GetStandardPollInterval();

    auto reader = std::make_unique<connext_lib::ConnextANOReader>(reader_ano_config, dds_config, poll_interval_ms);
    auto writer = std::make_unique<connext_lib::ConnextANOWriter>(writer_ano_config, dds_config, poll_interval_ms);

    std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryTimeoutMs));

    std::cout << "\n=== BATCH Mode Test ===" << std::endl;
    std::cout << "Samples: " << num_samples << ", Payload size: " << payload_size << " bytes" << std::endl;
    
    // Pre-allocate GPU buffer (reused for all sends)
    std::string test_message = createPayload(0, payload_size);
    connext_lib::test::CudaMemoryGuard gpu_guard = connext_lib::test::copyToGpu(
      test_message.c_str(), test_message.size());
    connext_lib::MemoryBufferView send_buffer{gpu_guard.get(), test_message.size(), true};
    
    // Start receiver thread to process samples in parallel
    std::vector<connext_lib::MemoryBufferView> received_samples;
    std::atomic<bool> receiving{true};
    std::atomic<size_t> recv_count{0};
    
    auto recv_start = std::chrono::high_resolution_clock::now();
    std::thread receiver_thread = startReceiverThread(
      reader.get(), received_samples, receiving, recv_count);
    
    // In BATCH mode, send in batches with periodic flushes
    // This allows GPU-completed bursts to be transmitted while preparing new ones
    constexpr int BATCH_SIZE = 100;
    const int num_batches = (num_samples + BATCH_SIZE - 1) / BATCH_SIZE;
    
    auto send_start = std::chrono::high_resolution_clock::now();
    
    for (int batch = 0; batch < num_batches; ++batch) {
      int batch_start = batch * BATCH_SIZE;
      int batch_end = std::min(batch_start + BATCH_SIZE, num_samples);
      
      // Send batch (bursts are enqueued without blocking)
      for (int i = batch_start; i < batch_end; ++i) {
        size_t sent = (i == 0) ? sendWithRetry(writer.get(), send_buffer) : writer->broadcast(send_buffer);
        RTI_TEST_ASSERT(sent >= 1);
      }
      
      // Flush to transmit ready bursts (short timeout - only send what's ready)
      writer->flush(50);
    }
    
    // Final flush to ensure all remaining bursts are sent
    int final_flushed = writer->flush(5000);
    std::cout << "Final flush sent " << final_flushed << " remaining bursts" << std::endl;
    
    auto send_end = std::chrono::high_resolution_clock::now();
    auto send_duration = std::chrono::duration_cast<std::chrono::microseconds>(send_end - send_start);
    
    std::cout << "Send phase completed in " << (send_duration.count() / 1000.0) << " ms" << std::endl;
    
    // Wait for all samples to be received
    waitForSamples(recv_count, num_samples, 5s);
    
    receiving = false;
    receiver_thread.join();
    
    auto recv_end = std::chrono::high_resolution_clock::now();
    auto recv_duration = std::chrono::duration_cast<std::chrono::microseconds>(recv_end - recv_start);
    
    // Calculate metrics
    double send_seconds = send_duration.count() / 1000000.0;
    double recv_seconds = recv_duration.count() / 1000000.0;
    double send_rate = num_samples / send_seconds;
    double recv_rate = received_samples.size() / recv_seconds;
    
    std::cout << "Sent: " << num_samples << " samples (" << send_rate << " samples/s)" << std::endl;
    std::cout << "Received: " << received_samples.size() << " samples (" << recv_rate << " samples/s)" << std::endl;
    std::cout << "Loss rate: " << ((num_samples - received_samples.size()) * 100.0 / num_samples) << "%" << std::endl;
    std::cout << "========================\n" << std::endl;

    // Verify we received all samples
    RTI_TEST_ASSERT(received_samples.size() == num_samples);

    // Validate samples
    std::string expected_message = createPayload(0, payload_size);
    for (auto& recv_buffer : received_samples) {
      RTI_TEST_ASSERT(recv_buffer.ptr != nullptr);
      RTI_TEST_ASSERT(recv_buffer.is_device);
      RTI_TEST_ASSERT(validateMessage(recv_buffer, expected_message));
      reader->freeBuffer(recv_buffer);
    }
  }

  void test_ano_multiple_receivers() {
    // Test multiple receivers with dedicated RX queues
    // Each receiver gets its own queue (0, 1, 2) and UDP destination port (4096, 4097, 4098)
    // Hardware flow steering directs packets to the correct queue based on UDP dst port
    //
    // NOTE: This test requires physical NICs because loopback mode doesn't configure
    // multiple RX queues (only queue 0 exists). In loopback, all 3 receivers would
    // compete for packets from the same queue, causing unpredictable packet distribution.
    if (!AnoInitializer::IsPhysicalMode()) {
      std::cout << "\n=== SKIPPING test_ano_multiple_receivers ===" << std::endl;
      std::cout << "This test requires physical NICs with multiple RX queues." << std::endl;
      std::cout << "Loopback mode only configures a single RX queue (queue 0)." << std::endl;
      std::cout << "\nTo run this test, set environment variables:" << std::endl;
      std::cout << "  export TEST_TX_NIC_PCIE=\"<tx_pcie_address>\"" << std::endl;
      std::cout << "  export TEST_RX_NIC_PCIE=\"<rx_pcie_address>\"" << std::endl;
      std::cout << "  export TEST_ETH_DST_MAC=\"<rx_mac_address>\"" << std::endl;
      std::cout << "============================================\n" << std::endl;
      return;
    }
    
    std::cout << "\n=== Testing multiple receivers with dedicated queues ===" << std::endl;
    std::cout << "Mode: Physical NICs" << std::endl;
    
    const std::string channel = "ano_multi_rx_" + std::to_string(++channel_counter_);
    const int num_samples = 5;
    const int num_readers = 3;
    
    // Create configurations
    auto writer_net_config = AnoTestConfigHelper::CreateDefaultWriterNetworkConfig();
    auto writer_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, "writer_buffer", kTestBufferSize, writer_net_config);
    auto writer_dds_config = AnoTestConfigHelper::CreateStandardDdsConfig(kTestDomain, channel);
    auto poll_interval_ms = AnoTestConfigHelper::GetStandardPollInterval();
    
    // Create multiple reader configurations (one per receiver with dedicated port/queue)
    auto reader_net_configs = AnoTestConfigHelper::CreateMultipleReaderConfigs(num_readers);
    std::vector<std::unique_ptr<connext_lib::ConnextANOReader>> readers;
    std::vector<std::string> buffer_ids = {"buffer_a", "buffer_b", "buffer_c"};
    
    for (size_t i = 0; i < num_readers; ++i) {
      auto reader_ano_config = AnoTestConfigHelper::CreateStandardAnoConfig(channel, buffer_ids[i], kTestBufferSize, reader_net_configs[i]);
      auto reader_dds_config = AnoTestConfigHelper::CreateStandardDdsConfig(kTestDomain, channel);
      readers.push_back(std::make_unique<connext_lib::ConnextANOReader>(
        reader_ano_config, reader_dds_config, poll_interval_ms));
    }

    auto writer = std::make_unique<connext_lib::ConnextANOWriter>(
      writer_ano_config, writer_dds_config, poll_interval_ms);

    std::this_thread::sleep_for(std::chrono::milliseconds(kDiscoveryTimeoutMs));

    // Track received samples for each reader
    std::vector<std::vector<connext_lib::MemoryBufferView>> all_received_samples(3);

    // Send 5 samples to all 3 receivers, poll during send
    for (int i = 0; i < num_samples; ++i) {
      std::string test_message = "ANO_MultiRx_Sample_" + std::to_string(i);
      connext_lib::test::CudaMemoryGuard gpu_guard = connext_lib::test::copyToGpu(
        test_message.c_str(), test_message.size());
      connext_lib::MemoryBufferView send_buffer{gpu_guard.get(), test_message.size(), true};

      // Broadcast should reach 3 destinations
      size_t sent = (i == 0) ? sendWithRetry(writer.get(), send_buffer, 3) : writer->broadcast(send_buffer);
      RTI_TEST_ASSERT(sent >= 3);

      // Poll each reader (2 times per sample)
      for (size_t reader_idx = 0; reader_idx < readers.size(); ++reader_idx) {

        auto recv_buffer = pollUntilReceived(readers[reader_idx].get(), 10, 5ms);
        RTI_TEST_ASSERT(recv_buffer.ptr != nullptr);
        all_received_samples[reader_idx].push_back(recv_buffer);
        
      }

      std::this_thread::sleep_for(20ms);
    }

    // With separate queues, each reader should receive its designated packets
    size_t total_received = 0;
    for (size_t reader_idx = 0; reader_idx < readers.size(); ++reader_idx) {
      std::cout<<"[info] Reader " << reader_idx << " (queue " << reader_idx << ", port " << (4096 + reader_idx) 
                << ") received " << all_received_samples[reader_idx].size() << " samples." << std::endl;
      total_received += all_received_samples[reader_idx].size();
    }
    
    std::cout << "[info] Total samples received across all readers: " << total_received 
              << " (expected: " << (num_samples * 3) << ")" << std::endl;
    
    // Verify each reader received at least kMinExpectedMultiRx samples (now with separate queues)
    for (size_t reader_idx = 0; reader_idx < readers.size(); ++reader_idx) {
      RTI_TEST_ASSERT(all_received_samples[reader_idx].size() >= kMinExpectedMultiRx);
      
      // Validate received samples (order may vary)
      for (auto& recv_buffer : all_received_samples[reader_idx]) {
        RTI_TEST_ASSERT(recv_buffer.ptr != nullptr);
        RTI_TEST_ASSERT(recv_buffer.is_device);
        
        // Verify it matches one of the sent patterns
        bool matched = false;
        for (int i = 0; i < num_samples; ++i) {
          std::string expected_message = "ANO_MultiRx_Sample_" + std::to_string(i);
          if (validateMessage(recv_buffer, expected_message)) {
            matched = true;
            break;
          }
        }
        RTI_TEST_ASSERT(matched);
      }
      
      // Free all buffers for this reader
      for (auto& buffer : all_received_samples[reader_idx]) {
        readers[reader_idx]->freeBuffer(buffer);
      }
    }
  }

private:
  ConnextAnoIntegrationTester() : rti::test::Tester("connext_lib_ano_integration_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextAnoIntegrationTester, test_full_ano_writer_to_reader_gpu_roundtrip);
    RTI_TEST_FUNCTION_ADD(ConnextAnoIntegrationTester, test_ano_high_frequency_samples);
    RTI_TEST_FUNCTION_ADD(ConnextAnoIntegrationTester, test_ano_batch_mode);
    RTI_TEST_FUNCTION_ADD(ConnextAnoIntegrationTester, test_ano_multiple_receivers);
  }

  std::atomic<int> channel_counter_{0};

  friend class rti::test::Singleton<ConnextAnoIntegrationTester>;
};

class ConnextAnoIntegrationTestContainer : public rti::test::TesterContainer,
                                           public rti::test::Singleton<ConnextAnoIntegrationTestContainer> {
private:
  ConnextAnoIntegrationTestContainer() : rti::test::TesterContainer("connext_lib_ano_integration") {
    add_tester<ConnextAnoIntegrationTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    
    // Initialize ANO if hardware is available
    if (!AnoInitializer::Initialize()) {
      std::cout << "Warning: ANO initialization failed or hardware not available. "
                << "Tests may fail if they require ANO hardware." << std::endl;
    }
    
    return rti::test::TesterContainer::on_tests_begin(setting);
  }
  
  bool on_tests_end(const RTITestSetting& setting) override {
    bool result = rti::test::TesterContainer::on_tests_end(setting);
    AnoInitializer::Shutdown();
    return result;
  }

  friend class rti::test::Singleton<ConnextAnoIntegrationTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ConnextAnoIntegrationTestContainer::get_instance().run_tests(argc, argv);
}
