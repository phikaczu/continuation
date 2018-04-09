#include <gtest\gtest.h>
#include <array>
#include <atomic>
#include <stdexcept>
#include <unordered_map>

#include "SimpleThreadPool.h"

// TODO mock std::thread used by the SimpleThreadPool and add true unit tests
// OPEN the following tests are rather integration test (but they are also needed)
TEST(simpleThreadPoolTest, tasksAreNotExecutedWhenNotStarted)
{
    SimpleThreadPool thPool(4);
    using boolType = std::atomic_bool;
    std::array<boolType, 6> executed{false, false, false, false, false, false};

    for (auto& item : executed)
    {
        auto method = [](boolType& methodExecuted) { methodExecuted = true; };

        thPool.schedule(std::move(method), std::ref(item));
    }

    // Let's wait some time to prove that the tasks will not be executed
    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (auto& item : executed)
    {
        ASSERT_FALSE(item);
    }
}

TEST(simpleThreadPoolTest, stopWillWaitForAlreadyExecutingTasksToFinish)
{
    // TODO
    ASSERT_TRUE(false);
}

TEST(simpleThreadPoolTest, scheduledTasksAreNotExecutedAfterStop)
{
    // TODO
    ASSERT_TRUE(false);
}

TEST(simpleThreadPoolTest, tasksAreScheduledWhenStarted)
{
    SimpleThreadPool thPool(4);
    using boolType = std::atomic_bool;
    std::array<boolType, 6> executed{false, false, false, false, false, false};

    for (auto& item : executed)
    {
        auto method = [](boolType& methodExecuted) { methodExecuted = true; };

        thPool.schedule(std::move(method), std::ref(item));
    }

    thPool.start();
    using Clock = std::chrono::high_resolution_clock;
    const auto start = Clock::now();

    // Wait fro the methods to be executed with an timeout
    while ((Clock::now() - start) <= std::chrono::seconds(60))
    {
        bool done{true};
        for (auto& item : executed)
        {
            done = done && item;
        }

        if (done)
            break;
    };

    for (auto& item : executed)
    {
        ASSERT_TRUE(item);
    }
}

TEST(simpleThreadPoolTest, tasksAreExecutedOnMultipleThreads)
{
    constexpr std::size_t threadCount{4};
    SimpleThreadPool thPool(threadCount);

    struct Wait
    {
        std::thread::id id;
        std::mutex mutex;
        std::condition_variable cv;
    };

    std::array<Wait, threadCount> waits{};

    for (auto& item : waits)
    {
        auto method = [&](Wait& wait) {
            std::unique_lock<std::mutex> lk(wait.mutex);
            wait.id = std::this_thread::get_id();
            // The thread needs to be kept busy for some time
            wait.cv.wait(lk);
        };

        thPool.schedule(std::move(method), std::ref(item));
    }

    thPool.start();

    using Clock = std::chrono::high_resolution_clock;
    const auto start = Clock::now();
    // Wait fro the methods to be started with an timeout
    while ((Clock::now() - start) <= std::chrono::seconds(60))
    {
        bool done{true};
        for (auto& item : waits)
        {
            std::lock_guard<std::mutex> lk(item.mutex);
            done = done && item.id != std::thread::id();
        }

        if (done)
            break;
    };

    using MapType = std::unordered_map<std::thread::id, size_t>;
    MapType count;
    // All methods were executed
    for (auto& item : waits)
    {
        std::lock_guard<std::mutex> lk(item.mutex);

        ASSERT_NE(std::thread::id(), item.id);
        count[item.id]++;
        // let the task finish
        item.cv.notify_one();
    }

    // The methods were executed at least on two different threads
    ASSERT_LT(static_cast<MapType::size_type>(1), count.size());
}

namespace
{
    class TestException : public std::exception
    {
        const char* what() const noexcept override
        {
            return "Test exception.\n";
        }
    };
}

TEST(simpleThreadPoolTest, exceptionsArePropagated)
{
    constexpr std::size_t taskCount{6};

    SimpleThreadPool thPool(4);
    using boolType = std::atomic_bool;
    std::array<boolType, taskCount> executed{false, false, false, false, false, false};

    for (auto& item : executed)
    {
        auto method = [](boolType& methodExecuted) {
            methodExecuted = true;
            throw TestException();
        };

        thPool.schedule(std::move(method), std::ref(item));
    }

    thPool.start();

    using Clock = std::chrono::high_resolution_clock;
    const auto start = Clock::now();

    // Wait fro the methods to be executed with an timeout
    while ((Clock::now() - start) <= std::chrono::seconds(60))
    {
        bool done{true};
        for (auto& item : executed)
        {
            done = done && item;
        }

        if (done)
            break;
    };

    thPool.stop();
    const auto except = thPool.popExceptions();

    ASSERT_EQ(taskCount, except.size());
    for (auto& item : except)
    {
        ASSERT_THROW(std::rethrow_exception(item), TestException);
    }
}

TEST(simpleThreadPoolTest, exceptionsWillNotLowerTheNumberOfThreadsInPool)
{
    constexpr std::size_t threadCount{4};

    SimpleThreadPool thPool(threadCount);
    struct Wait
    {
        Wait()
            : started{false}
        {
        }

        bool started;
        std::mutex mutex;
        std::condition_variable cv;
    };
    std::array<Wait, threadCount> waits{};

    for (auto& item : waits)
    {
        auto method = [](Wait& wait) {
            {
                std::unique_lock<std::mutex> lk(wait.mutex);
                wait.started = true;
                wait.cv.wait(lk);
            }

            throw TestException();
        };

        thPool.schedule(std::move(method), std::ref(item));
    }

    thPool.start();

    using Clock = std::chrono::high_resolution_clock;
    const auto start = Clock::now();
    // Wait fro the methods to be executed with an timeout
    while ((Clock::now() - start) <= std::chrono::seconds(60))
    {
        bool done{true};
        for (auto& item : waits)
        {
            std::lock_guard<std::mutex> lk(item.mutex);
            done = done && item.started;
        }

        if (done)
            break;
    };

    // All threads in the pool are occupied, lets throw exceptions
    for (auto& item : waits)
    {
        std::lock_guard<std::mutex> lk(item.mutex);

        ASSERT_TRUE(item.started);
        item.cv.notify_one();
    }

    std::array<Wait, threadCount> secondWaits{};

    for (auto& item : secondWaits)
    {
        auto method = [](Wait& wait) {
            {
                std::unique_lock<std::mutex> lk(wait.mutex);
                wait.started = true;
                wait.cv.wait(lk);
            }
        };

        thPool.schedule(std::move(method), std::ref(item));
    }

    const auto secondStart = Clock::now();
    // Wait fro the methods to be executed with an timeout
    while ((Clock::now() - secondStart) <= std::chrono::seconds(60))
    {
        bool done{true};
        for (auto& item : secondWaits)
        {
            std::lock_guard<std::mutex> lk(item.mutex);
            done = done && item.started;
        }

        if (done)
            break;
    };

    // All threads in the pool are occupied and the thread count was not changed
    for (auto& item : secondWaits)
    {
        std::lock_guard<std::mutex> lk(item.mutex);

        ASSERT_TRUE(item.started);
        item.cv.notify_one();
    }

    thPool.stop();
    (void)thPool.popExceptions();
}