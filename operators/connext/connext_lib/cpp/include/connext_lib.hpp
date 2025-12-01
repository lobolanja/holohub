// SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
// SPDX-License-Identifier: Apache-2.0
#pragma once

/**
 * @file connext_lib.hpp
 * @brief Public API for the Holoscan Connext Library
 *
 * This header provides the complete public interface for the Connext library,
 * which enables communication between Holoscan applications using RTI Connext DDS
 * and Advanced Network Objects (ANO) transports.
 *
 * ## Overview
 *
 * The Connext library provides high-level abstractions for publishing and subscribing
 * to payload data streams. It supports two transport mechanisms:
 * - **DDS Transport**: Standards-based publish-subscribe using RTI Connext DDS
 * - **ANO Transport**: High-performance zero-copy transport for RDMA-capable networks
 *
 * ## Basic Usage
 *
 * ### Configuration
 * Configure DDS and ANO settings using configuration objects:
 * ```cpp
 * connext_lib::DdsConfig dds_config(
 *     true,                    // enabled
 *     0,                       // domain_id
 *     "MyTopic",              // topic_name
 *     "BytesTopicType"        // topic_type_name
 * );
 *
 * connext_lib::AnoConfig ano_config(
 *     "my_channel",           // channel_name
 *     "buffer_01",            // buffer_id
 *     1024,                   // max_payload_bytes
 *     true                    // enabled
 * );
 * ```
 *
 * ### Writing Data
 * Create a writer and broadcast payloads:
 * ```cpp
 * connext_lib::ConnextANOWriter writer(ano_config, dds_config, 100ms);
 *
 * std::string message = "Hello, Connext!";
 * connext_lib::PayloadBufferView buffer{
 *     reinterpret_cast<const uint8_t*>(message.data()),
 *     message.size()
 * };
 *
 * size_t sent = writer.broadcast(buffer);
 * ```
 *
 * ### Reading Data
 * Create a reader and poll for samples:
 * ```cpp
 * connext_lib::ConnextANOReader reader(ano_config, dds_config, 100ms);
 *
 * std::vector<uint8_t> samples = reader.readSamples();
 * if (!samples.empty()) {
 *     std::string received(samples.begin(), samples.end());
 *     // Process received data...
 * }
 * ```
 *
 * ## Thread Safety
 *
 * - Configuration objects (`DdsConfig`, `AnoConfig`) are not thread-safe.
 *   Initialize them before passing to writers/readers.
 * - Writer and reader objects manage their own internal synchronization
 *   for DDS operations but should generally be used from a single thread.
 *
 * ## Memory Management
 *
 * - `PayloadBufferView` does not own the data it points to. Ensure the
 *   underlying buffer remains valid for the duration of the operation.
 * - Writers and readers manage their own resources using RAII principles.
 *
 * @see connext_lib::DdsConfig
 * @see connext_lib::AnoConfig
 * @see connext_lib::ConnextANOWriter
 * @see connext_lib::ConnextANOReader
 * @see connext_lib::ConnextDDSWriter
 * @see connext_lib::ConnextDDSReader
 */

// Configuration types
#include "connext_lib/config/config.hpp"

// Transport data structures
#include "connext_lib/transport/payload_transport.hpp"

// High-level communication interfaces
#include "connext_lib/comm/connext_writers.hpp"
#include "connext_lib/comm/connext_readers.hpp"

/**
 * @namespace connext_lib
 * @brief Root namespace for the Holoscan Connext Library
 *
 * All public types and functions are defined within this namespace.
 */
namespace connext_lib {

/**
 * @brief Returns the library version
 *
 * The version is encoded as: major * 10000 + minor * 100 + patch
 *
 * @return Integer representing the library version (e.g., 100 for v0.1.0)
 *
 * @example
 * ```cpp
 * int ver = connext_lib::version();
 * int major = ver / 10000;
 * int minor = (ver % 10000) / 100;
 * int patch = ver % 100;
 * ```
 */
inline int version(){
  // Placeholder semantic version encoded as major * 10000 + minor * 100 + patch.
  return 100;  // 0.1.0
}

}  // namespace connext_lib
