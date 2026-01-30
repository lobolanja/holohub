#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/transport/sender_info.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "dds/dds.hpp"

namespace {

using namespace std::chrono_literals;

// Helper to setup sender/receiver and synchronize DDS discovery
void setup_sender_receiver_and_wait(
  const std::string& channel,
  const std::string& expected_destination,
  connext_lib::DdsReceiverResourcesManager& receiver,
  connext_lib::DdsSenderResourcesManager& sender) {
  sender.startProcessing(10ms);
  receiver.announce();
  // If DDSCTestContext_waitForReaders is available, use it here for robust discovery
  // Otherwise, fallback to polling as before
  bool discovered = false;
  for (int attempt = 0; attempt < 50 && !discovered; ++attempt) {
    const auto& destinations = sender.destinations();
    for (const auto& entry : destinations) {
      if (entry.second == expected_destination) {
        discovered = true;
        break;
      }
    }
    if (!discovered) {
      std::this_thread::sleep_for(40ms);
    }
  }
  sender.stopProcessing();
  RTI_TEST_ASSERT(discovered);
}

class DdsResourceManagersTester
    : public rti::test::Tester,
      public rti::test::Singleton<DdsResourceManagersTester> {
 public:
  void dds_resource_manager_discovers_receivers() {
    // Test that sender discovers receiver after announcement
    const std::string channel =
        "rm_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "buffer_dds";
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    // Enable GPUDirect properties on the announced receiver so the sender
    // registers a canonical serialized DestinationInfo.
    connext_lib::AnoNetworkConfig gpu_cfg(true, "eth0", 0);
    connext_lib::AnoConfig ano_cfg(channel, buffer_id, 1024, true, gpu_cfg);
    connext_lib::DdsReceiverResourcesManager receiver(
      receiver_participant, ano_cfg);
    connext_lib::DdsSenderResourcesManager sender(
        sender_participant, channel);
    // Build expected destination string from the GPU config (ip:port:mac)
    const std::string expected = connext_lib::DestinationInfo{
        gpu_cfg.fast_ip(),
        gpu_cfg.fast_mac_address(),
        static_cast<uint16_t>(gpu_cfg.fast_port())
    }.toString();
    setup_sender_receiver_and_wait(channel, expected, receiver, sender);
  }

  void sender_filters_by_channel() {
    // Test that sender does not discover receiver if channel does not match
    const std::string channel =
        "rm_channel_" + std::to_string(++channel_counter_);
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    connext_lib::AnoNetworkConfig gpu_cfg(true, "eth0", 0);
    connext_lib::AnoConfig ano_filtered(channel, "buffer_filtered", 1024, true, gpu_cfg);
    connext_lib::DdsReceiverResourcesManager receiver(
      receiver_participant, ano_filtered);
    connext_lib::DdsSenderResourcesManager sender(
        sender_participant, channel + "_other");
    sender.startProcessing(10ms);
    receiver.announce();
    std::this_thread::sleep_for(200ms);
    sender.stopProcessing();
    RTI_TEST_ASSERT(sender.destinations().empty());
  }

  void sender_does_not_discover_unannounced_receiver() {
    // Negative test: sender should not discover receiver if not announced
    const std::string channel =
        "rm_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "buffer_unannounced";
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    connext_lib::AnoNetworkConfig gpu_cfg_un(true, "eth0", 0);
    connext_lib::AnoConfig ano_unannounced(channel, buffer_id, 1024, true, gpu_cfg_un);
    connext_lib::DdsReceiverResourcesManager receiver(
      receiver_participant, ano_unannounced);
    connext_lib::DdsSenderResourcesManager sender(
        sender_participant, channel);
    sender.startProcessing(10ms);
    // Do NOT announce receiver
    std::this_thread::sleep_for(200ms);
    sender.stopProcessing();
    RTI_TEST_ASSERT(sender.destinations().empty());
  }

 private:
  DdsResourceManagersTester()
      : rti::test::Tester("connext_lib_resource_managers_dds_tests") {
    RTI_TEST_FUNCTION_ADD(DdsResourceManagersTester,
                          dds_resource_manager_discovers_receivers);
    RTI_TEST_FUNCTION_ADD(DdsResourceManagersTester,
                          sender_filters_by_channel);
    RTI_TEST_FUNCTION_ADD(DdsResourceManagersTester,
                          sender_does_not_discover_unannounced_receiver);
  }

  std::atomic<int> channel_counter_{0};

  friend class rti::test::Singleton<DdsResourceManagersTester>;
};

class DdsResourceManagersTestContainer
    : public rti::test::TesterContainer,
      public rti::test::Singleton<DdsResourceManagersTestContainer> {
 private:
  DdsResourceManagersTestContainer()
      : rti::test::TesterContainer("connext_lib_resource_managers_dds") {
    add_tester<DdsResourceManagersTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<DdsResourceManagersTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return DdsResourceManagersTestContainer::get_instance().run_tests(argc, argv);
}
