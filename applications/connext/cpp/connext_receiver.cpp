#include "connext_common.hpp"

#include <chrono>
#include <thread>

#include <connext_ops/connext_rx.hpp>
#include <holoscan/core/conditions/gxf/count.hpp>
#include <holoscan/core/conditions/gxf/periodic.hpp>

namespace connext_demo {

class ConnextReceiverApp : public holoscan::Application {
 public:
  explicit ConnextReceiverApp(DemoAppConfig config)
      : config_(config), received_payloads_(std::make_shared<std::vector<std::string>>()) {}

  const std::vector<std::string>& received_payloads() const { return *received_payloads_; }

  void compose() override {
    using namespace holoscan;

    std::shared_ptr<holoscan::Condition> rx_condition;
    if (config_.message_count > 0) {
      rx_condition = make_condition<CountCondition>(config_.message_count);
    } else {
      rx_condition = make_condition<PeriodicCondition>(
          "rx_poll_period", std::chrono::milliseconds(config_.message_period_ms));
    }

    auto rx = make_operator<holoscan::ops::ConnextRxOp>(
      "connext_rx",
      rx_condition,
        Arg("enable_dds", config_.use_dds),
        Arg("enable_ano", !config_.use_dds),
        Arg("domain_id", config_.dds_domain_id),
        Arg("topic_name", config_.dds_topic_name),
        Arg("topic_type_name", config_.dds_topic_type),
        Arg("ano_channel", config_.ano_channel),
      Arg("ano_buffer_id", config_.ano_buffer_id),
        Arg("ano_max_payload", config_.ano_max_payload));

    if (config_.use_dds && config_.discovery_wait_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(config_.discovery_wait_ms));
    }

    std::shared_ptr<PayloadSinkOp> sink;
    if (config_.message_count > 0) {
      sink = make_operator<PayloadSinkOp>(
          "payload_sink", make_condition<CountCondition>(config_.message_count));
    } else {
      sink = make_operator<PayloadSinkOp>("payload_sink");
    }
    sink->set_storage(received_payloads_);

    add_flow(rx, sink, {{"output", "input"}});

    if (config_.message_count > 0) {
      HOLOSCAN_LOG_INFO(
          "Connext receiver configured. transport={}, iterations={}, discovery_wait_ms={}",
          config_.use_dds ? "dds" : "ano",
          config_.message_count,
          config_.discovery_wait_ms);
    } else {
      HOLOSCAN_LOG_INFO(
          "Connext receiver configured. transport={}, iterations=continuous, discovery_wait_ms={}",
          config_.use_dds ? "dds" : "ano",
          config_.discovery_wait_ms);
    }
  }

 private:
  DemoAppConfig config_{};
  std::shared_ptr<std::vector<std::string>> received_payloads_;
};

int run_receiver(const DemoAppConfig& config) {
  try {
    ConnextReceiverApp app(config);
    app.run();

    const auto& payloads = app.received_payloads();
    std::cout << "Receiver collected " << payloads.size() << " payload(s)." << std::endl;
  } catch (const std::exception& err) {
    HOLOSCAN_LOG_ERROR("Connext receiver failed: {}", err.what());
    return 1;
  }
  return 0;
}

}  // namespace connext_demo
