#pragma once

#include <atomic>
#include <chrono>
#include <map>
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

  /** Build the property set that will be injected into DDS QoS. */
  virtual ReceiverPropertySet BuildProperties() const = 0;

  /** Apply the supplied property set to the DDS entity. */
  virtual bool ApplyProperties(const ReceiverPropertySet& properties) = 0;

  /** Convenience wrapper that builds then applies the QoS properties. */
  bool Announce() { return ApplyProperties(BuildProperties()); }
};

/** Interface that consumes receiver announcements and exposes destinations. */
class SenderResourcesManagerInterface {
 public:
  virtual ~SenderResourcesManagerInterface() = default;

  virtual void StartProcessing(std::chrono::milliseconds poll_interval) = 0;
  virtual void StopProcessing() = 0;

  virtual const std::map<std::string, std::string>& Destinations() const = 0;
};

/** Base class that implements polling boilerplate for sender managers.
 */
class AbstractSenderResourcesManager : public SenderResourcesManagerInterface {
 public:
  AbstractSenderResourcesManager();
  ~AbstractSenderResourcesManager() override;

  void StartProcessing(std::chrono::milliseconds poll_interval) override;
  void StopProcessing() override;

  const std::map<std::string, std::string>& Destinations() const override {
    return resources_;
  }

 protected:
  /** Called by derived classes whenever a new receiver is discovered. */
  void RegisterReceiver(std::string destination, std::string buffer_id);

  /** Remove a receiver entry when it becomes unavailable. */
  void UnregisterReceiver(const std::string& destination);

  /** Utility for derived classes that wish to stop the polling loop. */
  void RequestStop();

  /** Hook invoked by the polling loop to fetch fresh DDS samples. */
  virtual void PollOnce() = 0;

 private:
  void WorkerLoop(std::chrono::milliseconds poll_interval);

  std::thread worker_;
  std::atomic<bool> should_stop_{true};
  std::map<std::string, std::string> resources_;
};

}  // namespace connext_lib
