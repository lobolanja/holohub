#pragma once
// resource_managers.hpp
// Resource management interfaces for Holoscan Connext library.
// Provides base classes for announcing and tracking receiver resources,
// and for managing sender destinations. Used in resource manager tests.

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include "dds/core/SafeEnumeration.hpp"
#include "dds/core/policy/CorePolicy.hpp"
#include "rti/core/policy/CorePolicy.hpp"

namespace connext_lib {

// Alias for property set used in receiver announcements.
using ReceiverPropertySet = rti::core::policy::Property;

/**
 * Interface for announcing receiver resources to the system.
 * Implementations should provide a mechanism to make receiver resources discoverable.
 */
class ReceiverResourcesManagerInterface {
 public:
  virtual ~ReceiverResourcesManagerInterface() = default;
  /**
   * Announces receiver resources to the system.
   * Returns true if the announcement was successful.
   */
  virtual bool announce() = 0;
};

/**
 * Interface for tracking and exposing available receiver destinations.
 * Implementations should provide mechanisms to start/stop tracking and to query discovered destinations.
 */
class SenderResourcesManagerInterface {
 public:
  virtual ~SenderResourcesManagerInterface() = default;
  /**
   * Starts tracking available receiver resources at the given interval.
   */
  virtual void startProcessing(std::chrono::milliseconds poll_interval) = 0;
  /**
   * Stops tracking receiver resources.
   */
  virtual void stopProcessing() = 0;
  /**
   * Returns a map of discovered destinations (unique identifier to resource ID).
   */
  [[nodiscard]] virtual std::map<std::string, std::string> destinations() const = 0;
};

/**
 * Base class for sender resource managers that handles polling and resource tracking.
 * Provides thread management and safe access to discovered resources.
 * Derived classes should implement the polling logic for resource discovery.
 */
class AbstractSenderResourcesManager : public SenderResourcesManagerInterface {
 public:
  AbstractSenderResourcesManager();
  ~AbstractSenderResourcesManager() override;
  void startProcessing(std::chrono::milliseconds poll_interval) override;
  void stopProcessing() override;
  [[nodiscard]] std::map<std::string, std::string> destinations() const override;
 protected:
  /**
   * Registers a receiver with the given destination and resource ID.
   */
  void registerReceiver(std::string destination, std::string resource_id);
  /**
   * Unregisters a receiver by destination.
   */
  void unregisterReceiver(const std::string& destination);
  /**
   * Requests the polling loop to stop.
   */
  void requestStop();
  /**
   * Hook invoked by the polling loop to discover new resources.
   * Must be implemented by derived classes.
   */
  virtual void pollOnce() = 0;
 private:
  /**
   * Worker thread loop for polling resource announcements.
   */
  void workerLoop(std::chrono::milliseconds poll_interval);
  std::thread worker_;
  std::atomic<bool> should_stop_{true};
  mutable std::mutex resources_mutex_;
  std::map<std::string, std::string> resources_;
};

}  // namespace connext_lib
