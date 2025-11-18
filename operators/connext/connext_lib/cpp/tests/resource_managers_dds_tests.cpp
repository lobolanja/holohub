#include "connext_lib/resource_managers_dds.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "dds/dds.hpp"

namespace {

using namespace std::chrono_literals;

// TODO: Investigate connext_lib_resource_managers_dds_tests flakiness/failure.

class DdsResourceManagersTester
    : public rti::test::Tester,
      public rti::test::Singleton<DdsResourceManagersTester> {
 public:
  void dds_resource_manager_discovers_receivers() {
    const std::string channel =
        "rm_channel_" + std::to_string(++channel_counter_);
    const std::string buffer_id = "buffer_dds";
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    connext_lib::DdsReceiverResourcesManager receiver(
        receiver_participant, buffer_id, channel);
    connext_lib::DdsSenderResourcesManager sender(
        sender_participant, channel);
    sender.startProcessing(10ms);
    receiver.announce();

    bool observed = false;
    for (int attempt = 0; attempt < 50 && !observed; ++attempt) {
      const auto& destinations = sender.destinations();
      for (const auto& entry : destinations) {
        if (entry.second == buffer_id) {
          observed = true;
          break;
        }
      }
      if (!observed) {
        std::this_thread::sleep_for(40ms);
      }
    }
    sender.stopProcessing();
    RTI_TEST_ASSERT(observed);
  }

  void sender_filters_by_channel() {
    const std::string channel =
        "rm_channel_" + std::to_string(++channel_counter_);
    dds::domain::DomainParticipant receiver_participant(domain_id());
    dds::domain::DomainParticipant sender_participant(domain_id());
    connext_lib::DdsReceiverResourcesManager receiver(
        receiver_participant, "buffer_filtered", channel);
    connext_lib::DdsSenderResourcesManager sender(
        sender_participant, channel + "_other");
    sender.startProcessing(10ms);
    receiver.announce();
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
