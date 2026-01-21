/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <holoscan/holoscan.hpp>
#include <holoscan/core/conditions/gxf/periodic.hpp>
#include "advanced_network/common.h"
#include "../include/tensor_generator_op.h"
#include "../include/tensor_network_tx_op.h"

namespace holoscan::apps {

/**
 * @brief Demo application that generates tensors and sends them over the network
 * 
 * This application creates a simple pipeline:
 * TensorGeneratorOp -> TensorNetworkTxOp
 * 
 * It generates tensors with "hello world #N" strings and transmits them
 * using the Advanced Network library with GPU-only mode.
 */
class DemoANOTxApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

     auto adv_net_config = from_config("advanced_network").as<advanced_network::NetworkConfig>();
    if (advanced_network::adv_net_init(adv_net_config) != advanced_network::Status::SUCCESS) {
      HOLOSCAN_LOG_ERROR("Failed to configure the Advanced Network manager");
      exit(1);
    }
    HOLOSCAN_LOG_INFO("Configured the Advanced Network manager");

    const auto mgr_type = advanced_network::get_manager_type(config());

    HOLOSCAN_LOG_INFO("Using Advanced Network manager {}",
                      advanced_network::manager_type_to_string(mgr_type));
     // DPDK is the default manager backend
    if (mgr_type == advanced_network::ManagerType::DPDK) {
#if ANO_MGR_DPDK
    /// Generator operator (generates tensors with "hello world #N")
    // Read max_count from config (0 = unlimited)
    uint64_t max_count = 0;
    try {
      max_count = from_config("tensor_generator.max_count").as<uint64_t>();
    } catch (...) {
      // If not specified, default to 0 (unlimited)
      max_count = 0;
    }

    auto generator = make_operator<ops::TensorGeneratorOp>(
        "tensor_generator",
        make_condition<PeriodicCondition>("periodic", Arg("recess_period") = std::string("100ms")),
        Arg("base_message") = std::string("hello world"),
        Arg("tensor_size") = static_cast<size_t>(1000),
        Arg("gpu_device") = 0);

    // Add CountCondition if max_count is specified and > 0
    if (max_count > 0) {
      generator->add_arg(make_condition<CountCondition>("count", max_count));
      HOLOSCAN_LOG_INFO("Generator will stop after {} tensors", max_count);
    } else {
      HOLOSCAN_LOG_INFO("Generator will run indefinitely");
    }

    // Network transmit operator
    auto tx = make_operator<ops::TensorNetworkTxOp>(
        "tensor_network_tx",
        from_config("tensor_network_tx"));

    // Connect pipeline
    add_flow(generator, tx, {{"tensor_out", "tensor_in"}});

#else
      HOLOSCAN_LOG_ERROR("DPDK manager/backend is disabled");
      exit(1);
#endif
    } else {
      HOLOSCAN_LOG_ERROR("Invalid Advanced Network manager/backend for this demo");
      exit(1);
    }

    

    HOLOSCAN_LOG_INFO("DemoANOTxApp configured: GPU -> Network (GPUDirect)");
  }
};

}  // namespace holoscan::apps

int run_tx_app(const std::string& config_path) {
  try {
    auto app = holoscan::make_application<holoscan::apps::DemoANOTxApp>();
    if (!config_path.empty()) {
      app->config(config_path);
      app->scheduler(app->make_scheduler<holoscan::MultiThreadScheduler>("multithread-scheduler",
                                                           app->from_config("scheduler")));
    }
    app->run();
  } catch (const std::exception& ex) {
    HOLOSCAN_LOG_ERROR("TX app failed: {}", ex.what());
    return 1;
  }
  return 0;
}
