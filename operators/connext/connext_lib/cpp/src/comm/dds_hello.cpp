#include "connext_lib/comm/dds_hello.hpp"

#include <chrono>
#include <exception>
#include <string>
#include <thread>

#include "dds/core/QosProvider.hpp"
#include "dds/dds.hpp"
#include "dds/pub/qos/DataWriterQos.hpp"
#include "dds/sub/qos/DataReaderQos.hpp"

namespace connext_lib {

bool dds_hello_world_roundtrip(const std::string& message, int domain_id) {
  try {
    dds::domain::DomainParticipant participant(domain_id);
    dds::topic::Topic<dds::core::StringTopicType> topic(participant, "holohub::dds_hello_world");

    // set the QoS profile for reader and writer to Pattern.Status
    dds::pub::qos::DataWriterQos writer_qos = dds::core::QosProvider::Default().datawriter_qos(
            "BuiltinQosLib::Pattern.Status");
    dds::sub::qos::DataReaderQos reader_qos = dds::core::QosProvider::Default().datareader_qos(
            "BuiltinQosLib::Pattern.Status");

    dds::pub::Publisher publisher(participant);
    dds::sub::Subscriber subscriber(participant);

    dds::pub::DataWriter<dds::core::StringTopicType> writer(publisher, topic, writer_qos);
    dds::sub::DataReader<dds::core::StringTopicType> reader(subscriber, topic, reader_qos);

    writer.write(dds::core::StringTopicType(message));

    const auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < timeout) {
      dds::sub::LoanedSamples<dds::core::StringTopicType> samples = reader.take();
      for (const auto& sample : samples) {
        if (sample.info().valid()) {
          return sample.data() == message;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  } catch (const std::exception&) {
    return false;
  }

  return false;
}

}  // namespace connext_lib
