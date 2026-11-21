#pragma once
#include "alerts/ui.hpp"
#include <mutex>
#include <utility>

namespace alerts {
class StateStore {
public:
    void publish(UiState state) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = std::move(state);
    }
    UiState read() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }
private:
    mutable std::mutex mutex_;
    UiState state_;
};
}  // namespace alerts
