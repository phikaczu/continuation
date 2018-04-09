#pragma once

#include "IThreadPool.h"

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class SimpleThreadPool final : public IThreadPool
{
public:
    using ExceptContainerType = std::vector<std::exception_ptr>;

    explicit SimpleThreadPool(std::size_t threadCount);
    /// Destroys the instance.
    /// \note Internally calls SimpleThreadPool::stop().
    /// \note Un-popped exceptions will be swallowed, for DEBUG and assertion is made.
    /// \note It is recommended to call SimpleThreadPool::stop() and SimpleThreadPool::popExceptions() methods before instance destruction.
    ~SimpleThreadPool();

    /// Starts the threads in the thread pool.
    /// \\note Successive calls without call to SimpleThreadPool::stop() in between has no effect.
    void start();
    /// Stops the threads in the thread pool.
    void stop();
    ExceptContainerType popExceptions();

private:
    // OPTIM
    // each thread could have a non-blocking FIFO as it's personal task queue
    void scheduleInner(MethodType&& method) override;
    void threadPoolMethod() noexcept;

    std::vector<std::unique_ptr<std::thread>> _threads;
    std::mutex _threadWaitMtx;
    std::condition_variable _threadWait;
    std::queue<MethodType> _taskQueue;
    std::size_t _threadCount;
    bool _run;

    std::mutex _exceptMtx;
    ExceptContainerType _exceptions;
};