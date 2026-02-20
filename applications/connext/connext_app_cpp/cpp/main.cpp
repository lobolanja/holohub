#include "connext_common.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  namespace fs = std::filesystem;

  fs::path binary_path = fs::path(argv[0]);
  try {
    binary_path = fs::canonical(binary_path);
  } catch (const std::exception&) {
    // Leave binary_path as provided when canonicalization fails (e.g., permission issues).
  }

  const fs::path binary_dir = binary_path.parent_path();

  fs::path default_config_path = binary_dir / "connext_sender.yaml";
  const fs::path installed_examples_path =
      binary_dir / "examples" / "connext" / "connext_sender.yaml";
  if (!fs::exists(default_config_path) && fs::exists(installed_examples_path)) {
    default_config_path = installed_examples_path;
  }

  fs::path config_path = (argc >= 2) ? fs::path(argv[1]) : default_config_path;
  if (!fs::exists(config_path)) {
    std::cerr << "Configuration file not found: " << config_path << std::endl;
    std::cerr << "Provide a YAML file path as the first argument." << std::endl;
    return 1;
  }

  try {
    connext_demo::ConnextDemoApp app;
    app.config(config_path);
    app.run();

    if (app.mode() == connext_demo::DemoMode::kRx) {
      const auto& payloads = app.received_payloads();
      std::cout << "Receiver collected " << payloads.size() << " payload(s)." << std::endl;
    }
  } catch (const std::exception& err) {
    HOLOSCAN_LOG_ERROR("Connext demo failed: {}", err.what());
    return 1;
  }

  return 0;
}
