// Unit test for ANOPayloadReader using injected fake IGpuDirectNetworkReceiver
#include "connext_lib/transport/payload_transport_ano.hpp"
#include "connext_ano_lib/gpu_direct_network_receiver.h"

#include "ndds/rtitest/Tester.hpp"
#include "ndds/rtitest/test_setting_impl.h"
#include <cstring>

namespace {

using namespace rti::test;

class FakeReceiver : public holoscan::ops::IGpuDirectNetworkReceiver {
 public:
  explicit FakeReceiver(const std::vector<uint8_t>& payload) : returned_(false), freed_(false), size_(payload.size()) {
    buf_ = new uint8_t[size_];
    std::memcpy(buf_, payload.data(), size_);
  }
  ~FakeReceiver() override {
    // if not freed by test, make sure to free
    if (buf_) delete[] buf_;
  }

  std::optional<holoscan::ops::ReceivedData> receive() override {
    if (returned_) return std::nullopt;
    returned_ = true;
    holoscan::ops::ReceivedData rd;
    rd.gpu_payload = static_cast<void*>(buf_);
    rd.payload_bytes = size_;
    return rd;
  }

  void free_received_data(void* gpu_payload) override {
    if (gpu_payload == buf_) {
      delete[] buf_;
      buf_ = nullptr;
      freed_ = true;
    }
  }

  size_t max_payload_size() const override { return 64 * 1024; }
  holoscan::ops::ReceptionStats get_stats() const override { return {}; }
  void reset_stats() override {}

  bool was_freed() const { return freed_; }

 private:
  uint8_t* buf_;
  size_t size_;
  bool returned_;
  bool freed_;
};

class ANOPayloadReaderTester : public Tester, public Singleton<ANOPayloadReaderTester> {
 public:
  void readNext_and_freeData_delegate_to_receiver() {
    // Payload to be returned by fake receiver
    std::vector<uint8_t> payload{10,20,30,40};

    // Create fake receiver and keep raw pointer to inspect freed flag later
    auto* raw = new FakeReceiver(payload);
    std::unique_ptr<holoscan::ops::IGpuDirectNetworkReceiver> up(raw);

    connext_lib::PayloadReaderOptions opts;
    opts.expected_payload_bytes = 1024;
    connext_lib::AnoConfig ano_cfg;

    // Construct ANOPayloadReader directly with injected receiver
    connext_lib::ANOPayloadReader reader(dds::domain::DomainParticipant(0), opts, ano_cfg, std::move(up));

    void* data_ptr = nullptr;
    std::size_t size = 0;

    bool ok = reader.readNext(data_ptr, size, std::chrono::milliseconds(100));
    RTI_TEST_ASSERT(ok);
    RTI_TEST_ASSERT(data_ptr != nullptr);
    RTI_TEST_ASSERT_EQUALS_INT(static_cast<int>(size), static_cast<int>(payload.size()));

    // Freeing should delegate to the receiver
    reader.freeData(data_ptr);
    RTI_TEST_ASSERT(raw->was_freed());
  }

 private:
  ANOPayloadReaderTester() : Tester("connext_lib_payload_transport_ano_reader_tests") {
    RTI_TEST_FUNCTION_ADD(ANOPayloadReaderTester, readNext_and_freeData_delegate_to_receiver);
  }
  friend class Singleton<ANOPayloadReaderTester>;
};

class ANOPayloadReaderTestContainer : public TesterContainer, public Singleton<ANOPayloadReaderTestContainer> {
 private:
  ANOPayloadReaderTestContainer() : TesterContainer("connext_lib_payload_transport_ano_reader") {
    add_tester<ANOPayloadReaderTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return TesterContainer::on_tests_begin(setting);
  }
  friend class rti::test::Singleton<ANOPayloadReaderTestContainer>;
};

}  // namespace

int main(int argc, char** argv) {
  return ANOPayloadReaderTestContainer::get_instance().run_tests(argc, argv);
}
