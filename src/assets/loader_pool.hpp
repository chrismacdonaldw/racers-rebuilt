#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace rr::assets {

class LoaderPool {
public:
    explicit LoaderPool(std::size_t workerCount);

    LoaderPool(const LoaderPool&) = delete;
    LoaderPool& operator=(const LoaderPool&) = delete;
    LoaderPool(LoaderPool&&) = delete;
    LoaderPool& operator=(LoaderPool&&) = delete;
    ~LoaderPool() = default;

    /// One fewer than the hardware threads, so the calling thread has a core too.
    [[nodiscard]] static std::size_t DefaultWorkerCount();

    /// Runs `job` for each index below `count`, on the workers and the caller, and returns when
    /// all have finished. `job` must not throw. Only one thread may call this.
    void ForEach(std::size_t count, const std::function<void(std::size_t)>& job);

private:
    void Work(const std::stop_token& stop);
    bool RunNext(std::unique_lock<std::mutex>& lock);

    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::condition_variable finished_;
    const std::function<void(std::size_t)>* job_ = nullptr;
    std::size_t count_ = 0;
    std::size_t next_ = 0;
    std::size_t unfinished_ = 0;
    std::vector<std::jthread> workers_;
};

}
