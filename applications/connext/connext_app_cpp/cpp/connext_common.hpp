#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <holoscan/holoscan.hpp>
#include <cuda_runtime.h>

namespace connext_demo {

enum class DemoMode {
  kTx,
  kRx
};

struct DemoAppConfig {
  std::string payload = "hello_holoscan";
  int message_count = 0;
  int message_period_ms = 1000;
  bool use_dds = false;
  bool use_ano = false;
  int dds_domain_id = 2;
  std::string dds_topic_name = "ConnextDemoTopic";
  std::string dds_topic_type;
  std::string ano_channel = "connext_demo_channel";
  std::string ano_buffer_id = "connext_demo_buffer";
  std::uint64_t ano_max_payload = 4096;
  std::string destination_reference;
  int discovery_wait_ms = 3000;
  DemoMode mode = DemoMode::kTx;
};

class PayloadSourceOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(PayloadSourceOp)

  PayloadSourceOp() = default;

  void setup(holoscan::OperatorSpec& spec) override;
  void start() override;
  void compute(holoscan::InputContext& input, holoscan::OutputContext& output,
               holoscan::ExecutionContext& context) override;

 private:
  holoscan::Parameter<std::string> base_payload_;
  holoscan::Parameter<bool> use_gpu_memory_;
  int emitted_count_ = 0;
};

class PayloadSinkOp : public holoscan::Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(PayloadSinkOp)

  PayloadSinkOp() = default;

  void setup(holoscan::OperatorSpec& spec) override;
  void set_storage(const std::shared_ptr<std::vector<std::string>>& storage);
  void compute(holoscan::InputContext& input, holoscan::OutputContext& output,
               holoscan::ExecutionContext& context) override;

 private:
  std::shared_ptr<std::vector<std::string>> storage_;
};

class ConnextDemoApp : public holoscan::Application {
 public:
  ConnextDemoApp();

  void compose() override;

  DemoMode mode() const { return demo_config_.mode; }
  const std::vector<std::string>& received_payloads() const { return *received_payloads_; }

 private:
  DemoAppConfig load_demo_config();
  void configure_tx_operators();
  void configure_rx_operators();

  DemoAppConfig demo_config_{};
  std::shared_ptr<std::vector<std::string>> received_payloads_;
};

}  // namespace connext_demo
