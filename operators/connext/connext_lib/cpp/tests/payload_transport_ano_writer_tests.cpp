// Use the same RTI test framework as the other connext_lib tests.
#include "connext_lib/transport/payload_transport_ano.hpp"
#include "connext_lib/transport/sender_factory.hpp"

#include "connext_ano_lib/gpu_direct_network_sender.h"

#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"

namespace {

using namespace rti::test;

class FakeSender : public holoscan::ops::IGpuDirectNetworkSender {
 public:
  FakeSender() : called_(false), last_size_(0), ready_(false) {}
  void send(void* /*gpu_data*/, size_t size) override { called_ = true; last_size_ = size; }
  bool is_ready() const override { return ready_; }
  void set_ready(bool r) { ready_ = r; }

  size_t max_payload_size() const override { return static_cast<size_t>(64 * 1024); }
  holoscan::ops::TransmissionStats get_stats() const override {
    holoscan::ops::TransmissionStats s;
    s.bytes_transmitted = last_size_;
    s.packets_sent = (last_size_ > 0) ? 1 : 0;
    s.frames_dropped = 0;
    return s;
  }
  void reset_stats() override { /* no-op for fake */ }
  int flush(int /*timeout_ms*/) override { return 0; /* no-op for fake */ }

  bool called() const { return called_; }
  size_t last_size() const { return last_size_; }

 private:
  bool called_;
  size_t last_size_;
  bool ready_;
};

class TestSenderFactory : public connext_lib::ISenderFactory {
 public:
  TestSenderFactory() : last_created_(nullptr) {}
  std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> create_sender(const std::string& /*destination_reference*/) override {
    auto p = std::make_unique<FakeSender>();
    last_created_ = p.get();
    return p;
  }
    std::unique_ptr<holoscan::ops::IGpuDirectNetworkSender> create_sender(const connext_lib::DestinationInfo& /*dest*/) override {
      // For tests, behave same as string overload.
      auto p = std::make_unique<FakeSender>();
      last_created_ = p.get();
      return p;
    }

  FakeSender* last_created() const { return last_created_; }

 private:
  FakeSender* last_created_;
};

class PayloadTransportANOTester : public Tester,
                                public Singleton<PayloadTransportANOTester> {
 public:
  void writeTo_respects_sender_readiness() {
    auto factory = std::make_shared<TestSenderFactory>();
    // ANO config contains channel and max payload info
    connext_lib::AnoConfig ano_cfg;

    auto writer = connext_lib::MakeANOPayloadWriter(ano_cfg, factory);

    std::vector<uint8_t> payload{1,2,3,4,5};
    void* raw_ptr = static_cast<void*>(payload.data());
    connext_lib::MemoryBufferView view;
    view.ptr = raw_ptr;
    view.size_bytes = payload.size();
    view.is_device = false;

    writer->setBuffer(view);

    auto created = factory->last_created();
    RTI_TEST_ASSERT(created == nullptr);

    // Defining destination info so the writer can create a sender.
    connext_lib::DestinationInfo dest_info;
    dest_info.ip_addr = "127.0.0.1";
    dest_info.mac_addr = "aa:bb:cc:dd:ee:ff";
    dest_info.udp_port = static_cast<uint16_t>(1234);

    // The writer will use this to create a sender via the factory.
    auto ano_writer = dynamic_cast<connext_lib::ANOPayloadWriter*>(writer.get());
    RTI_TEST_ASSERT(ano_writer != nullptr);

    // Second attempt: writer should create a sender (which is not ready by default), so write should fail but sender exists.
    bool ok2 = writer->writeTo(dest_info.toString());
    RTI_TEST_ASSERT(!ok2);

    created = factory->last_created();
    RTI_TEST_ASSERT(created != nullptr);
    RTI_TEST_ASSERT(!created->called());

    // Now mark sender as ready and try again: write should succeed and send should be invoked.
    created->set_ready(true);
    bool ok3 = writer->writeTo(dest_info.toString());
    RTI_TEST_ASSERT(ok3);
    RTI_TEST_ASSERT(created->called());
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(created->last_size()), static_cast<int>(payload.size()));
  }

 private:
  PayloadTransportANOTester()
      : Tester("connext_lib_payload_transport_ano_tests") {
    RTI_TEST_FUNCTION_ADD(PayloadTransportANOTester, writeTo_respects_sender_readiness);
  }

  friend class Singleton<PayloadTransportANOTester>;
};

class PayloadTransportANOTestContainer : public TesterContainer,
                                       public Singleton<PayloadTransportANOTestContainer> {
 private:
  PayloadTransportANOTestContainer()
      : TesterContainer("connext_lib_payload_transport_ano") {
    add_tester<PayloadTransportANOTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return TesterContainer::on_tests_begin(setting);
  }
  
  friend class rti::test::Singleton<PayloadTransportANOTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return PayloadTransportANOTestContainer::get_instance().run_tests(argc, argv);
}
