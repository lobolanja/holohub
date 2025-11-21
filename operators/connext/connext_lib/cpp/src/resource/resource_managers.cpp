#include "connext_lib/resource/resource_managers.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>

namespace connext_lib {

AbstractSenderResourcesManager::AbstractSenderResourcesManager() = default;

AbstractSenderResourcesManager::~AbstractSenderResourcesManager() {
  AbstractSenderResourcesManager::stopProcessing();
}

void AbstractSenderResourcesManager::startProcessing(
    std::chrono::milliseconds poll_interval) {
  // Always stop any existing worker before spinning up another to avoid
  // accidental thread leaks when operators restart.
  stopProcessing();
  should_stop_.store(false);
  worker_ = std::thread(&AbstractSenderResourcesManager::workerLoop, this,
                        poll_interval);
}

void AbstractSenderResourcesManager::stopProcessing() {
  should_stop_.store(true);
  if (worker_.joinable()) {
    // Joining here keeps teardown synchronous which helps tests avoid races.
    worker_.join();
  }
}

void AbstractSenderResourcesManager::registerReceiver(std::string destination,
                                                      std::string buffer_id) {
  std::lock_guard<std::mutex> lock(resources_mutex_);
  // Overwrite existing entries so reconnects simply refresh the buffer id.
  resources_[std::move(destination)] = std::move(buffer_id);
}

void AbstractSenderResourcesManager::unregisterReceiver(
    const std::string& destination) {
  std::lock_guard<std::mutex> lock(resources_mutex_);
  resources_.erase(destination);
}

void AbstractSenderResourcesManager::requestStop() {
  should_stop_.store(true);
}

void AbstractSenderResourcesManager::workerLoop(const std::chrono::milliseconds poll_interval) {
  while (!should_stop_.load()) {
    // Derived managers fetch DDS samples and invoke Register/Unregister within
    // this hook. Keeping the loop tiny makes behavior easy to reason about.
    pollOnce();
    std::this_thread::sleep_for(poll_interval);
  }
}

std::map<std::string, std::string> AbstractSenderResourcesManager::destinations() const {
  std::lock_guard<std::mutex> lock(resources_mutex_);
  return resources_;
}

}  // namespace connext_lib
