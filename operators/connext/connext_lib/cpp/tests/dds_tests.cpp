#include "connext_lib.hpp"
#include "connext_lib/comm/dds_hello.hpp"

#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {

class DdsTester : public rti::test::Tester,
                  public rti::test::Singleton<DdsTester> {
 public:
  void version_reports_placeholder() {
    RTI_TEST_ASSERT_EQUALS_INT(100, connext_lib::version());
  }

  void dds_hello_roundtrip_succeeds() {
    RTI_TEST_ASSERT(connext_lib::dds_hello_world_roundtrip(
        "Holoscan DDS roundtrip", domain_id()));
  }

 private:
  DdsTester() : rti::test::Tester("connext_lib_dds_tests") {
    RTI_TEST_FUNCTION_ADD(DdsTester, version_reports_placeholder);
    RTI_TEST_FUNCTION_ADD(DdsTester, dds_hello_roundtrip_succeeds);
  }

  friend class rti::test::Singleton<DdsTester>;
};

class DdsTestContainer : public rti::test::TesterContainer,
                         public rti::test::Singleton<DdsTestContainer> {
 private:
  DdsTestContainer() : rti::test::TesterContainer("connext_lib_dds") {
    add_tester<DdsTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<DdsTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return DdsTestContainer::get_instance().run_tests(argc, argv);
}
