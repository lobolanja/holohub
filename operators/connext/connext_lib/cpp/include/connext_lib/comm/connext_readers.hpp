#pragma once
// connext_readers.hpp
// High-level DDS and ANO reader interfaces for Holoscan Connext library.
// Provides classes to read payload samples from DDS and ANO transports.

#include "connext_lib/config/config.hpp"
#include <memory>
#include <string>
#include <vector>
#include "connext_lib/transport/payload_transport_dds.hpp"
#include "connext_lib/resource/resource_managers_dds.hpp"
#include "connext_lib/comm/connext_rx.hpp"

namespace connext_lib {

/**
 * DDS Reader for payload samples.
 * Wraps a DDS payload reader and configuration, providing a simple interface
 * to poll for new samples. Used in tests for roundtrip validation.
 */
class ConnextDDSReader {
public:
    ConnextDDSReader(const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms = std::chrono::milliseconds(100));
    /**
     * Polls for new samples from DDS. Returns a vector of bytes if available.
     * Used in tests to verify roundtrip message delivery.
     */
    [[nodiscard]] std::vector<std::uint8_t> readSamples() const;
private:
    std::unique_ptr<PayloadReaderInterface> payload_reader_;
    std::chrono::milliseconds poll_interval_ms_;
};

/**
 * ANO Reader for payload samples.
 * Wraps an ANO receiver and configuration, providing a simple interface
 * to poll for new samples. Used in tests for ANO roundtrip validation.
 */
class ConnextANOReader {
public:
    ConnextANOReader(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms);
    /**
     * Polls for new samples from ANO. Returns a vector of bytes if available.
     * Used in tests to verify ANO message delivery.
     */
    [[nodiscard]] std::vector<std::uint8_t> readSamples() const;
private:
    std::unique_ptr<ConnextRx> rx_;
    std::chrono::milliseconds poll_interval_ms_;
};

} // namespace connext_lib
