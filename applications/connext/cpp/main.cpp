#include "connext_common.hpp"

int main(int argc, char** argv) {
  using namespace connext_demo;

  DemoAppConfig config;
  bool show_usage = false;
  if (!parse_arguments(argc, argv, config, show_usage)) {
    if (show_usage) {
      print_usage(argv[0]);
      return 0;
    }
    print_usage(argv[0]);
    return 1;
  }

  switch (config.mode) {
    case DemoMode::kTx:
      return run_sender(config);
    case DemoMode::kRx:
      return run_receiver(config);
    default:
      print_usage(argv[0]);
      return 1;
  }
}
