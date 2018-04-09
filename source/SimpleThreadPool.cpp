#include "SimpleThreadPool.h"

#include <cassert>

SimpleThreadPool::SimpleThreadPool(std::size_t threadCount)
    : _threadCount{threadCount}
    , _run{false}
{
}

SimpleThreadPool::~SimpleThreadPool()
{
    stop();

    {
        std::lock_guard<std::mutex> lk(_exceptMtx);
        // Destructor is noexcept
        assert(_exceptions.empty());
    }
}

void SimpleThreadPool::start()
{
    if (!_threads.empty())
        return;
    _run = true;

    for (std::size_t threadNr = 0; threadNr < _threadCount; ++threadNr)
    {
        _threads.emplace_back(std::make_unique<std::thread>(&SimpleThreadPool::threadPoolMethod, this));
    }
}

void SimpleThreadPool::stop()
{
    {
        std::lock_guard<std::mutex> lk(_threadWaitMtx);
        _run = false;
        _threadWait.notify_all();
    }

    for (auto& thread : _threads)
    {
        if (thread->joinable())
        {
            thread->join();
        }
    }

    _threads.clear();
}

SimpleThreadPool::ExceptContainerType SimpleThreadPool::popExceptions()
{
    ExceptContainerType exceptions;

    {
        std::lock_guard<std::mutex> lk(_exceptMtx);
        exceptions = std::move(_exceptions);
        assert(_exceptions.empty());
    }

    return exceptions;
}

void SimpleThreadPool::scheduleInner(MethodType&& method)
{
    std::lock_guard<std::mutex> lk(_threadWaitMtx);
    _taskQueue.push(std::move(method));
    _threadWait.notify_one();
}

void SimpleThreadPool::threadPoolMethod() noexcept
{
    while (true)
    {
        try
        {
            MethodType task;

            {
                std::unique_lock<std::mutex> lk(_threadWaitMtx);
                _threadWait.wait(lk, [&] { return !_taskQueue.empty() || !_run; });
                if (!_run)
                    break;

                task = std::move(_taskQueue.front());
                _taskQueue.pop();
            }

            assert(task);
            (*task)();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lk(_exceptMtx);
            _exceptions.push_back(std::current_exception());
        }
    }
}
