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
#include <connext_ops/connext_tx.hpp>
#include <holoscan/core/conditions/gxf/periodic.hpp>
#include <advanced_network/common.h>
#include <cuda_runtime.h>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

using namespace holoscan;
using namespace holoscan::ops;

namespace {

constexpr char kTestPayload[] = "integration_test_payload";

class DummySourceOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(DummySourceOp)
  DummySourceOp() = default;
  
  ~DummySourceOp() {
    if (gpu_ptr_ && use_gpu_.has_value() && use_gpu_.get()) {
      cudaFree(gpu_ptr_);
      gpu_ptr_ = nullptr;
    }
  }
  
  void setup(OperatorSpec& spec) override {
    spec.output<nvidia::gxf::Entity>("output");
    spec.param(use_gpu_, "use_gpu", "Use GPU", "Use GPU memory for ANO", false);
    spec.param(start_delay_ms_, "start_delay_ms", "Start Delay MS", 
               "Delay before starting to send messages (ms)", 0);
    spec.param(test_duration_seconds_, "test_duration_seconds", "Test Duration",
               "Duration to run the test in seconds", 15);
  }
  
  void start() override {
    Operator::start();
    message_counter_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    
    // Apply start delay if configured
    if (start_delay_ms_.get() > 0) {
      std::cout << "[TX] Waiting " << start_delay_ms_.get() 
                << "ms for RX initialization..." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(start_delay_ms_.get()));
    }
    
    // Allocate GPU memory once if needed
    if (use_gpu_.get()) {
      size_t size = strlen(kTestPayload);
      cudaError_t err = cudaMalloc(&gpu_ptr_, size);
      if (err != cudaSuccess) {
        throw std::runtime_error(std::string("cudaMalloc failed: ") + cudaGetErrorString(err));
      }
      
      // Copy data to GPU
      err = cudaMemcpy(gpu_ptr_, kTestPayload, size, cudaMemcpyHostToDevice);
      if (err != cudaSuccess) {
        cudaFree(gpu_ptr_);
        gpu_ptr_ = nullptr;
        throw std::runtime_error(std::string("cudaMemcpy failed: ") + cudaGetErrorString(err));
      }
      
      std::cout << "[TX] GPU memory allocated and initialized for ANO transmission" << std::endl;
    } else {
      std::cout << "[TX] Using CPU memory for DDS transmission" << std::endl;
    }
    
    std::cout << "[TX] Transmission will run for " << test_duration_seconds_.get() << "s" << std::endl;
  }
  
  void compute(InputContext&, OutputContext& output, ExecutionContext& context) override {
    // Check if test duration has elapsed
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    
    if (elapsed_seconds >= test_duration_seconds_.get()) {
      std::cout << "\n[TX] Test duration (" << test_duration_seconds_.get() 
                << "s) elapsed - sent " << message_counter_ << " messages" << std::endl;
      GxfGraphInterrupt(context.context());
      return;
    }
    
    message_counter_++;
    
    auto entity = nvidia::gxf::Entity::New(context.context());
    auto payload_tensor = entity.value().add<nvidia::gxf::Tensor>("payload");
    const char* payload = kTestPayload;
    nvidia::gxf::Shape payload_shape{static_cast<int32_t>(strlen(payload))};
    
    if (use_gpu_.get()) {
      // Use pre-allocated GPU memory (no cleanup needed, done in destructor)
      payload_tensor.value()->wrapMemory(
          payload_shape,
          nvidia::gxf::PrimitiveType::kUnsigned8,
          sizeof(std::uint8_t),
          nvidia::gxf::ComputeTrivialStrides(payload_shape, sizeof(std::uint8_t)),
          nvidia::gxf::MemoryStorageType::kDevice,
          gpu_ptr_,
          nullptr);
    } else {
      // Use CPU memory for DDS
      payload_tensor.value()->wrapMemory(
          payload_shape,
          nvidia::gxf::PrimitiveType::kUnsigned8,
          sizeof(std::uint8_t),
          nvidia::gxf::ComputeTrivialStrides(payload_shape, sizeof(std::uint8_t)),
          nvidia::gxf::MemoryStorageType::kSystem,
          (void*)payload,
          nullptr);
    }
    
    // Log every 5 messages to reduce noise
    if (message_counter_ % 5 == 1) {
      std::cout << "[TX] Sending payload: " << payload 
                << " (message " << message_counter_ << ", " 
                << elapsed_seconds << "s elapsed)" << std::endl;
    }
    
    output.emit(entity.value(), "output");
  }
  
 private:
  Parameter<bool> use_gpu_;
  Parameter<int> start_delay_ms_;
  Parameter<int> test_duration_seconds_;
  void* gpu_ptr_ = nullptr;
  int message_counter_ = 0;
  std::chrono::steady_clock::time_point start_time_;
};

} // namespace

class E2ETxApp : public Application {
 public:
  void compose() override {
    using namespace holoscan;
    
    // Read from config
    auto enable_dds = from_config("enable_dds").as<bool>();
    auto enable_ano = from_config("enable_ano").as<bool>();
    auto domain_id = from_config("domain_id").as<int>();
    auto topic_name = from_config("topic_name").as<std::string>();
    auto ano_channel = from_config("ano_channel").as<std::string>();
    auto test_duration_seconds = from_config("test_duration_seconds").as<int>();
    auto message_period_ms = from_config("message_period_ms").as<int>();
    
    // TX timing configuration
    int tx_start_delay_ms = 0;
    try {
      tx_start_delay_ms = from_config("tx_start_delay_ms").as<int>();
    } catch (...) {
      // Use default if not in config
    }
    
    int discovery_wait_ms = 0;
    try {
      discovery_wait_ms = from_config("discovery_wait_ms").as<int>();
    } catch (...) {
      // Use default if not in config
    }
    
    std::cout << "[TX App] Starting with:" << std::endl;
    std::cout << "  - enable_dds: " << enable_dds << std::endl;
    std::cout << "  - enable_ano: " << enable_ano << std::endl;
    std::cout << "  - domain_id: " << domain_id << std::endl;
    std::cout << "  - topic_name: " << topic_name << std::endl;
    std::cout << "  - ano_channel: " << ano_channel << std::endl;
    std::cout << "  - test_duration_seconds: " << test_duration_seconds << std::endl;
    std::cout << "  - message_period_ms: " << message_period_ms << std::endl;
    std::cout << "  - tx_start_delay_ms: " << tx_start_delay_ms << std::endl;
    std::cout << "  - discovery_wait_ms: " << discovery_wait_ms << std::endl;
    
    // Initialize advanced network if ANO is enabled
    if (enable_ano) {
      try {
        auto adv_net_config = from_config("advanced_network").as<holoscan::advanced_network::NetworkConfig>();
        if (holoscan::advanced_network::adv_net_init(adv_net_config) != holoscan::advanced_network::Status::SUCCESS) {
          throw std::runtime_error("[TX App] Failed to initialize advanced network manager");
        }
        std::cout << "[TX App] Advanced network manager initialized successfully" << std::endl;
      } catch (const std::exception& e) {
        throw std::runtime_error("[TX App] Failed to load or initialize advanced_network configuration: " + std::string(e.what()));
      }
    }
    
    // Use PeriodicCondition to send messages at configured rate
    auto source_condition = make_condition<PeriodicCondition>(
        "tx_periodic",
        std::chrono::milliseconds(message_period_ms));
    
    auto source = make_operator<DummySourceOp>("source",
                                                source_condition,
                                                Arg("use_gpu", enable_ano),
                                                Arg("start_delay_ms", tx_start_delay_ms),
                                                Arg("test_duration_seconds", test_duration_seconds));
    
    auto tx = make_operator<ConnextTxOp>("tx", from_config("tx"));
    
    // Wait for DDS discovery if configured
    if (enable_dds && discovery_wait_ms > 0) {
      std::cout << "[TX App] Waiting " << discovery_wait_ms << "ms for DDS discovery..." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(discovery_wait_ms));
    }
    
    add_flow(source, tx, {{"output", "input"}});
    
    std::cout << "[TX App] Composition complete" << std::endl;
  }
};

int main(int argc, char** argv) {
  auto app = holoscan::make_application<E2ETxApp>();
  
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config.yaml>" << std::endl;
    return 1;
  }
  
  // Load config first to initialize ANO if needed
  app->config(argv[1]);
  app->run();
  
  std::cout << "[TX App] Finished successfully" << std::endl;
  return 0;
}
