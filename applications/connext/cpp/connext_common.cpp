#include "connext_common.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

#include <connext_ops/connext_rx.hpp>
#include <connext_ops/connext_tx.hpp>
#include <holoscan/core/conditions/gxf/count.hpp>
#include <holoscan/core/conditions/gxf/periodic.hpp>

namespace {

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

}  // namespace

namespace connext_demo {
void PayloadSourceOp::setup(holoscan::OperatorSpec& spec) {
  spec.output<nvidia::gxf::Entity>("output");
  spec.param(base_payload_, "base_payload", "Base Payload", "Base payload string.");
}

void PayloadSourceOp::start() {
  holoscan::Operator::start();
  emitted_count_ = 0;
}

void PayloadSourceOp::compute(holoscan::InputContext&, holoscan::OutputContext& output,
                              holoscan::ExecutionContext& context) {
  emitted_count_ += 1;
  const std::string message = base_payload_.get() + "_#" + std::to_string(emitted_count_);

  std::cout << "PayloadSource sending payload: " << message << std::endl;

  auto payload_storage =
      std::make_shared<std::vector<std::uint8_t>>(message.begin(), message.end());

  auto entity = nvidia::gxf::Entity::New(context.context());
  if (!entity) {
    throw std::runtime_error("Failed to allocate entity for payload source.");
  }

  auto payload_tensor = entity.value().add<nvidia::gxf::Tensor>("payload");
  if (!payload_tensor) {
    throw std::runtime_error("Failed to add payload tensor to entity in payload source.");
  }

  nvidia::gxf::Shape payload_shape{static_cast<int32_t>(payload_storage->size())};

  auto wrap_result = payload_tensor.value()->wrapMemory(
      payload_shape,
      nvidia::gxf::PrimitiveType::kUnsigned8,
      sizeof(std::uint8_t),
      nvidia::gxf::ComputeTrivialStrides(payload_shape, sizeof(std::uint8_t)),
      nvidia::gxf::MemoryStorageType::kSystem,
      payload_storage->data(),
      [payload_storage](void*) mutable {
        payload_storage.reset();
        return nvidia::gxf::Success;
      });

  if (!wrap_result) {
    throw std::runtime_error("Failed to wrap payload storage into tensor in payload source.");
  }

  output.emit(entity.value(), "output");
}

void PayloadSinkOp::setup(holoscan::OperatorSpec& spec) {
  spec.input<nvidia::gxf::Entity>("input");
}

void PayloadSinkOp::set_storage(const std::shared_ptr<std::vector<std::string>>& storage) {
  storage_ = storage;
}

void PayloadSinkOp::compute(holoscan::InputContext& input, holoscan::OutputContext&,
                            holoscan::ExecutionContext&) {
  auto entity_expected = input.receive<nvidia::gxf::Entity>("input");
  if (!entity_expected) { return; }

  auto tensor_expected = entity_expected->get<nvidia::gxf::Tensor>("payload");
  if (!tensor_expected) { return; }

  auto tensor = tensor_expected.value();
  auto data_expected = tensor->data<std::uint8_t>();
  if (!data_expected) { return; }

  const auto* data = data_expected.value();
  const auto& shape = tensor->shape();
  if (shape.rank() == 0) { return; }

  const auto length = static_cast<std::size_t>(shape.dimension(0));
  std::string payload(reinterpret_cast<const char*>(data), length);

  std::cout << "PayloadSink received payload: " << payload << std::endl;

  if (storage_) { storage_->push_back(std::move(payload)); }
}

ConnextDemoApp::ConnextDemoApp()
    : received_payloads_(std::make_shared<std::vector<std::string>>()) {}

DemoAppConfig ConnextDemoApp::load_demo_config() {
  DemoAppConfig result{};

  if (from_config("demo").size() == 0) {
    throw std::runtime_error("Missing 'demo' section in configuration");
  }

  std::string mode_value = "tx";
  if (auto mode_arg = from_config("demo.mode"); mode_arg.size() > 0) {
    mode_value = mode_arg.as<std::string>();
  }
  mode_value = ToLower(mode_value);
  if (mode_value == "tx" || mode_value == "sender") {
    result.mode = DemoMode::kTx;
  } else if (mode_value == "rx" || mode_value == "receiver") {
    result.mode = DemoMode::kRx;
  } else {
    throw std::runtime_error("demo.mode must be 'tx'/'sender' or 'rx'/'receiver'");
  }

  if (auto count_arg = from_config("demo.message_count"); count_arg.size() > 0) {
    auto count_value = count_arg.as<int>();
    if (count_value < 0) { throw std::runtime_error("demo.message_count must be >= 0"); }
    result.message_count = count_value;
  }

  if (auto period_arg = from_config("demo.message_period_ms"); period_arg.size() > 0) {
    auto period_value = period_arg.as<int>();
    if (period_value <= 0) { throw std::runtime_error("demo.message_period_ms must be > 0"); }
    result.message_period_ms = period_value;
  }

  if (auto wait_arg = from_config("demo.discovery_wait_ms"); wait_arg.size() > 0) {
    auto wait_value = wait_arg.as<int>();
    if (wait_value < 0) { throw std::runtime_error("demo.discovery_wait_ms must be >= 0"); }
    result.discovery_wait_ms = wait_value;
  }

  if (auto payload_arg = from_config("payload_source.base_payload");
      payload_arg.size() > 0) {
    result.payload = payload_arg.as<std::string>();
  }

  auto apply_transport_info = [&](const std::string& section) -> bool {
    if (from_config(section).size() == 0) { return false; }

    const auto enable_dds_key = section + ".enable_dds";
    if (auto enable_dds_arg = from_config(enable_dds_key); enable_dds_arg.size() > 0) {
      result.use_dds = enable_dds_arg.as<bool>();
    } else if (auto enable_ano_arg = from_config(section + ".enable_ano");
               enable_ano_arg.size() > 0) {
      result.use_dds = !enable_ano_arg.as<bool>();
    }

    if (auto domain_arg = from_config(section + ".domain_id"); domain_arg.size() > 0) {
      auto domain_value = domain_arg.as<int>();
      if (domain_value < 0) { throw std::runtime_error("domain_id must be >= 0"); }
      result.dds_domain_id = domain_value;
    }
    if (auto topic_name_arg = from_config(section + ".topic_name");
        topic_name_arg.size() > 0) {
      result.dds_topic_name = topic_name_arg.as<std::string>();
    }
    if (auto topic_type_arg = from_config(section + ".topic_type_name");
        topic_type_arg.size() > 0) {
      result.dds_topic_type = topic_type_arg.as<std::string>();
    }
    if (auto ano_channel_arg = from_config(section + ".ano_channel");
        ano_channel_arg.size() > 0) {
      result.ano_channel = ano_channel_arg.as<std::string>();
    }
    if (auto ano_buffer_arg = from_config(section + ".ano_buffer_id");
        ano_buffer_arg.size() > 0) {
      result.ano_buffer_id = ano_buffer_arg.as<std::string>();
    }
    if (auto ano_max_payload_arg = from_config(section + ".ano_max_payload");
        ano_max_payload_arg.size() > 0) {
      auto value = ano_max_payload_arg.as<std::uint64_t>();
      if (value == 0) { throw std::runtime_error("ano_max_payload must be > 0"); }
      result.ano_max_payload = value;
    }
    if (auto destination_arg = from_config(section + ".destination_reference");
        destination_arg.size() > 0) {
      result.destination_reference = destination_arg.as<std::string>();
    }

    return true;
  };

  if (result.mode == DemoMode::kTx) {
    apply_transport_info("connext_tx");
  } else {
    const bool has_rx = apply_transport_info("connext_rx");
    if (!has_rx) {
      if (auto tx_enable_dds = from_config("connext_tx.enable_dds");
          tx_enable_dds.size() > 0) {
        result.use_dds = tx_enable_dds.as<bool>();
      }
    }
  }

  if (auto use_dds_override = from_config("demo.use_dds"); use_dds_override.size() > 0) {
    result.use_dds = use_dds_override.as<bool>();
  }

  return result;
}

void ConnextDemoApp::compose() {
  using namespace holoscan;

  demo_config_ = load_demo_config();
  received_payloads_ = std::make_shared<std::vector<std::string>>();

  if (demo_config_.mode == DemoMode::kTx) {
    std::shared_ptr<Condition> source_condition;
    if (demo_config_.message_count > 0) {
      source_condition = make_condition<CountCondition>(demo_config_.message_count);
    } else {
      source_condition = make_condition<PeriodicCondition>(
          "payload_source_period", std::chrono::milliseconds(demo_config_.message_period_ms));
    }

    auto source = make_operator<PayloadSourceOp>(
        "payload_source", from_config("payload_source"), source_condition);

    auto tx = make_operator<holoscan::ops::ConnextTxOp>("connext_tx", from_config("connext_tx"));

    add_flow(source, tx, {{"output", "input"}});

    if (demo_config_.message_count > 0) {
      HOLOSCAN_LOG_INFO(
          "Connext sender configured. transport={}, payload='{}', iterations={}, period_ms={}",
          demo_config_.use_dds ? "dds" : "ano",
          demo_config_.payload,
          demo_config_.message_count,
          demo_config_.message_period_ms);
    } else {
      HOLOSCAN_LOG_INFO(
          "Connext sender configured. transport={}, payload='{}', iterations=continuous, period_ms={}",
          demo_config_.use_dds ? "dds" : "ano",
          demo_config_.payload,
          demo_config_.message_period_ms);
    }
  } else {
    std::shared_ptr<Condition> rx_condition;
    if (demo_config_.message_count > 0) {
      rx_condition = make_condition<CountCondition>(demo_config_.message_count);
    } else {
      rx_condition = make_condition<PeriodicCondition>(
          "rx_poll_period", std::chrono::milliseconds(demo_config_.message_period_ms));
    }

    auto rx = make_operator<holoscan::ops::ConnextRxOp>(
        "connext_rx", from_config("connext_rx"), rx_condition);

    if (demo_config_.use_dds && demo_config_.discovery_wait_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(demo_config_.discovery_wait_ms));
    }

    std::shared_ptr<PayloadSinkOp> sink;
    if (demo_config_.message_count > 0) {
      sink = make_operator<PayloadSinkOp>(
          "payload_sink", make_condition<CountCondition>(demo_config_.message_count));
    } else {
      sink = make_operator<PayloadSinkOp>("payload_sink");
    }
    sink->set_storage(received_payloads_);

    add_flow(rx, sink, {{"output", "input"}});

    if (demo_config_.message_count > 0) {
      HOLOSCAN_LOG_INFO(
          "Connext receiver configured. transport={}, iterations={}, discovery_wait_ms={}",
          demo_config_.use_dds ? "dds" : "ano",
          demo_config_.message_count,
          demo_config_.discovery_wait_ms);
    } else {
      HOLOSCAN_LOG_INFO(
          "Connext receiver configured. transport={}, iterations=continuous, discovery_wait_ms={}",
          demo_config_.use_dds ? "dds" : "ano",
          demo_config_.discovery_wait_ms);
    }
  }
}

}  // namespace connext_demo
