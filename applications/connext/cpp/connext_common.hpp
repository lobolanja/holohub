#pragma once

#include <cstdint>
#include <getopt.h>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <holoscan/holoscan.hpp>

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

void print_usage(const char* program_name);
bool parse_arguments(int argc, char** argv, DemoAppConfig& config, bool& show_usage);

int run_sender(const DemoAppConfig& config);
int run_receiver(const DemoAppConfig& config);

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

}  // namespace connext_demo
