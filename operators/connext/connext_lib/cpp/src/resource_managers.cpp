#include "connext_lib/resource_managers.hpp"

#include <thread>

namespace connext_lib {

AbstractSenderResourcesManager::AbstractSenderResourcesManager() = default;

AbstractSenderResourcesManager::~AbstractSenderResourcesManager() {
  StopProcessing();
}

void AbstractSenderResourcesManager::StartProcessing(
    std::chrono::milliseconds poll_interval) {
  // Always stop any existing worker before spinning up another to avoid
  // accidental thread leaks when operators restart.
  StopProcessing();
  should_stop_.store(false);
  worker_ = std::thread(&AbstractSenderResourcesManager::WorkerLoop, this,
                        poll_interval);
}

void AbstractSenderResourcesManager::StopProcessing() {
  should_stop_.store(true);
  if (worker_.joinable()) {
    // Joining here keeps teardown synchronous which helps tests avoid races.
    worker_.join();
  }
}

void AbstractSenderResourcesManager::RegisterReceiver(std::string destination,
                                                      std::string buffer_id) {
  // Overwrite existing entries so reconnects simply refresh the buffer id.
  resources_[std::move(destination)] = std::move(buffer_id);
}

void AbstractSenderResourcesManager::UnregisterReceiver(
    const std::string& destination) {
  resources_.erase(destination);
}

void AbstractSenderResourcesManager::RequestStop() {
  should_stop_.store(true);
}

void AbstractSenderResourcesManager::WorkerLoop(
    std::chrono::milliseconds poll_interval) {
  while (!should_stop_.load()) {
    // Derived managers fetch DDS samples and invoke Register/Unregister within
    // this hook. Keeping the loop tiny makes behavior easy to reason about.
    PollOnce();
    std::this_thread::sleep_for(poll_interval);
  }
}

}  // namespace connext_lib
