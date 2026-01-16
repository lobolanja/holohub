/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <holoscan/holoscan.hpp>
#include "tensor_generator_op.h"
#include "tensor_network_tx_op.h"
#include "tensor_network_rx_op.h"
#include "tensor_printer_op.h"
#include "advanced_network/kernels.h"

class DemoAnoTxRxApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    // Initialize Advanced Network manager (single DPDK instance)
    auto adv_net_config = from_config("advanced_network").as<NetworkConfig>();
    if (advanced_network::adv_net_init(adv_net_config) != advanced_network::Status::SUCCESS) {
      HOLOSCAN_LOG_ERROR("Failed to configure the Advanced Network manager");
      exit(1);
    }
    HOLOSCAN_LOG_INFO("Configured the Advanced Network manager with both TX and RX");

    // TX pipeline: Generator -> Network TX
    auto generator = make_operator<ops::TensorGeneratorOp>(
        "tensor_generator",
        from_config("tensor_generator"),
        make_condition<PeriodicCondition>("periodic", Arg("recess_period") = std::string("1s")),
        Arg("base_message") = std::string("hello world"),
        Arg("tensor_size") = static_cast<size_t>(1000),
        Arg("gpu_device") = 0);

    auto tx_op = make_operator<ops::TensorNetworkTxOp>(
        "tensor_network_tx",
        from_config("tensor_network_tx"));

    // RX pipeline: Network RX -> Printer
    auto rx_op = make_operator<ops::TensorNetworkRxOp>(
        "tensor_network_rx",
        from_config("tensor_network_rx"));
    
    auto printer = make_operator<ops::TensorPrinterOp>(
        "tensor_printer",
        from_config("tensor_printer"));

    // Connect TX pipeline
    add_flow(generator, tx_op, {{"tensor_out", "tensor_in"}});

    // Connect RX pipeline
    add_flow(rx_op, printer, {{"tensor_out", "tensor_in"}});

    HOLOSCAN_LOG_INFO("Added complete TX and RX pipelines to the same application");
  }
};

int main(int argc, char** argv) {
  auto app = holoscan::make_application<DemoAnoTxRxApp>();
  
  if (argc < 2) {
    HOLOSCAN_LOG_ERROR("Usage: {} <config_file.yaml>", argv[0]);
    return 1;
  }

  app->config(argv[1]);
  app->run();

  return 0;
}
