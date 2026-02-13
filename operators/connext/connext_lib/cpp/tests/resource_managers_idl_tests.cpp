#include "connext_lib/resource/resource_managers_idl.hpp"
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

// Helper to setup sender/receiver and synchronize discovery via IDL
void setup_idl_sender_receiver_and_wait(
  const std::string& channel,
  const std::string& expected_destination,
  connext_lib::DdsIdlReceiverResourcesManager& receiver,
  connext_lib::DdsIdlSenderResourcesManager& sender) {
  
  // Announce receiver (creates writer and sends REGISTER sample)
  receiver.announce();
  
  // Give time for discovery on dedicated domain
  std::this_thread::sleep_for(100ms);
  
  // Poll for discovery
  bool discovered = false;
  for (int attempt = 0; attempt < 50 && !discovered; ++attempt) {
    sender.pollOnce();
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
  
  RTI_TEST_ASSERT(discovered);
}

class IdlResourceManagersTester
    : public rti::test::Tester,
      public rti::test::Singleton<IdlResourceManagersTester> {
 public:
  void idl_resource_manager_discovers_receivers() {
    // Test that sender discovers receiver after announcement using IDL-based discovery
    const std::string channel =
        "idl_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "buffer_idl";
    
    // Create participants on regular domain (discovery uses domain 101)
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    
    connext_lib::AnoNetworkConfig gpu_cfg("192.168.1.10", 5000);
    gpu_cfg.set_fast_mac_address("00:11:22:33:44:55");
    connext_lib::AnoConfig ano_cfg(channel, buffer_id, 1024, true, gpu_cfg);
    
    connext_lib::DdsIdlSenderResourcesManager sender(sender_participant, channel);
    connext_lib::DdsIdlReceiverResourcesManager receiver(receiver_participant, ano_cfg);
    
    // Build expected destination string from the GPU config (ip:port:mac)
    const std::string expected = connext_lib::DestinationInfo{
        gpu_cfg.fast_ip(),
        gpu_cfg.fast_mac_address(),
        static_cast<uint16_t>(gpu_cfg.fast_port())
    }.toString();
    
    setup_idl_sender_receiver_and_wait(channel, expected, receiver, sender);
  }

  void sender_filters_by_channel_idl() {
    // Test that sender does not discover receiver if channel does not match
    const std::string channel =
        "idl_channel_" + std::to_string(++channel_counter_);
    
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    
    connext_lib::AnoNetworkConfig gpu_cfg("192.168.1.20", 6000);
    gpu_cfg.set_fast_mac_address("00:AA:BB:CC:DD:EE");
    connext_lib::AnoConfig ano_filtered(channel, "buffer_filtered", 1024, true, gpu_cfg);
    
    connext_lib::DdsIdlReceiverResourcesManager receiver(receiver_participant, ano_filtered);
    connext_lib::DdsIdlSenderResourcesManager sender(sender_participant, channel + "_other");
    
    receiver.announce();
    std::this_thread::sleep_for(200ms);
    
    // Poll multiple times to ensure no discovery
    for (int i = 0; i < 5; ++i) {
      sender.pollOnce();
      std::this_thread::sleep_for(20ms);
    }
    
    RTI_TEST_ASSERT(sender.destinations().empty());
  }

  void sender_does_not_discover_unannounced_receiver_idl() {
    // Negative test: sender should not discover receiver if not announced
    const std::string channel =
        "idl_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "buffer_unannounced";
    
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    
    connext_lib::AnoNetworkConfig gpu_cfg_un("192.168.1.30", 7000);
    gpu_cfg_un.set_fast_mac_address("00:FF:EE:DD:CC:BB");
    connext_lib::AnoConfig ano_unannounced(channel, buffer_id, 1024, true, gpu_cfg_un);
    
    connext_lib::DdsIdlReceiverResourcesManager receiver(receiver_participant, ano_unannounced);
    connext_lib::DdsIdlSenderResourcesManager sender(sender_participant, channel);
    
    // Do NOT announce receiver
    std::this_thread::sleep_for(200ms);
    
    // Poll multiple times
    for (int i = 0; i < 5; ++i) {
      sender.pollOnce();
      std::this_thread::sleep_for(20ms);
    }
    
    RTI_TEST_ASSERT(sender.destinations().empty());
  }

  void receiver_unregister_on_destruction() {
    // Test that receiver sends UNREGISTER on destruction
    const std::string channel =
        "idl_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "buffer_unreg";
    
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    
    connext_lib::AnoNetworkConfig gpu_cfg("192.168.1.40", 8000);
    gpu_cfg.set_fast_mac_address("11:22:33:44:55:66");
    connext_lib::AnoConfig ano_cfg(channel, buffer_id, 1024, true, gpu_cfg);
    
    connext_lib::DdsIdlSenderResourcesManager sender(sender_participant, channel);
    
    std::string expected_dest;
    {
      connext_lib::DdsIdlReceiverResourcesManager receiver(receiver_participant, ano_cfg);
      
      expected_dest = connext_lib::DestinationInfo{
          gpu_cfg.fast_ip(),
          gpu_cfg.fast_mac_address(),
          static_cast<uint16_t>(gpu_cfg.fast_port())
      }.toString();
      
      receiver.announce();
      std::this_thread::sleep_for(100ms);
      
      // Verify discovery
      bool discovered = false;
      for (int attempt = 0; attempt < 20 && !discovered; ++attempt) {
        sender.pollOnce();
        const auto& destinations = sender.destinations();
        for (const auto& entry : destinations) {
          if (entry.second == expected_dest) {
            discovered = true;
            break;
          }
        }
        if (!discovered) {
          std::this_thread::sleep_for(50ms);
        }
      }
      RTI_TEST_ASSERT(discovered);
      
      // Receiver destructor will send UNREGISTER
    }
    
    // Wait for NOT_ALIVE instance state to propagate
    std::this_thread::sleep_for(200ms);
    
    // Poll and verify unregistration
    for (int i = 0; i < 10; ++i) {
      sender.pollOnce();
      std::this_thread::sleep_for(30ms);
    }
    
    // Destination should be removed
    const auto& destinations = sender.destinations();
    bool found = false;
    for (const auto& entry : destinations) {
      if (entry.second == expected_dest) {
        found = true;
        break;
      }
    }
    RTI_TEST_ASSERT(!found);
  }

  void receiver_liveliness_reannouncement() {
    // Test that receiver can assert liveliness via re-announcement
    const std::string channel =
        "idl_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "buffer_liveliness";
    
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    
    connext_lib::AnoNetworkConfig gpu_cfg("192.168.1.50", 9000);
    gpu_cfg.set_fast_mac_address("AA:BB:CC:DD:EE:FF");
    connext_lib::AnoConfig ano_cfg(channel, buffer_id, 1024, true, gpu_cfg);
    
    connext_lib::DdsIdlSenderResourcesManager sender(sender_participant, channel);
    connext_lib::DdsIdlReceiverResourcesManager receiver(receiver_participant, ano_cfg);
    
    const std::string expected = connext_lib::DestinationInfo{
        gpu_cfg.fast_ip(),
        gpu_cfg.fast_mac_address(),
        static_cast<uint16_t>(gpu_cfg.fast_port())
    }.toString();
    
    // First announcement
    setup_idl_sender_receiver_and_wait(channel, expected, receiver, sender);
    
    // Re-announce (assert liveliness)
    receiver.announce();
    std::this_thread::sleep_for(50ms);
    
    // Verify still discovered
    sender.pollOnce();
    const auto& destinations = sender.destinations();
    bool found = false;
    for (const auto& entry : destinations) {
      if (entry.second == expected) {
        found = true;
        break;
      }
    }
    RTI_TEST_ASSERT(found);
  }

 private:
  IdlResourceManagersTester()
      : rti::test::Tester("connext_lib_resource_managers_idl_tests") {
    RTI_TEST_FUNCTION_ADD(IdlResourceManagersTester,
                          idl_resource_manager_discovers_receivers);
    RTI_TEST_FUNCTION_ADD(IdlResourceManagersTester,
                          sender_filters_by_channel_idl);
    RTI_TEST_FUNCTION_ADD(IdlResourceManagersTester,
                          sender_does_not_discover_unannounced_receiver_idl);
    RTI_TEST_FUNCTION_ADD(IdlResourceManagersTester,
                          receiver_unregister_on_destruction);
    RTI_TEST_FUNCTION_ADD(IdlResourceManagersTester,
                          receiver_liveliness_reannouncement);
  }

  std::atomic<int> channel_counter_{0};

  friend class rti::test::Singleton<IdlResourceManagersTester>;
};

class IdlResourceManagersTestContainer
    : public rti::test::TesterContainer,
      public rti::test::Singleton<IdlResourceManagersTestContainer> {
 private:
  IdlResourceManagersTestContainer()
      : rti::test::TesterContainer("connext_lib_resource_managers_idl") {
    add_tester<IdlResourceManagersTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<IdlResourceManagersTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return IdlResourceManagersTestContainer::get_instance().run_tests(argc, argv);
}
