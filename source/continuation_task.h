#pragma once

#include <future>
#include <memory>
#include "IThreadPool.h"
#include "cancellation_token.h"

class ContinuationTask final
{
private:
    class Impl;

public:
    using TaskMethod = std::function<void()>;
    using CancelableTaskMethod = std::function<TaskMethod::result_type(CancellationToken)>;
    using Future = std::future<TaskMethod::result_type>;
    using Promise = std::promise<TaskMethod::result_type>;

    /**
     * Creates a new instance with a fulfilled future.
     * @param thPool thread pool to be used for task scheduling
     * @param cancellation token for canceling this task
     * @note The @p thPool instance needs to stay alive as long as this instance and all instances created by the
     * ContinuationTask::continue_with(TaskMethod&&) method are alive.
     */
    ContinuationTask(IThreadPool& thPool, CancellationToken cancellation = _dummyToken);

    /**
     * Creates a new instance.
     * @param thPool thread pool to be used for task scheduling
     * @param method task to be executed on the thread pool
     * @param cancellation token for canceling this task
     * @note The @p thPool instance needs to stay alive as long as this instance and all instances created by the
     * ContinuationTask::continue_with(TaskMethod&&) method are alive.
     */
    ContinuationTask(IThreadPool& thPool, TaskMethod&& method, CancellationToken cancellation = _dummyToken);

    /**
     * Creates a new instance.
     * @param thPool thread pool to be used for task scheduling
     * @param method cancelable task to be executed on the thread pool
     * @param cancellation token for canceling this task
     * @note The @p thPool instance needs to stay alive as long as this instance and all instances created by the
     * ContinuationTask::continue_with(TaskMethod&&) method are alive.
     */
    ContinuationTask(IThreadPool& thPool, CancelableTaskMethod&& method, CancellationToken cancellation = _dummyToken);

private:
    ContinuationTask(std::shared_ptr<Impl> sharedState);

public:
    /**
     * Schedules a new task for execution after the task represented by this instance is finished.
     * @param method task to be executed on the thread pool
     * @returns A new continuation instance representing the new task.
     */
    ContinuationTask continue_with(TaskMethod&& method);

    /**
     * @returns A future that will be fulfilled by the task.
     */
    Future& get_future();

private:
    static CancellationToken _dummyToken;

    std::shared_ptr<Impl> _pImpl;
};