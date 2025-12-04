/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <holoscan/holoscan.hpp>
#include <connext_ops/connext_rx.hpp>
#include <connext_ops/connext_tx.hpp>
#include <ndds/rtitest/Tester.hpp>
#include <ndds/rtitest/test_setting_impl.h>
#include <holoscan/core/conditions/gxf/count.hpp>
#include <thread>
#include <chrono>
#include <string>
#include <cstring>

using namespace holoscan;
using namespace holoscan::ops;

namespace {

constexpr char kTestPayload[] = "integration_test_payload";
constexpr int kDomainId = 42;
constexpr char kTopicName[] = "TestTopic";
constexpr char kAnoChannel[] = "TestAnoChannel";

class DummySourceOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(DummySourceOp)
  DummySourceOp() = default;
  void setup(OperatorSpec& spec) override {
    spec.output<nvidia::gxf::Entity>("output");
  }
  void compute(InputContext&, OutputContext& output, ExecutionContext& context) override {
    auto entity = nvidia::gxf::Entity::New(context.context());
    auto payload_tensor = entity.value().add<nvidia::gxf::Tensor>("payload");
    const char* payload = kTestPayload;
    nvidia::gxf::Shape payload_shape{static_cast<int32_t>(strlen(payload))};
    payload_tensor.value()->wrapMemory(
        payload_shape,
        nvidia::gxf::PrimitiveType::kUnsigned8,
        sizeof(std::uint8_t),
        nvidia::gxf::ComputeTrivialStrides(payload_shape, sizeof(std::uint8_t)),
        nvidia::gxf::MemoryStorageType::kSystem,
        (void*)payload,
        nullptr);
    output.emit(entity.value(), "output");
  }
};

class TestReceiverOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TestReceiverOp)
  TestReceiverOp() = default;
  std::string received_payload;
  void setup(OperatorSpec& spec) override {
    spec.input<nvidia::gxf::Entity>("input");
  }
  void compute(InputContext& input, OutputContext&, ExecutionContext&) override {
    auto entity = input.receive<nvidia::gxf::Entity>("input").value();
    auto tensor = entity.get<nvidia::gxf::Tensor>("payload");
    if (tensor) {
      auto tensor_handle = tensor.value();
      auto data_expected = tensor_handle->data<uint8_t>();
      if (data_expected) {
        const auto* data = data_expected.value();
        const auto& shape = tensor_handle->shape();
        received_payload.assign(reinterpret_cast<const char*>(data), shape.dimension(0));
      }
    }
  }
};

class ConnextOpsApp : public Application {
 public:
 ConnextOpsApp(bool enable_dds, bool enable_ano)
      : enable_dds_(enable_dds), enable_ano_(enable_ano) {}

  const std::string& received_payload() const { return sink_op_->received_payload; }

  void compose() override {
    auto source_count = make_condition<CountCondition>(3);
    auto rx_count = make_condition<CountCondition>(3);

    auto source = make_operator<DummySourceOp>("source", source_count);
    auto tx = make_operator<ConnextTxOp>("tx",
                                         Arg("enable_dds", enable_dds_),
                                         Arg("enable_ano", enable_ano_),
                                         Arg("domain_id", kDomainId),
                                         Arg("topic_name", kTopicName),
                                         Arg("ano_channel", kAnoChannel));
    auto rx = make_operator<ConnextRxOp>("rx",
                                         Arg("enable_dds", enable_dds_),
                                         Arg("enable_ano", enable_ano_),
                                         Arg("domain_id", kDomainId),
                                         Arg("topic_name", kTopicName),
                                         Arg("ano_channel", kAnoChannel),
                                         rx_count);
    sink_op_ = make_operator<TestReceiverOp>("sink");

    add_flow(source, tx, {{"output", "input"}});
    add_flow(rx, sink_op_, {{"output", "input"}});
  }

 private:
  bool enable_dds_;
  bool enable_ano_;
  std::shared_ptr<TestReceiverOp> sink_op_;
};

void run_connext_tx_rx_roundtrip_test(bool enable_dds, bool enable_ano) {
  ConnextOpsApp app(enable_dds, enable_ano);
  app.run();
  RTI_TEST_ASSERT(app.received_payload() == kTestPayload);
}

class ConnextOpsIntegrationTester : public rti::test::Tester,
                                    public rti::test::Singleton<ConnextOpsIntegrationTester> {
 public:
  void dds_roundtrip() { run_connext_tx_rx_roundtrip_test(true, false); }
  void ano_roundtrip() { run_connext_tx_rx_roundtrip_test(false, true); }

 private:
  ConnextOpsIntegrationTester() : rti::test::Tester("connext_ops_integration_tests") {
    RTI_TEST_FUNCTION_ADD(ConnextOpsIntegrationTester, dds_roundtrip);
    RTI_TEST_FUNCTION_ADD(ConnextOpsIntegrationTester, ano_roundtrip);
  }

  friend class rti::test::Singleton<ConnextOpsIntegrationTester>;
};

class ConnextOpsIntegrationTestContainer : public rti::test::TesterContainer,
                                           public rti::test::Singleton<ConnextOpsIntegrationTestContainer> {
 private:
  ConnextOpsIntegrationTestContainer()
      : rti::test::TesterContainer("connext_ops_integration") {
    add_tester<ConnextOpsIntegrationTester>();
  }

  bool on_tests_begin(const RTITestSetting& setting) override {
    RTITestSetting_setupStandalone();
    return rti::test::TesterContainer::on_tests_begin(setting);
  }

  friend class rti::test::Singleton<ConnextOpsIntegrationTestContainer>;
};

} // namespace

int main(int argc, char** argv) {
  return ConnextOpsIntegrationTestContainer::get_instance().run_tests(argc, argv);
}
