/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <string>
#include <holoscan/holoscan.hpp>

// Forward declarations
int run_tx_app(const std::string& config_path);
int run_rx_app(const std::string& config_path);

void print_usage(const char* prog_name) {
  std::cout << "Usage: " << prog_name << " <mode> [config_file]\n"
            << "  mode: tx or rx\n"
            << "  config_file: YAML configuration file (optional)\n"
            << "\n"
            << "Examples:\n"
            << "  " << prog_name << " tx demo_ano_tx.yaml\n"
            << "  " << prog_name << " rx demo_ano_rx.yaml\n";
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  std::string mode(argv[1]);
  std::string config_path = (argc >= 3) ? argv[2] : "";

  if (mode == "tx") {
    HOLOSCAN_LOG_INFO("Starting Tensor Network TX Application");
    return run_tx_app(config_path);
  } else if (mode == "rx") {
    HOLOSCAN_LOG_INFO("Starting Tensor Network RX Application");
    return run_rx_app(config_path);
  } else {
    std::cerr << "Error: Invalid mode '" << mode << "'. Must be 'tx' or 'rx'\n";
    print_usage(argv[0]);
    return 1;
  }
}
