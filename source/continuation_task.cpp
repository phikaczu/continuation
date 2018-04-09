#include "continuation_task.h"

#include <exception>
#include <mutex>
#include <queue>
#include "canceled_exception.h"
#include "cancellation_source.h"

namespace
{
    class PromiseMethod
    {
    public:
        PromiseMethod();
        explicit PromiseMethod(ContinuationTask::TaskMethod&& method);

        void operator()() noexcept;
        ContinuationTask::Future get_future();
        void cancel();

    private:
        ContinuationTask::TaskMethod _method;
        ContinuationTask::Promise _promise;
    };

    PromiseMethod::PromiseMethod()
    {
        _promise.set_value();
    }

    PromiseMethod::PromiseMethod(ContinuationTask::TaskMethod&& method)
        : _method(std::move(method))
    {
    }

    void PromiseMethod::operator()() noexcept
    {
        try
        {
            // TODO this works only when the return type is void
            _method();
            _promise.set_value();
        }
        catch (...)
        {
            _promise.set_exception(std::current_exception());
        }
    }

    ContinuationTask::Future PromiseMethod::get_future()
    {
        return _promise.get_future();
    }

    void PromiseMethod::cancel()
    {
        try
        {
            throw CanceledException();
        }
        catch (...)
        {
            _promise.set_exception(std::current_exception());
        }
    }

    CancellationToken getDummyToken()
    {
        static CancellationSource source;

        return source.get_token();
    }
}

class ContinuationTask::Impl final
{
private:
    using MethodContainer = std::queue<std::shared_ptr<Impl>>;

public:
    Impl(IThreadPool& thPool, CancellationToken&& cancellation);
    Impl(IThreadPool& thPool, TaskMethod&& method, CancellationToken&& cancellation);
    Impl(std::shared_ptr<Impl> parent, TaskMethod&& method);

    static void scheduleNow(std::shared_ptr<ContinuationTask::Impl> task);

    ContinuationTask continue_with(std::shared_ptr<Impl> parent, TaskMethod&& method);
    void schedule(std::shared_ptr<Impl>& task);
    Future& get_future();

private:
    static void threadMethod(std::shared_ptr<ContinuationTask::Impl> task) noexcept;

    IThreadPool& _thPool;
    std::shared_ptr<Impl> _parent;
    PromiseMethod _method;
    MethodContainer _childs;
    std::mutex _scheduleLock;
    Future _future;
    CancellationToken _cancellation;
};

ContinuationTask::Impl::Impl(IThreadPool& thPool, CancellationToken&& cancellation)
    : _thPool(thPool)
    , _parent()
    , _method()
    , _future(_method.get_future())
    , _cancellation(cancellation)  // relevant only for children
{
}

ContinuationTask::Impl::Impl(IThreadPool& thPool, TaskMethod&& method, CancellationToken&& cancellation)
    : _thPool(thPool)
    , _parent()
    , _method(std::move(method))
    , _future(_method.get_future())
    , _cancellation(cancellation)
{
}

ContinuationTask::Impl::Impl(std::shared_ptr<Impl> parent, TaskMethod&& method)
    : _thPool(parent->_thPool)
    , _parent(std::move(parent))
    , _method(std::move(method))
    , _future(_method.get_future())
    , _cancellation(_parent->_cancellation)
{
}

ContinuationTask ContinuationTask::Impl::continue_with(std::shared_ptr<Impl> parent, TaskMethod&& method)
{
    auto childImpl = std::make_shared<Impl>(std::move(parent), std::move(method));
    ContinuationTask child(childImpl);
    {
        std::lock_guard<std::mutex> lk(_scheduleLock);
        schedule(childImpl);
    }

    return child;
}

void ContinuationTask::Impl::schedule(std::shared_ptr<Impl>& task)
{
    if (get_future().wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        scheduleNow(task);
    }
    else
    {
        _childs.push(task);
    }
}

ContinuationTask::Future& ContinuationTask::Impl::get_future()
{
    return _future;
}

void ContinuationTask::Impl::scheduleNow(std::shared_ptr<ContinuationTask::Impl> task)
{
    if (task->_cancellation.is_canceled())
    {
        task->_method.cancel();
    }
    else
    {
        auto& thPool = task->_thPool;
        // Someone needs to hold the task instance till the threadMethod finishes
        // so the shared_ptr<Impl> is given as argument
        thPool.schedule(&ContinuationTask::Impl::threadMethod, std::move(task));
    }
}

void ContinuationTask::Impl::threadMethod(std::shared_ptr<ContinuationTask::Impl> task) noexcept
{
    try
    {
        auto method = std::move(*task)._method;

        // TODO will provide a result of the previous task, if the parent is canceled, than an exception will be stored as the future result
        task->_parent.reset();

        if (task->_cancellation.is_canceled())
        {
            method.cancel();
        }
        else
        {
            method();
        }

        {
            std::lock_guard<std::mutex> lk(task->_scheduleLock);
            while (!task->_childs.empty())
            {
                task->scheduleNow(std::move(task->_childs.front()));
                task->_childs.pop();
            }
        }
    }
    catch (...)
    {
        // TODO
        // What if the exception belongs to the future, i.e. is of std::future_error type?
        // Moreover the promise value could be already set, so it is illegal to set also the exception

        // TODO handle exceptions -> does a possible exception belong to the
        // methods promise?
        //
        //_promise.set_exception(std::current_exception());
    }
}

CancellationToken ContinuationTask::_dummyToken = getDummyToken();

ContinuationTask::ContinuationTask(IThreadPool& thPool, CancellationToken cancellation /* = _dummyToken*/)
    : _pImpl(std::make_shared<Impl>(thPool, std::move(cancellation)))
{
}

ContinuationTask::ContinuationTask(IThreadPool& thPool, TaskMethod&& method, CancellationToken cancellation /* = _dummyToken*/)
    : _pImpl(std::make_shared<Impl>(thPool, std::move(method), std::move(cancellation)))
{
    // OPEN or inherit Impl class from std::enable_shared_from_this
    Impl::scheduleNow(_pImpl);
}

ContinuationTask::ContinuationTask(IThreadPool& thPool, CancelableTaskMethod&& method, CancellationToken cancellation /* = _dummyToken*/)
    : _pImpl(std::make_shared<Impl>(thPool, std::bind(std::move(method), cancellation), std::move(cancellation)))
{
    // OPEN or inherit Impl class from std::enable_shared_from_this
    Impl::scheduleNow(_pImpl);
}

ContinuationTask::ContinuationTask(std::shared_ptr<Impl> sharedState)
    : _pImpl(std::move(sharedState))
{
    // Scheduling will be done by the caller
}

ContinuationTask ContinuationTask::continue_with(TaskMethod&& method)
{
    return _pImpl->continue_with(_pImpl, std::move(method));
}

ContinuationTask::Future& ContinuationTask::get_future()
{
    return _pImpl->get_future();
}
