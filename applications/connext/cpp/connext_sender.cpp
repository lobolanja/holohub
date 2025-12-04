#include "connext_common.hpp"

#include <chrono>

#include <connext_ops/connext_tx.hpp>
#include <holoscan/core/conditions/gxf/count.hpp>
#include <holoscan/core/conditions/gxf/periodic.hpp>

namespace connext_demo {

class ConnextSenderApp : public holoscan::Application {
 public:
  explicit ConnextSenderApp(DemoAppConfig config) : config_(config) {}

  void compose() override {
    using namespace holoscan;

    std::shared_ptr<holoscan::Condition> source_condition;
    if (config_.message_count > 0) {
      source_condition = make_condition<CountCondition>(config_.message_count);
    } else {
      source_condition = make_condition<PeriodicCondition>(
          "payload_source_period",
          std::chrono::milliseconds(config_.message_period_ms));
    }

    auto source = make_operator<PayloadSourceOp>(
      "payload_source", source_condition, Arg("base_payload") = config_.payload);

    auto tx = make_operator<holoscan::ops::ConnextTxOp>(
        "connext_tx",
        Arg("enable_dds", config_.use_dds),
        Arg("enable_ano", !config_.use_dds),
        Arg("domain_id", config_.dds_domain_id),
        Arg("topic_name", config_.dds_topic_name),
        Arg("topic_type_name", config_.dds_topic_type),
        Arg("ano_channel", config_.ano_channel),
      Arg("ano_buffer_id", config_.ano_buffer_id),
        Arg("ano_max_payload", config_.ano_max_payload),
        Arg("destination_reference", config_.destination_reference));

    add_flow(source, tx, {{"output", "input"}});

    if (config_.message_count > 0) {
      HOLOSCAN_LOG_INFO(
          "Connext sender configured. transport={}, payload='{}', iterations={}, period_ms={}",
          config_.use_dds ? "dds" : "ano",
          config_.payload,
          config_.message_count,
          config_.message_period_ms);
    } else {
      HOLOSCAN_LOG_INFO(
          "Connext sender configured. transport={}, payload='{}', iterations=continuous, period_ms={}",
          config_.use_dds ? "dds" : "ano",
          config_.payload,
          config_.message_period_ms);
    }
  }

 private:
  DemoAppConfig config_{};
};

int run_sender(const DemoAppConfig& config) {
  try {
    ConnextSenderApp app(config);
    app.run();
  } catch (const std::exception& err) {
    HOLOSCAN_LOG_ERROR("Connext sender failed: {}", err.what());
    return 1;
  }
  return 0;
}

}  // namespace connext_demo
