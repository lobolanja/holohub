#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <holoscan/holoscan.hpp>

#include "connext_lib.hpp"

namespace holoscan::ops {

/**
 * Native Connext receive operator.
 *
 * The class currently exposes configuration hooks and keeps placeholders for
 * the DDS/ANO plumbing. Actual transport wiring will be added in follow-up
 * changes.
 */
class ConnextRxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(ConnextRxOp)
  ConnextRxOp() = default;

  void setup(OperatorSpec& spec) override;
  void start() override;
  void stop() override;
  void compute(InputContext& input,
               OutputContext& output,
               ExecutionContext& context) override;

 private:
  void refresh_configs();

  Parameter<holoscan::IOSpec*> output_;
  Parameter<bool> enable_dds_;
  Parameter<int> domain_id_;
  Parameter<std::string> topic_name_;
  Parameter<std::string> topic_type_name_;
  Parameter<bool> enable_ano_;
  Parameter<std::string> ano_channel_;
  Parameter<std::string> ano_buffer_id_;
  Parameter<uint64_t> ano_max_payload_;

  connext_lib::DdsConfig dds_config_{};
  connext_lib::AnoConfig ano_config_{};

  std::unique_ptr<connext_lib::ConnextDDSReader> dds_reader_;
  std::unique_ptr<connext_lib::ConnextANOReader> ano_reader_;
};

}  // namespace holoscan::ops
