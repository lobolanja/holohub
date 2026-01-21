/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <holoscan/holoscan.hpp>
#include "advanced_network/common.h"
#include "../include/tensor_network_rx_op.h"
#include "../include/tensor_printer_op.h"

namespace holoscan::apps {

/**
 * @brief Demo application that receives tensors from the network and prints them
 * 
 * This application creates a simple pipeline:
 * TensorNetworkRxOp -> TensorPrinterOp
 * 
 * It receives tensors transmitted over the network using Advanced Network library
 * in GPU-only mode and prints their contents.
 */
class DemoANORxApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto adv_net_config = from_config("advanced_network").as<advanced_network::NetworkConfig>();
    if (advanced_network::adv_net_init(adv_net_config) != advanced_network::Status::SUCCESS) {
      HOLOSCAN_LOG_ERROR("Failed to configure the Advanced Network manager");
      exit(1);
    }
    HOLOSCAN_LOG_INFO("Configured the Advanced Network manager");

    const auto [rx_en, tx_en] = advanced_network::get_rx_tx_configs_enabled(config());
    const auto mgr_type = advanced_network::get_manager_type(config());

    HOLOSCAN_LOG_INFO("Using Advanced Network manager {}",
                      advanced_network::manager_type_to_string(mgr_type));
     // DPDK is the default manager backend
    if (mgr_type == advanced_network::ManagerType::DPDK) {
#if ANO_MGR_DPDK
    // Network receive operator
    // Read max_count from config (0 = unlimited)
    uint64_t max_count = 0;
    try {
      max_count = from_config("tensor_network_rx.max_count").as<uint64_t>();
    } catch (...) {
      // If not specified, default to 0 (unlimited)
      max_count = 0;
    }

    auto rx = make_operator<ops::TensorNetworkRxOp>(
        "tensor_network_rx",
        from_config("tensor_network_rx"));

    // Add CountCondition if max_count is specified and > 0
    if (max_count > 0) {
      rx->add_arg(make_condition<CountCondition>("count", max_count));
      HOLOSCAN_LOG_INFO("Network RX will stop after {} packets", max_count);
    } else {
      HOLOSCAN_LOG_INFO("Network RX will run indefinitely");
    }
    
    // Printer operator (prints received tensor contents)
    auto printer = make_operator<ops::TensorPrinterOp>(
        "tensor_printer",
        Arg("max_print_size") = static_cast<size_t>(256));

    // Connect pipeline
    add_flow(rx, printer, {{"tensor_out", "tensor_in"}});
#else
      HOLOSCAN_LOG_ERROR("DPDK manager/backend is disabled");
      exit(1);
#endif
    } else {
      HOLOSCAN_LOG_ERROR("Invalid Advanced Network manager/backend for this demo");
      exit(1);
    }

    

    HOLOSCAN_LOG_INFO("DemoANORxApp configured: Network (GPUDirect) -> Printer");
  }
};

}  // namespace holoscan::apps

int run_rx_app(const std::string& config_path) {
  try {
    auto app = holoscan::make_application<holoscan::apps::DemoANORxApp>();
    if (!config_path.empty()) {
      app->config(config_path);
      app->scheduler(app->make_scheduler<holoscan::MultiThreadScheduler>("multithread-scheduler",
                                                           app->from_config("scheduler")));
    }
    app->run();
  } catch (const std::exception& ex) {
    HOLOSCAN_LOG_ERROR("RX app failed: {}", ex.what());
    return 1;
  }
  return 0;
}
