#include "connext_lib/config.hpp"

#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {

class ConfigTester : public rti::test::Tester,
                     public rti::test::Singleton<ConfigTester> {
 public:
  // Sanity-check the default configuration and verify setters mutate state.
  void dds_config_defaults() {
    connext_lib::DdsConfig cfg;
    RTI_TEST_ASSERT(!cfg.enabled());
    RTI_TEST_ASSERT_EQUALS_INT(0, cfg.domain_id());
    RTI_TEST_ASSERT(cfg.topic_name() == "system_setup");
    RTI_TEST_ASSERT(cfg.topic_type_name().empty());
    cfg.set_topic_name("custom");
    cfg.set_topic_type_name("type");
    cfg.set_domain_id(5);
    cfg.set_enabled(true);
    RTI_TEST_ASSERT(cfg.enabled());
    RTI_TEST_ASSERT_EQUALS_INT(5, cfg.domain_id());
    RTI_TEST_ASSERT(cfg.topic_name() == "custom");
    RTI_TEST_ASSERT(cfg.topic_type_name() == "type");
  }

  void dds_config_custom_ctor() {
    connext_lib::DdsConfig cfg{/*enabled=*/true,
                               /*domain_id=*/42,
                               /*topic_name=*/"custom_topic",
                               /*topic_type_name=*/"custom_type"};
    RTI_TEST_ASSERT(cfg.enabled());
    RTI_TEST_ASSERT_EQUALS_INT(42, cfg.domain_id());
    RTI_TEST_ASSERT(cfg.topic_name() == "custom_topic");
    RTI_TEST_ASSERT(cfg.topic_type_name() == "custom_type");
  }

  // ANO config/state currently just toggles the DDS transport placeholder; this
  // test ensures the hooks behave before real ANO hardware lands.
  void ano_config_and_state() {
    connext_lib::AnoConfig cfg{"channel", 4096, true};
    RTI_TEST_ASSERT(cfg.enabled());
    RTI_TEST_ASSERT(cfg.channel_name() == "channel");
    RTI_TEST_ASSERT_EQUALS_INT(4096, static_cast<int>(cfg.max_payload_bytes()));

    connext_lib::TransportState state(cfg);
    RTI_TEST_ASSERT(!state.ShouldUseAno());
    state.ActivateAno();
    RTI_TEST_ASSERT(state.ShouldUseAno());
    state.DeactivateAno();
    RTI_TEST_ASSERT(!state.ShouldUseAno());
  }

 private:
  ConfigTester() : rti::test::Tester("connext_lib_config_tests") {
    RTI_TEST_FUNCTION_ADD(ConfigTester, dds_config_defaults);
    RTI_TEST_FUNCTION_ADD(ConfigTester, dds_config_custom_ctor);
    RTI_TEST_FUNCTION_ADD(ConfigTester, ano_config_and_state);
  }

  friend class rti::test::Singleton<ConfigTester>;
};

class ConfigTestContainer : public rti::test::TesterContainer,
                            public rti::test::Singleton<ConfigTestContainer> {
 private:
  ConfigTestContainer() : rti::test::TesterContainer("connext_lib_config") {
    add_tester<ConfigTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConfigTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ConfigTestContainer::get_instance().run_tests(argc, argv);
}
