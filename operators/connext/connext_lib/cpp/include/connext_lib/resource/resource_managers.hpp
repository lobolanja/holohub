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
class ReceiverResourcesManagerInterface {
 public:
  virtual ~ReceiverResourcesManagerInterface() = default;
  virtual bool announce() = 0;
};
class SenderResourcesManagerInterface {
 public:
  virtual ~SenderResourcesManagerInterface() = default;
  virtual void startProcessing(std::chrono::milliseconds poll_interval) = 0;
  virtual void stopProcessing() = 0;
  [[nodiscard]] virtual std::map<std::string, std::string> destinations() const = 0;
};
class AbstractSenderResourcesManager : public SenderResourcesManagerInterface {
 public:
  AbstractSenderResourcesManager();
  ~AbstractSenderResourcesManager() override;
  void startProcessing(std::chrono::milliseconds poll_interval) override;
  void stopProcessing() override;
  [[nodiscard]] std::map<std::string, std::string> destinations() const override;
 protected:
  void registerReceiver(std::string destination, std::string buffer_id);
  void unregisterReceiver(const std::string& destination);
  void requestStop();
  virtual void pollOnce() = 0;
 private:
  void workerLoop(std::chrono::milliseconds poll_interval);
  std::thread worker_;
  std::atomic<bool> should_stop_{true};
  mutable std::mutex resources_mutex_;
  std::map<std::string, std::string> resources_;
};
}  // namespace connext_lib
