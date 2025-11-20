#pragma once

#include "config.hpp"

#include <memory>
#include <string>
#include <vector>
#include "connext_lib/payload_transport_dds.hpp"
#include "connext_lib/resource_managers_dds.hpp"
#include "connext_lib/connext_rx.hpp"

namespace connext_lib {

class ConnextDDSReader {
public:
    explicit ConnextDDSReader(const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms);
    [[nodiscard]] std::vector<std::uint8_t> readSamples() const;
private:
    std::unique_ptr<PayloadReaderInterface> payload_reader_;
    std::chrono::milliseconds poll_interval_ms_;
};

class ConnextANOReader {
public:
    ConnextANOReader(const AnoConfig& ano_config, const DdsConfig& dds_config, std::chrono::milliseconds poll_interval_ms);
    [[nodiscard]] std::vector<std::uint8_t> readSamples() const;
private:
    std::unique_ptr<ConnextRx> rx_;
    std::chrono::milliseconds poll_interval_ms_;
};

} // namespace connext_lib
