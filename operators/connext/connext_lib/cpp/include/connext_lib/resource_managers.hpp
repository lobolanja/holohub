#pragma once

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

using ReceiverPropertySet = rti::core::policy::Property;

/** Interface responsible for advertising receiver buffer references over DDS. */
class ReceiverResourcesManagerInterface {

 public:
  virtual ~ReceiverResourcesManagerInterface() = default;

  /** Convenience wrapper that builds then applies the QoS properties. */
  virtual bool announce() = 0;
};

/** Interface that consumes receiver announcements and exposes destinations. */
class SenderResourcesManagerInterface {
 public:
  virtual ~SenderResourcesManagerInterface() = default;

  virtual void startProcessing(std::chrono::milliseconds poll_interval) = 0;
  virtual void stopProcessing() = 0;

  [[nodiscard]] virtual std::map<std::string, std::string> destinations() const = 0;
};

/** Base class that implements polling boilerplate for sender managers.
 */
class AbstractSenderResourcesManager : public SenderResourcesManagerInterface {
 public:
  AbstractSenderResourcesManager();
  ~AbstractSenderResourcesManager() override;

  void startProcessing(std::chrono::milliseconds poll_interval) override;
  void stopProcessing() override;

  // Return a copy to avoid holding the lock during iteration
  [[nodiscard]] std::map<std::string, std::string> destinations() const override;

 protected:
  /** Called by derived classes whenever a new receiver is discovered. */
  void registerReceiver(std::string destination, std::string buffer_id);

  /** Remove a receiver entry when it becomes unavailable. */
  void unregisterReceiver(const std::string& destination);

  /** Utility for derived classes that wish to stop the polling loop. */
  void requestStop();

  /** Hook invoked by the polling loop to fetch fresh DDS samples. */
  virtual void pollOnce() = 0;

 private:
  void workerLoop(std::chrono::milliseconds poll_interval);

  std::thread worker_;
  std::atomic<bool> should_stop_{true};
  mutable std::mutex resources_mutex_;
  std::map<std::string, std::string> resources_;
};

}  // namespace connext_lib
