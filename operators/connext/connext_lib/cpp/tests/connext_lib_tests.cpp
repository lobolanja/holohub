#include "connext_lib/connext_lib.hpp"
#include "connext_lib/dds_hello.hpp"

#include "ndds/rtitest/Tester.hpp"

class ConnextLibTester : public rti::test::Tester,
                         public rti::test::Singleton<ConnextLibTester> {
public:
  void version_reports_placeholder() {
    RTI_TEST_ASSERT_EQUALS_INT(100, connext_lib::version());
  }

  void dds_hello_roundtrip_succeeds() {
    RTI_TEST_ASSERT(connext_lib::dds_hello_world_roundtrip(
        "Holoscan DDS roundtrip", domain_id()));
  }

private:
  ConnextLibTester() : rti::test::Tester("connext_lib_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextLibTester, version_reports_placeholder);
    RTI_TEST_FUNCTION_ADD(ConnextLibTester, dds_hello_roundtrip_succeeds);
  }

  friend class rti::test::Singleton<ConnextLibTester>;
};

class ConnextLibTestContainer : public rti::test::TesterContainer,
                                public rti::test::Singleton<
                                    ConnextLibTestContainer> {
private:
  ConnextLibTestContainer() : rti::test::TesterContainer("connext_lib") {
    add_tester<ConnextLibTester>();
  }

  friend class rti::test::Singleton<ConnextLibTestContainer>;
};

int main(int argc, char **argv) {
  return ConnextLibTestContainer::get_instance().run_tests(argc, argv);
}
