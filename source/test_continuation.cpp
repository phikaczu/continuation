#include <gtest\gtest.h>

#include <array>
#include <atomic>
#include <chrono>

#include "SimpleThreadPool.h"
#include "canceled_exception.h"
#include "cancellation_source.h"
#include "continuation_task.h"

TEST(continuationTest, basicAssumptions)
{
    auto promise = std::make_unique<std::promise<int>>();
    auto future = promise->get_future();

    promise->set_value(3);
    promise.reset();

    const auto result = future.get();
    ASSERT_EQ(3, result);
}

TEST(continuationTest, basicAssumptions2)
{
    std::promise<int> promise;
    auto future = promise.get_future();

    std::thread th([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        promise.set_value(1);
    });

    const auto result = future.get();
    ASSERT_EQ(1, result);
    th.join();
}

TEST(continuationTest, taskWillBeExecuted)
{
    std::atomic_bool executed{false};

    SimpleThreadPool thPool(2);
    thPool.start();

    auto task = [&]() { executed.store(true); };

    ContinuationTask continuation(thPool, std::move(task));
    auto& future = continuation.get_future();

    const auto waitResult = future.wait_for(std::chrono::seconds(60));

    ASSERT_EQ(std::future_status::ready, waitResult);
    ASSERT_TRUE(executed.load());
}

namespace
{
    using size_type = std::size_t;
    static constexpr size_type items{12};

    template <class T>
    using Container = std::array<T, items>;

    using Clock = std::chrono::high_resolution_clock;
    using TimeUnit = std::chrono::milliseconds;
}

// There are several meanings of chaining:
// * tasks are executed after each other without an overlap (this test does not cover this case properly)
// * the intermediate continuations instances do not need to be referenced from the code
TEST(continuationTest, properChaining)
{
    SimpleThreadPool thPool(4);
    thPool.start();

    Container<std::chrono::milliseconds> taskSleep{TimeUnit{250},
                                                   TimeUnit{500},
                                                   TimeUnit{250},
                                                   TimeUnit{750},
                                                   TimeUnit{500},
                                                   TimeUnit{250},
                                                   TimeUnit{150},
                                                   TimeUnit{500},
                                                   TimeUnit{200},
                                                   TimeUnit{650},
                                                   TimeUnit{0},
                                                   TimeUnit{250}};
    Container<Clock::time_point> results{};

    size_type idx{0};
    auto first = [&, idx]() {
        std::this_thread::sleep_for(taskSleep[idx]);
        results[idx] = Clock::now();
    };

    ContinuationTask task(thPool, std::move(first));
    ++idx;

    for (; idx < items; ++idx)
    {
        auto method = [&, idx]() {
            std::this_thread::sleep_for(taskSleep[idx]);
            results[idx] = Clock::now();
        };

        task = task.continue_with(std::move(method));
    }

    task.get_future().wait();

    for (decltype(results.size()) idx = 1; idx < results.size(); ++idx)
    {
        const auto& first = results.at(idx - 1);
        const auto& second = results.at(idx);
        const auto taskDuration = std::chrono::duration_cast<TimeUnit>(second - first);
        const auto taskAbsoluteDuration = std::chrono::duration_cast<TimeUnit>(second - results.at(0));

        std::cout << "Task duration: set " << taskSleep.at(idx).count() << " ms, actual " << taskDuration.count() << " ms, duration "
                  << taskAbsoluteDuration.count() << std::endl;

        ASSERT_LT(first, second) << "Time points of task execution are not increasing for task indexes " << idx - 1 << " and " << idx;
        ASSERT_GE(taskDuration, taskSleep.at(idx)) << "Task with index " << idx << " was executed faster then expected";
    }
}

TEST(continuationTest, fulfiledFutureComnstructor)
{
    // Thread pool will not be started - it is not needed. So 0 threads can be requested.
    SimpleThreadPool thPool(0);
    ContinuationTask task(thPool);

    ASSERT_EQ(std::future_status::ready, task.get_future().wait_for(std::chrono::seconds(0)));
}

TEST(continuationTest, abandonedContinuation)
{
    std::mutex cvMtx;
    std::condition_variable cv;
    bool wakeUp = false;

    SimpleThreadPool thPool(1);
    thPool.start();
    {
        auto method = [&]() {
            std::unique_lock<std::mutex> lk(cvMtx);
            cv.wait(lk, [&]() { return wakeUp; });
        };

        ContinuationTask task(thPool, std::move(method));
    }

    {
        std::lock_guard<std::mutex> lk(cvMtx);
        wakeUp = true;
        cv.notify_one();
    }

    auto exceptions = thPool.popExceptions();
    ASSERT_TRUE(exceptions.empty());
}

namespace
{
    template <typename T>
    decltype(auto) getGuard(T&& method)
    {
        return Guard<std::decay_t<T>>(std::forward<T>(method));
    }

    template <typename T>
    class Guard final
    {
    public:
        template <typename U>
        Guard(U&& method)
            : _method(std::forward<U>(method))
        {
        }

        Guard() = delete;
        Guard(const Guard&) = delete;
        Guard(Guard&&) = default;

        ~Guard()
        {
            _method();
        }

    private:
        T _method;
    };
}

TEST(continuationTest, cancelableTaskScheduled)
{
    CancellationSource cs;
    std::mutex cvMtx, signalMtx, executionMtx;
    std::condition_variable cv, signalCv, executionCv;
    bool executionStarted = false;
    bool cancellationSignalized = false;
    bool terminate = false;
    const auto loopTimeout = std::chrono::milliseconds(100);

    SimpleThreadPool thPool(1);
    thPool.start();

    auto guard = getGuard([&]() {
        std::lock_guard<std::mutex> lk(cvMtx);
        terminate = true;
        cv.notify_one();
    });

    auto method = [&](CancellationToken ct) {
        {
            std::lock_guard<std::mutex> lk(executionMtx);
            executionStarted = true;
            executionCv.notify_one();
        }

        bool continueLoop = true;
        while (continueLoop)
        {
            {
                std::unique_lock<std::mutex> lk(cvMtx);
                continueLoop = !cv.wait_for(lk, loopTimeout, [&]() { return terminate || ct.is_canceled(); });
            }

            if (ct.is_canceled())
            {
                std::lock_guard<std::mutex> lk(signalMtx);
                cancellationSignalized = true;
                signalCv.notify_one();
            }
        }
    };

    ContinuationTask task(thPool, std::move(method), cs.get_token());

    // wait for the method to be executed on the thread
    {
        std::unique_lock<std::mutex> lk(executionMtx);
        const auto executed = executionCv.wait_for(lk, std::chrono::seconds(30), [&]() { return executionStarted; });

        ASSERT_TRUE(executed) << "execution of the task did not start";
    }

    cs.cancel();
    {
        std::unique_lock<std::mutex> lk(signalMtx);
        signalCv.wait_for(lk, 10 * loopTimeout);
        ASSERT_TRUE(cancellationSignalized) << "cancellation was not signaled";
    }

    ASSERT_EQ(std::future_status::ready, task.get_future().wait_for(std::chrono::seconds(30)));

    // It is up to the scheduled method to set the exception if it terminates prematurely! Note that the task could already finish normally.
    // ASSERT_THROW(task.get_future().get(), CanceledException);
}

TEST(continuationTest, cancelableTaskNotScheduled)
{
    CancellationSource cs;
    SimpleThreadPool thPool(1);

    auto method = []() {};

    ContinuationTask task(thPool, std::move(method), cs.get_token());

    cs.cancel();
    thPool.start();
    thPool.stop();

    ASSERT_THROW(task.get_future().get(), CanceledException);
}