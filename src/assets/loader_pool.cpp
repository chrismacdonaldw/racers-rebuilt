#include "assets/loader_pool.hpp"

#include <algorithm>

namespace rr::assets {

LoaderPool::LoaderPool(std::size_t workerCount) {
    workers_.reserve(workerCount);
    for (std::size_t i = 0; i < workerCount; ++i) {
        workers_.emplace_back([this](const std::stop_token& stop) { Work(stop); });
    }
}

std::size_t LoaderPool::DefaultWorkerCount() {
    return std::max(std::thread::hardware_concurrency(), 1U) - 1;
}

void LoaderPool::ForEach(std::size_t count, const std::function<void(std::size_t)>& job) {
    std::unique_lock lock(mutex_);
    job_ = &job;
    count_ = count;
    next_ = 0;
    unfinished_ = count;
    wake_.notify_all();
    while (RunNext(lock)) {
    }
    finished_.wait(lock, [this] { return unfinished_ == 0; });
    job_ = nullptr;
}

void LoaderPool::Work(const std::stop_token& stop) {
    std::unique_lock lock(mutex_);
    while (wake_.wait(lock, stop, [this] { return job_ != nullptr && next_ < count_; })) {
        RunNext(lock);
    }
}

bool LoaderPool::RunNext(std::unique_lock<std::mutex>& lock) {
    if (job_ == nullptr || next_ >= count_) {
        return false;
    }
    const std::size_t index = next_++;
    const std::function<void(std::size_t)>& job = *job_;
    lock.unlock();
    job(index);
    lock.lock();
    if (--unfinished_ == 0) {
        finished_.notify_all();
    }
    return true;
}

}
