#include "connext_common.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace connext_demo {

namespace {

bool parse_positive_int(const char* arg, int& value_out) {
  char* end = nullptr;
  long value = std::strtol(arg, &end, 10);
  if (end == arg || value <= 0 || value > std::numeric_limits<int>::max()) { return false; }
  value_out = static_cast<int>(value);
  return true;
}

bool parse_non_negative_int(const char* arg, int& value_out) {
  char* end = nullptr;
  long value = std::strtol(arg, &end, 10);
  if (end == arg || value < 0 || value > std::numeric_limits<int>::max()) { return false; }
  value_out = static_cast<int>(value);
  return true;
}

bool parse_positive_uint64(const char* arg, std::uint64_t& value_out) {
  char* end = nullptr;
  unsigned long long value = std::strtoull(arg, &end, 10);
  if (end == arg || value == 0) { return false; }
  value_out = static_cast<std::uint64_t>(value);
  return true;
}

}  // namespace

void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " [options]\n"
            << "Options:\n"
            << "  --payload <string>           Base payload to transmit (default: hello_holoscan)\n"
            << "  --mode <tx|rx>               Run as transmitter (tx) or receiver (rx) (default: tx)\n"
            << "  --message-count <int>        Number of payloads to send (0 for continuous, default: 0)\n"
            << "  --message-period-ms <int>    Delay between payloads when continuous (default: 1000)\n"
            << "  --use-dds                    Enable DDS transport (default: ANO)\n"
            << "  --use-ano                    Enable ANO transport (default if --use-dds not set)\n"
            << "  --dds-domain-id <int>        DDS domain identifier (default: 2)\n"
            << "  --dds-topic-name <string>    DDS topic name (default: ConnextDemoTopic)\n"
            << "  --dds-topic-type <string>    DDS topic type name (optional)\n"
            << "  --ano-channel <string>       ANO channel name (default: connext_demo_channel)\n"
            << "  --ano-buffer-id <string>     ANO buffer identifier (default: connext_demo_buffer)\n"
            << "  --ano-max-payload <bytes>    Maximum ANO payload size (default: 4096)\n"
            << "  --destination <string>       Optional destination reference hint\n"
            << "  --discovery-wait-ms <int>    Wait before activating sink in DDS mode (default: 3000)\n"
            << "  --help                       Show this message and exit\n";
}

bool parse_arguments(int argc, char** argv, DemoAppConfig& config, bool& show_usage) {
  bool dds_selected = false;
  bool ano_selected = false;

  static struct option long_options[] = {
      {"mode", required_argument, nullptr, 'o'},
      {"payload", required_argument, nullptr, 'p'},
      {"message-count", required_argument, nullptr, 'm'},
      {"message-period-ms", required_argument, nullptr, 'P'},
      {"use-dds", no_argument, nullptr, 'D'},
      {"use-ano", no_argument, nullptr, 'A'},
      {"dds-domain-id", required_argument, nullptr, 'i'},
      {"dds-topic-name", required_argument, nullptr, 't'},
      {"dds-topic-type", required_argument, nullptr, 'T'},
      {"ano-channel", required_argument, nullptr, 'a'},
      {"ano-buffer-id", required_argument, nullptr, 'B'},
      {"ano-max-payload", required_argument, nullptr, 'M'},
      {"destination", required_argument, nullptr, 'r'},
      {"discovery-wait-ms", required_argument, nullptr, 'w'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  opterr = 0;
  optind = 1;

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "", long_options, &option_index);
    if (c == -1) { break; }

    switch (c) {
      case 'o': {
        if (!optarg) {
          std::cerr << "--mode requires an argument (tx or rx)." << std::endl;
          return false;
        }
        std::string mode_str(optarg);
        std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (mode_str == "tx") {
          config.mode = DemoMode::kTx;
        } else if (mode_str == "rx") {
          config.mode = DemoMode::kRx;
        } else {
          std::cerr << "Invalid --mode value: " << optarg << std::endl;
          return false;
        }
        break;
      }
      case 'p':
        config.payload = optarg ? optarg : "";
        break;
      case 'm': {
        int value = 0;
        if (!parse_non_negative_int(optarg, value)) {
          std::cerr << "Invalid --message-count value: " << optarg << std::endl;
          return false;
        }
        config.message_count = value;
        break;
      }
      case 'P': {
        int value = 0;
        if (!parse_positive_int(optarg, value)) {
          std::cerr << "Invalid --message-period-ms value: " << optarg << std::endl;
          return false;
        }
        config.message_period_ms = value;
        break;
      }
      case 'D':
        dds_selected = true;
        break;
      case 'A':
        ano_selected = true;
        break;
      case 'i': {
        int value = 0;
        if (!parse_non_negative_int(optarg, value)) {
          std::cerr << "Invalid --dds-domain-id value: " << optarg << std::endl;
          return false;
        }
        config.dds_domain_id = value;
        break;
      }
      case 't':
        config.dds_topic_name = optarg ? optarg : "";
        break;
      case 'T':
        config.dds_topic_type = optarg ? optarg : "";
        break;
      case 'a':
        config.ano_channel = optarg ? optarg : "";
        break;
      case 'B':
        config.ano_buffer_id = optarg ? optarg : "";
        break;
      case 'M': {
        std::uint64_t value = 0;
        if (!parse_positive_uint64(optarg, value)) {
          std::cerr << "Invalid --ano-max-payload value: " << optarg << std::endl;
          return false;
        }
        config.ano_max_payload = value;
        break;
      }
      case 'r':
        config.destination_reference = optarg ? optarg : "";
        break;
      case 'w': {
        int value = 0;
        if (!parse_non_negative_int(optarg, value)) {
          std::cerr << "Invalid --discovery-wait-ms value: " << optarg << std::endl;
          return false;
        }
        config.discovery_wait_ms = value;
        break;
      }
      case 'h':
        show_usage = true;
        return false;
      default:
        std::cerr << "Unknown option encountered." << std::endl;
        return false;
    }
  }

  if (optind < argc) {
    std::cerr << "Unexpected positional argument: " << argv[optind] << std::endl;
    return false;
  }

  if (dds_selected && ano_selected) {
    std::cerr << "DDS and ANO transports are mutually exclusive. Choose only one." << std::endl;
    return false;
  }

  if (dds_selected) {
    config.use_dds = true;
  } else if (ano_selected) {
    config.use_dds = false;
  }

  return true;
}

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

}  // namespace connext_demo
