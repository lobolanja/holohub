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
  std::cout<< "Starting sender resources manager processing loop with poll interval "
            << poll_interval.count() << " ms" << std::endl;
  
  // Stop any existing worker (cleanup before starting new one)
  if (worker_.joinable()) {
    std::cout << "Stopping existing sender resources manager worker before restarting" << std::endl;
    stopProcessing();
  }
  
  should_stop_.store(false);
  worker_ = std::thread(&AbstractSenderResourcesManager::workerLoop, this,
                        poll_interval);
}

void AbstractSenderResourcesManager::stopProcessing() {
  std::cout << "Stopping sender resources manager processing loop" << std::endl;
  should_stop_.store(true);
  if (worker_.joinable()) {
    // Joining here keeps teardown synchronous which helps tests avoid races.
    worker_.join();
  }
}

void AbstractSenderResourcesManager::registerReceiver(std::string destination,
                                                      std::string destination_info) {
  std::lock_guard<std::mutex> lock(resources_mutex_);
  std::cout << "Registering receiver: destination=" << destination
            << ", info=" << destination_info << std::endl;
  // Overwrite existing entries so reconnects simply refresh the resource info.
  resources_[std::move(destination)] = std::move(destination_info);
}

void AbstractSenderResourcesManager::unregisterReceiver(
    const std::string& destination) {
  std::lock_guard<std::mutex> lock(resources_mutex_);
  int del = resources_.erase(destination);
  std::cout << "Unregistered receiver: destination=" << destination << ", removed entries=" << del << std::endl;
}

void AbstractSenderResourcesManager::requestStop() {
  should_stop_.store(true);
}

void AbstractSenderResourcesManager::workerLoop(const std::chrono::milliseconds poll_interval) {
  std::cout << "Sender resources manager worker loop started with poll interval "
            << poll_interval.count() << " ms" << std::endl;
  
  int iteration_count = 0;
  while (!should_stop_.load()) {
    try {
      // Derived managers fetch DDS samples and invoke Register/Unregister within
      // this hook. Keeping the loop tiny makes behavior easy to reason about.
      pollOnce();
      
      // Log periodic heartbeat every 50 iterations (5 seconds with 100ms poll)
      if (++iteration_count % 50 == 0) {
        std::cout << "Sender resources manager worker still polling (iteration " 
                  << iteration_count << ")" << std::endl;
      }
    } catch (const std::exception& e) {
      std::cerr << "Exception in pollOnce(): " << e.what() 
                << " - continuing worker loop" << std::endl;
    } catch (...) {
      std::cerr << "Unknown exception in pollOnce() - continuing worker loop" << std::endl;
    }
    std::this_thread::sleep_for(poll_interval);
  }
  
  std::cout << "Sender resources manager worker loop exiting (should_stop=" 
            << should_stop_.load() << ", iterations=" << iteration_count << ")" << std::endl;
}

std::map<std::string, std::string> AbstractSenderResourcesManager::destinations() const {
  std::lock_guard<std::mutex> lock(resources_mutex_);
  return resources_;
}

}  // namespace connext_lib
