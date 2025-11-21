#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/comm/connext_readers.hpp"
#include "connext_lib/config/config.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include "dds/dds.hpp"
#include <chrono>
#include <string>
#include <vector>

namespace {
using namespace std::chrono_literals;

class ConnextANOIntegrationTester : public rti::test::Tester,
                                    public rti::test::Singleton<ConnextANOIntegrationTester> {
 public:
  void ano_writer_roundtrip() {
    const std::string channel_name = "TestAnoChannel";
    const std::string buffer_id = "ano_buffer_test";
    const std::string topic = "TestAnoTopic";
    const int domain = domain_id();
    const std::string test_message = "ano_writer_roundtrip_message";
    const std::size_t max_payload_bytes = 1024;

    connext_lib::AnoConfig ano_config(channel_name, buffer_id, max_payload_bytes, true);
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);

    // Writer (SUT)
    connext_lib::ConnextANOWriter writer(ano_config, dds_config, poll_interval_ms);

    // Reader
    connext_lib::ConnextANOReader reader(ano_config, dds_config, poll_interval_ms);

    // Allow discovery
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Write payload
    std::vector<std::uint8_t> payload_v(test_message.begin(), test_message.end());
    connext_lib::PayloadBufferView buffer_to_send{
      reinterpret_cast<const std::uint8_t*>(test_message.data()), test_message.size()};
    auto res = writer.broadcast(buffer_to_send);
    RTI_TEST_ASSERT(res>0);

    // Poll for sample availability up to timeout
    std::vector<std::uint8_t> samples;
    const int max_attempts = 30; // 3 seconds total
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
      samples = reader.readSamples();
      if (!samples.empty()) break;
      std::this_thread::sleep_for(poll_interval_ms);
    }
    std::string received(samples.begin(), samples.end());
    RTI_TEST_ASSERT(received == test_message);
  }

 private:
  ConnextANOIntegrationTester() : rti::test::Tester("connext_ano_integration_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextANOIntegrationTester, ano_writer_roundtrip);
  }
  friend class rti::test::Singleton<ConnextANOIntegrationTester>;
};

class ConnextANOIntegrationTestContainer : public rti::test::TesterContainer,
                                           public rti::test::Singleton<ConnextANOIntegrationTestContainer> {
 private:
  ConnextANOIntegrationTestContainer() : rti::test::TesterContainer("connext_ano_integration") {
    add_tester<ConnextANOIntegrationTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConnextANOIntegrationTestContainer>;
};

} // namespace

int main(int argc, char** argv) {
  return ConnextANOIntegrationTestContainer::get_instance().run_tests(argc, argv);
}

