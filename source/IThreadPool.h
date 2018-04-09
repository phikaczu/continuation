#pragma once

#include <functional>
#include <memory>
#include "mbind.h"

class IThreadPool
{
public:
    virtual ~IThreadPool() = default;

    template <typename Function, typename... Args>
    void schedule(Function&& f, Args&&... args);

protected:
    using MethodType = std::unique_ptr<mbind>;
    virtual void scheduleInner(MethodType&& method) = 0;
};

template <typename Function, typename... Args>
void IThreadPool::schedule(Function&& f, Args&&... args)
{
    MethodType method = Bind::bind(std::forward<Function>(f), std::forward<Args>(args)...);
    scheduleInner(std::move(method));
}
