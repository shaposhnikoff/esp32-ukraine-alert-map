#pragma once
#include "alerts/store.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace alerts {
class Connection {
public:
    explicit Connection(StateStore& store);
    ~Connection();
private:
    StateStore& store_;
    std::atomic<bool> stopping_{};
    std::mutex mutex_;
    std::condition_variable signal_;
    std::thread thread_;
    void run();
};
}
