#include "connext_lib/comm/connext_readers.hpp"
#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {

class ConnextReadersTester : public rti::test::Tester,
                            public rti::test::Singleton<ConnextReadersTester> {
 public:
  void basic_construction_and_read_samples() {
    const std::string topic = "TestTopic";
    const int domain = domain_id();
    const std::string test_message = "dds_reader_writer_roundtrip";
    const std::size_t max_payload_bytes = 1024;
    dds::domain::DomainParticipant dp(domain);

    // Create DDS writer
    connext_lib::DdsPayloadWriter writer(dp, topic, max_payload_bytes);
    connext_lib::PayloadBufferView buffer{
        reinterpret_cast<const std::uint8_t*>(test_message.data()),
        test_message.size()
    };
    writer.setBuffer(buffer);
    RTI_TEST_ASSERT(writer.writeTo("broadcast"));

    // Create DDS reader (SUT)
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);
    connext_lib::ConnextDDSReader reader(dds_config, poll_interval_ms);

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

  void ano_reader_construction_and_read_samples() {
    const std::string channel_name = "TestAnoChannel";
    const std::string buffer_id = "ano_buffer_test";
    const std::string topic = "TestAnoTopic";
    const int domain = domain_id();
    const std::string test_message = "ano_reader_writer_roundtrip";
    const std::size_t max_payload_bytes = 1024;

    // Create ANO and DDS configs
    connext_lib::AnoConfig ano_config(channel_name, buffer_id, max_payload_bytes, true);
    connext_lib::DdsConfig dds_config(true, domain, topic, "BytesTopicType");
    std::chrono::milliseconds poll_interval_ms(100);

    // Create ANO reader (SUT)
    connext_lib::ConnextANOReader reader(ano_config, dds_config, poll_interval_ms);

    dds::domain::DomainParticipant dp(domain);
    // Create DDS writer
    connext_lib::DdsPayloadWriter writer(dp, "GPU/"+topic, max_payload_bytes);
    connext_lib::PayloadBufferView buffer{
        reinterpret_cast<const std::uint8_t*>(test_message.data()),
        test_message.size()
    };
    writer.setBuffer(buffer);
    RTI_TEST_ASSERT(writer.writeTo(buffer_id));

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
  ConnextReadersTester() : rti::test::Tester("connext_lib_readers_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextReadersTester, basic_construction_and_read_samples);
    RTI_TEST_FUNCTION_ADD(ConnextReadersTester, ano_reader_construction_and_read_samples);
  }

  friend class rti::test::Singleton<ConnextReadersTester>;
};

class ConnextReadersTestContainer : public rti::test::TesterContainer,
                                    public rti::test::Singleton<ConnextReadersTestContainer> {
 private:
  ConnextReadersTestContainer() : rti::test::TesterContainer("connext_lib_readers") {
    add_tester<ConnextReadersTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConnextReadersTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ConnextReadersTestContainer::get_instance().run_tests(argc, argv);
}
