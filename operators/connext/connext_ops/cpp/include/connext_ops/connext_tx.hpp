#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <holoscan/holoscan.hpp>

#include "connext_lib.hpp"

namespace holoscan::ops {

/**
 * Native Connext transmit operator placeholder.
 *
 * Stores configuration parameters and owns the transport handles that will be
 * wired up once the Connext helpers expose the necessary APIs.
 */
class ConnextTxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(ConnextTxOp)

  void setup(OperatorSpec& spec) override;
  void start() override;
  void stop() override;
  void compute(InputContext& input,
               OutputContext& output,
               ExecutionContext& context) override;

 private:
  void refresh_configs();

  Parameter<holoscan::IOSpec*> input_;
  Parameter<bool> enable_dds_;
  Parameter<int> domain_id_;
  Parameter<std::string> topic_name_;
  Parameter<std::string> topic_type_name_;
  Parameter<bool> enable_ano_;
  Parameter<std::string> ano_channel_;
  Parameter<uint64_t> ano_max_payload_;
  Parameter<std::string> destination_reference_;

  connext_lib::DdsConfig dds_config_{};
  connext_lib::AnoConfig ano_config_{};

  std::unique_ptr<connext_lib::ConnextDDSWriter> dds_writer_;
  std::unique_ptr<connext_lib::ConnextANOWriter> ano_writer_; 
};

}  // namespace holoscan::ops
