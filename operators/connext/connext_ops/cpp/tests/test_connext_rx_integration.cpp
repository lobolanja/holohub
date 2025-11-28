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

#include <gtest/gtest.h>
#include <holoscan/holoscan.hpp>
#include <connext_ops/connext_rx.hpp>
#include <thread>
#include <chrono>
#include <string>

using namespace holoscan;
using namespace holoscan::ops;

namespace {

constexpr char kTestPayload[] = "integration_test_payload";
constexpr int kDomainId = 42;
constexpr char kTopicName[] = "TestTopic";
constexpr char kAnoChannel[] = "TestAnoChannel";

class DummySourceOp : public Operator {
 public:
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
  std::string received_payload;
  void setup(OperatorSpec& spec) override {
    spec.input<nvidia::gxf::Entity>("input");
  }
  void compute(InputContext& input, OutputContext&, ExecutionContext&) override {
    auto entity = input.receive<nvidia::gxf::Entity>("input").value();
    auto tensor = entity.get<nvidia::gxf::Tensor>("payload");
    if (tensor) {
      const auto* data = static_cast<const uint8_t*>(tensor->data());
      received_payload.assign(reinterpret_cast<const char*>(data), tensor->getShape().dimension(0));
    }
  }
};

void run_connext_rx_test(bool enable_dds, bool enable_ano) {
  Fragment fragment;
  auto source = fragment.make_operator<DummySourceOp>("source");
  auto rx = fragment.make_operator<ConnextRxOp>("rx");
  auto sink = fragment.make_operator<TestReceiverOp>("sink");

  rx->add_arg(Arg("enable_dds", enable_dds));
  rx->add_arg(Arg("enable_ano", enable_ano));
  rx->add_arg(Arg("domain_id", kDomainId));
  rx->add_arg(Arg("topic_name", kTopicName));
  rx->add_arg(Arg("ano_channel", kAnoChannel));

  fragment.add_flow(source, rx, { {"output", "input"} });
  fragment.add_flow(rx, sink, { {"output", "input"} });

  Executor executor(&fragment);
  executor.run();

  std::this_thread::sleep_for(std::chrono::seconds(2));
  auto* sink_op = static_cast<TestReceiverOp*>(sink.get());
  ASSERT_EQ(sink_op->received_payload, kTestPayload);
}

TEST(ConnextRxIntegration, DDS) {
  run_connext_rx_test(true, false);
}

TEST(ConnextRxIntegration, ANO) {
  run_connext_rx_test(false, true);
}

} // namespace
