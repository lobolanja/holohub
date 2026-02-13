/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <holoscan/holoscan.hpp>
#include <connext_ops/connext_rx.hpp>
#include <holoscan/core/conditions/gxf/periodic.hpp>
#include <advanced_network/common.h>
#include <cuda_runtime.h>
#include <string>
#include <cstring>
#include <chrono>
#include <iomanip>

using namespace holoscan;
using namespace holoscan::ops;

namespace {

class TestReceiverOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TestReceiverOp)
  TestReceiverOp() = default;
  
  void setup(OperatorSpec& spec) override {
    spec.input<nvidia::gxf::Entity>("input");
    spec.param(expect_gpu_, "expect_gpu", "Expect GPU", "Expect GPU tensor for validation", false);
    spec.param(expected_payload_, "expected_payload", "Expected Payload", 
               "Expected payload string for validation", std::string(""));
    spec.param(test_duration_seconds_, "test_duration_seconds", "Test Duration", 
               "Duration to run the test in seconds", 15);
    spec.param(expected_message_count_, "expected_message_count", "Expected Message Count", 
               "Expected number of messages based on TX rate and duration", 30);
    spec.param(success_threshold_percent_, "success_threshold_percent", "Success Threshold", 
               "Percentage of expected messages that must be received for success", 95);
  }
  
  void start() override {
    Operator::start();
    start_time_ = std::chrono::steady_clock::now();
    received_count_ = 0;
    
    if (expect_gpu_.get()) {
      // Check CUDA availability
      int device_count = 0;
      cudaError_t err = cudaGetDeviceCount(&device_count);
      if (err != cudaSuccess || device_count == 0) {
        throw std::runtime_error(
            std::string("[RX] CUDA GPU required for ANO test but not available: ") + 
            cudaGetErrorString(err));
      }
      std::cout << "[RX] GPU available for ANO reception (devices: " << device_count << ")" << std::endl;
    } else {
      std::cout << "[RX] Using CPU memory for DDS reception" << std::endl;
    }
    
    int min_required = (expected_message_count_.get() * success_threshold_percent_.get()) / 100;
    std::cout << "[RX] Time-based test - running for " << test_duration_seconds_.get() << "s" << std::endl;
    std::cout << "[RX] Expected messages: " << expected_message_count_.get() 
              << ", minimum required (" << success_threshold_percent_.get() << "%): " 
              << min_required << std::endl;
  }
  
  void compute(InputContext& input, OutputContext&, ExecutionContext& context) override {
    // Check if test duration has elapsed
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    
    if (elapsed_seconds >= test_duration_seconds_.get()) {
      // Test duration complete - evaluate results
      int min_required = (expected_message_count_.get() * success_threshold_percent_.get()) / 100;
      float success_rate = (float)received_count_ / expected_message_count_.get() * 100.0f;
      
      std::cout << "\n[RX] Test duration (" << test_duration_seconds_.get() << "s) elapsed" << std::endl;
      std::cout << "[RX] Received " << received_count_ << "/" << expected_message_count_.get() 
                << " messages (" << std::fixed << std::setprecision(1) << success_rate << "%)" << std::endl;
      
      if (received_count_ >= min_required) {
        std::cout << "[RX] ✓ Test PASSED - received >= " << success_threshold_percent_.get() 
                  << "% of expected messages" << std::endl;
      } else {
        std::cout << "[RX] ✗ Test FAILED - received only " << received_count_ << "/" 
                  << min_required << " minimum required messages" << std::endl;
      }
      
      GxfGraphInterrupt(context.context());
      return;
    }
    
    // Try to receive entity (may not be available yet)
    auto maybe_entity = input.receive<nvidia::gxf::Entity>("input");
    if (!maybe_entity) {
      // No data this cycle - normal with periodic condition
      return;
    }
    
    auto entity = maybe_entity.value();
    auto tensor = entity.get<nvidia::gxf::Tensor>("payload");
    
    if (tensor) {
      auto tensor_handle = tensor.value();
      auto data_expected = tensor_handle->data<uint8_t>();
      
      if (data_expected) {
        const auto* data = data_expected.value();
        const auto& shape = tensor_handle->shape();
        size_t size = shape.dimension(0);
        
        // Check if data is on GPU or CPU
        const bool is_gpu = 
            (tensor_handle->storage_type() == nvidia::gxf::MemoryStorageType::kDevice);
        
        // Validate expected memory type
        if (expect_gpu_.has_value() && expect_gpu_.get() && !is_gpu) {
          throw std::runtime_error(
              "[RX] ERROR: Expected GPU tensor but received CPU tensor");
        }
        
        std::string received_payload;
        if (is_gpu) {
          // Copy from GPU to CPU for validation
          std::vector<uint8_t> cpu_data(size);
          cudaError_t err = cudaMemcpy(cpu_data.data(), data, size, cudaMemcpyDeviceToHost);
          if (err == cudaSuccess) {
            received_payload.assign(reinterpret_cast<const char*>(cpu_data.data()), size);
          } else {
            throw std::runtime_error(
                std::string("[RX] cudaMemcpy failed: ") + cudaGetErrorString(err));
          }
        } else {
          // Data already on CPU
          received_payload.assign(reinterpret_cast<const char*>(data), size);
        }
        
        received_count_++;
        
        // Log every 5 messages to reduce noise
        if (received_count_ % 5 == 1) {
          std::cout << "[RX] Received payload: " << received_payload 
                    << " (" << received_count_ << " msgs, "
                    << elapsed_seconds << "s elapsed)" << std::endl;
        }
        
        // Validate payload content
        if (!expected_payload_.get().empty() && received_payload != expected_payload_.get()) {
          throw std::runtime_error(
              std::string("[RX] ERROR: Payload mismatch - expected '") + 
              expected_payload_.get() + "', got '" + received_payload + "'");
        }
      }
    }
  }
  
 private:
  std::chrono::steady_clock::time_point start_time_;
  int received_count_ = 0;
  Parameter<bool> expect_gpu_;
  Parameter<std::string> expected_payload_;
  Parameter<int> test_duration_seconds_;
  Parameter<int> expected_message_count_;
  Parameter<int> success_threshold_percent_;
};

} // namespace

class E2ERxApp : public Application {
 public:
  void compose() override {
    using namespace holoscan;
    
    // Read from config
    auto enable_dds = from_config("enable_dds").as<bool>();
    auto enable_ano = from_config("enable_ano").as<bool>();
    auto domain_id = from_config("domain_id").as<int>();
    auto topic_name = from_config("topic_name").as<std::string>();
    auto ano_channel = from_config("ano_channel").as<std::string>();
    auto expected_payload = from_config("expected_payload").as<std::string>();
    auto test_duration_seconds = from_config("test_duration_seconds").as<int>();
    auto expected_message_count = from_config("expected_message_count").as<int>();
    auto success_threshold_percent = from_config("success_threshold_percent").as<int>();
    
    // RX polling configuration
    int rx_poll_period_ms = 100;  // Default: 100ms for DDS
    try {
      rx_poll_period_ms = from_config("rx_poll_period_ms").as<int>();
    } catch (...) {
      // Use default if not in config
    }
    
    std::cout << "[RX App] Starting with:" << std::endl;
    std::cout << "  - enable_dds: " << enable_dds << std::endl;
    std::cout << "  - enable_ano: " << enable_ano << std::endl;
    std::cout << "  - domain_id: " << domain_id << std::endl;
    std::cout << "  - topic_name: " << topic_name << std::endl;
    std::cout << "  - ano_channel: " << ano_channel << std::endl;
    std::cout << "  - test_duration_seconds: " << test_duration_seconds << std::endl;
    std::cout << "  - expected_message_count: " << expected_message_count << std::endl;
    std::cout << "  - success_threshold_percent: " << success_threshold_percent << std::endl;
    std::cout << "  - expected_payload: " << expected_payload << std::endl;
    std::cout << "  - rx_poll_period_ms: " << rx_poll_period_ms << std::endl;
    
    // Initialize advanced network if ANO is enabled
    if (enable_ano) {
      try {
        auto adv_net_config = from_config("advanced_network").as<holoscan::advanced_network::NetworkConfig>();
        if (holoscan::advanced_network::adv_net_init(adv_net_config) != holoscan::advanced_network::Status::SUCCESS) {
          throw std::runtime_error("[RX App] Failed to initialize advanced network manager");
        }
        std::cout << "[RX App] Advanced network manager initialized successfully" << std::endl;
      } catch (const std::exception& e) {
        throw std::runtime_error("[RX App] Failed to load or initialize advanced_network configuration: " + std::string(e.what()));
      }
    }
    
    // Use PeriodicCondition to poll continuously
    auto rx_condition = make_condition<PeriodicCondition>(
        "rx_poll", 
        std::chrono::milliseconds(rx_poll_period_ms));
    
    auto rx = make_operator<ConnextRxOp>("rx", rx_condition, from_config("rx"));
    
    // Sink also needs PeriodicCondition to check timeout
    auto sink_condition = make_condition<PeriodicCondition>(
        "sink_poll",
        std::chrono::milliseconds(rx_poll_period_ms));
    
    auto sink = make_operator<TestReceiverOp>("sink",
                                              sink_condition,
                                              Arg("expect_gpu", enable_ano),
                                              Arg("expected_payload", expected_payload),
                                              Arg("test_duration_seconds", test_duration_seconds),
                                              Arg("expected_message_count", expected_message_count),
                                              Arg("success_threshold_percent", success_threshold_percent));
    
    add_flow(rx, sink, {{"output", "input"}});
    
    std::cout << "[RX App] Composition complete" << std::endl;
  }
};

int main(int argc, char** argv) {
  auto app = holoscan::make_application<E2ERxApp>();
  
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config.yaml>" << std::endl;
    return 1;
  }
  
  // Load config first to initialize ANO if needed
  app->config(argv[1]);
  app->run();
  
  std::cout << "[RX App] Finished successfully" << std::endl;
  return 0;
}
