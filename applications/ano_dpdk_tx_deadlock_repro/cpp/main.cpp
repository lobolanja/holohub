/*
 * SPDX-FileCopyrightText: Copyright (c) 2026, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Reproducer for the DpdkMgr TX mempool/completion-polling deadlock documented in
 * ANO_DPDK_TX_DEADLOCK_BUG.md.
 *
 * Strategy:
 *   - Configure the ANO DPDK backend with num_bufs/batch_size such that
 *     I_max = floor(num_bufs / batch_size) - 2 is very small (1 in the shipped YAML).
 *   - Submit bursts as fast as is_tx_burst_available() allows, with a small inter-burst
 *     sleep to ensure the tx_core_worker's input ring drains between calls. This kills
 *     the steady-state throughput that would otherwise let the mlx5 PMD opportunistically
 *     reclaim TX completions inside rte_eth_tx_burst().
 *   - Once I_max in-flight bursts accumulate, is_tx_burst_available() returns false and
 *     stays false: the worker has no work to dequeue, so it never calls tx_burst, so
 *     completions never get reclaimed, so the mempool never refills.
 *   - A watchdog counts consecutive gate-false readings and announces the verdict.
 */

#include "advanced_network/common.h"
#include "holoscan/holoscan.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace holoscan::advanced_network;

namespace holoscan::ops {

class DeadlockReproTxOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(DeadlockReproTxOp)

  DeadlockReproTxOp() = default;

  void setup(OperatorSpec& spec) override {
    spec.param<std::string>(interface_name_, "interface_name", "TX interface name", "", "");
    spec.param<uint32_t>(batch_size_, "batch_size", "Packets per burst", "", 1024u);
    spec.param<uint16_t>(payload_size_, "payload_size", "UDP payload bytes", "", 1000);
    spec.param<uint16_t>(header_size_, "header_size", "L2+L3+L4 header bytes", "", 42);
    spec.param<uint32_t>(tx_pacing_us_, "tx_pacing_us", "Sleep between bursts (us)", "", 200u);
    spec.param<uint64_t>(deadlock_threshold_, "deadlock_threshold",
                         "Consecutive gate-false count that declares deadlock", "", 200000ull);
    spec.param<uint64_t>(target_packets_, "target_packets", "Success target", "", 200000ull);
    spec.param<std::string>(eth_dst_addr_, "eth_dst_addr", "Ethernet dst (xx:xx:..)", "", "");
    spec.param<std::string>(ip_src_addr_, "ip_src_addr", "IPv4 src", "", "");
    spec.param<std::string>(ip_dst_addr_, "ip_dst_addr", "IPv4 dst", "", "");
    spec.param<uint16_t>(udp_src_port_, "udp_src_port", "UDP src port", "", 4096);
    spec.param<uint16_t>(udp_dst_port_, "udp_dst_port", "UDP dst port", "", 4096);
  }

  void initialize() override {
    holoscan::Operator::initialize();

    port_id_ = get_port_id(interface_name_.get());
    if (port_id_ == -1) {
      HOLOSCAN_LOG_ERROR("Invalid TX interface name in config: {}", interface_name_.get());
      std::exit(2);
    }

    format_eth_addr(eth_dst_, eth_dst_addr_.get());
    inet_pton(AF_INET, ip_src_addr_.get().c_str(), &ip_src_);
    inet_pton(AF_INET, ip_dst_addr_.get().c_str(), &ip_dst_);
    ip_src_ = ntohl(ip_src_);
    ip_dst_ = ntohl(ip_dst_);

    // Reusable CPU payload buffer of constant bytes.
    payload_buf_.assign(payload_size_.get(), 0xA5);

    HOLOSCAN_LOG_INFO(
        "Reproducer TX op initialized: batch={} pacing_us={} target={} deadlock_threshold={}",
        batch_size_.get(), tx_pacing_us_.get(), target_packets_.get(),
        deadlock_threshold_.get());
  }

  void compute(InputContext&, OutputContext&, ExecutionContext& context) override {
    // Pace the producer so the tx_core_worker's input ring drains between submissions.
    if (tx_pacing_us_.get() > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(tx_pacing_us_.get()));
    }

    // Build a candidate burst (we need it for is_tx_burst_available's per-segment check).
    auto* msg = create_tx_burst_params();
    set_header(msg, static_cast<uint16_t>(port_id_), queue_id_,
               static_cast<int64_t>(batch_size_.get()), /*segs=*/1);

    if (!is_tx_burst_available(msg)) {
      ++gate_false_consecutive_;
      free_tx_metadata(msg);

      // Periodic progress log so the test output is human-readable.
      if ((gate_false_consecutive_ % 10000ull) == 0ull) {
        HOLOSCAN_LOG_WARN(
            "is_tx_burst_available=false consecutive={} | sent_bursts={} sent_packets={}",
            gate_false_consecutive_, sent_bursts_, sent_packets_);
      }

      if (gate_false_consecutive_ >= deadlock_threshold_.get() && !verdict_emitted_) {
        verdict_emitted_ = true;
        HOLOSCAN_LOG_CRITICAL(
            "*** DEADLOCK REPRODUCED *** is_tx_burst_available() has returned false {} times "
            "in a row. sent_bursts={} sent_packets={} (target={}). The DpdkMgr TX "
            "mempool/completion-polling deadlock is present.",
            gate_false_consecutive_, sent_bursts_, sent_packets_, target_packets_.get());
        // Exit with code 0 so an automated harness can treat reproduction as success.
        std::quick_exit(0);
      }
      return;
    }

    gate_false_consecutive_ = 0;

    if (get_tx_packet_burst(msg) != Status::SUCCESS) {
      HOLOSCAN_LOG_ERROR("get_tx_packet_burst failed despite gate=true");
      free_tx_metadata(msg);
      return;
    }

    const auto num_pkts = static_cast<int>(get_num_packets(msg));
    const auto ip_len = payload_size_.get() + header_size_.get() - 14;       // - Eth
    const auto udp_len = payload_size_.get() + header_size_.get() - (14 + 20);  // - Eth - IP

    for (int i = 0; i < num_pkts; ++i) {
      if (set_eth_header(msg, i, eth_dst_) != Status::SUCCESS ||
          set_ipv4_header(msg, i, ip_len, 17, ip_src_, ip_dst_) != Status::SUCCESS ||
          set_udp_header(msg, i, udp_len, udp_src_port_.get(), udp_dst_port_.get()) !=
              Status::SUCCESS ||
          set_udp_payload(msg, i, payload_buf_.data(),
                          static_cast<int>(payload_buf_.size())) != Status::SUCCESS ||
          set_packet_lengths(msg, i, {payload_size_.get() + header_size_.get()}) !=
              Status::SUCCESS) {
        HOLOSCAN_LOG_ERROR("Header/payload population failed for packet {}", i);
        free_all_packets_and_burst_tx(msg);
        return;
      }
    }

    if (send_tx_burst(msg) == Status::SUCCESS) {
      ++sent_bursts_;
      sent_packets_ += num_pkts;
    }

    if (sent_packets_ >= target_packets_.get() && !verdict_emitted_) {
      verdict_emitted_ = true;
      HOLOSCAN_LOG_INFO(
          "NO DEADLOCK OBSERVED: sent_packets={} bursts={} (target={}). The TX path made "
          "forward progress for the full duration — either the fix is in place or the "
          "workload sized around the structural bug.",
          sent_packets_, sent_bursts_, target_packets_.get());
      // Exit code 1 distinguishes the no-repro path from reproduction.
      std::quick_exit(1);
    }
  }

 private:
  Parameter<std::string> interface_name_;
  Parameter<uint32_t> batch_size_;
  Parameter<uint16_t> payload_size_;
  Parameter<uint16_t> header_size_;
  Parameter<uint32_t> tx_pacing_us_;
  Parameter<uint64_t> deadlock_threshold_;
  Parameter<uint64_t> target_packets_;
  Parameter<std::string> eth_dst_addr_;
  Parameter<std::string> ip_src_addr_;
  Parameter<std::string> ip_dst_addr_;
  Parameter<uint16_t> udp_src_port_;
  Parameter<uint16_t> udp_dst_port_;

  int port_id_{-1};
  static constexpr uint16_t queue_id_ = 0;
  char eth_dst_[6]{};
  uint32_t ip_src_{0};
  uint32_t ip_dst_{0};
  std::vector<uint8_t> payload_buf_;

  uint64_t sent_bursts_{0};
  uint64_t sent_packets_{0};
  uint64_t gate_false_consecutive_{0};
  bool verdict_emitted_{false};
};

}  // namespace holoscan::ops

class ReproApp : public holoscan::Application {
 public:
  void compose() override {
    using namespace holoscan;

    auto adv_net_config = from_config("advanced_network").as<NetworkConfig>();
    if (adv_net_init(adv_net_config) != Status::SUCCESS) {
      HOLOSCAN_LOG_ERROR("adv_net_init failed");
      std::exit(2);
    }

    auto tx = make_operator<ops::DeadlockReproTxOp>(
        "repro_tx",
        from_config("repro_tx"),
        make_condition<BooleanCondition>("is_alive", true));
    add_operator(tx);
  }
};

int main(int argc, char** argv) {
  auto app = holoscan::make_application<ReproApp>();
  const std::string cfg = (argc > 1) ? argv[1] : "ano_dpdk_tx_deadlock_repro.yaml";
  app->config(cfg);
  app->run();

  // Reached only if the scheduler hit max_duration_ms without either verdict firing.
  HOLOSCAN_LOG_WARN(
      "Run ended without a verdict being emitted (scheduler hit max_duration_ms). "
      "Check the log for the last is_tx_burst_available consecutive count to interpret.");
  shutdown();
  return 2;
}
