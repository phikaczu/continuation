#pragma once

#include <cassert>
#include <memory>
#include <tuple>

class mbind
{
public:
    virtual ~mbind() = default;
    virtual void operator()() = 0;
};

class Bind;

template <typename Function, typename... Args>
class fbind final : public mbind
{
    friend Bind;

private:
    template <typename... T>
    using Storage = std::tuple<std::decay_t<T>...>;

    fbind(Function&& function, Args&&... args);

public:
    void operator()() override;

private:
    bool _executed;
    Function _function;
    Storage<Args&&...> _args;
};

class Bind final
{
public:
    Bind() = delete;

    template <typename Function, typename... Args>
    static std::unique_ptr<mbind> bind(Function&& function, Args&&... args);
};

template <typename Function, typename... Args>
fbind<Function, Args...>::fbind(Function&& function, Args&&... args)
    : _executed{false}
    , _function(std::move(function))
    , _args(std::forward<Args>(args)...)
{
}

template <typename Function, typename... Args>
void fbind<Function, Args...>::operator()()
{
    assert(!_executed);
    std::apply(_function, std::move(_args));
    _executed = true;
}

template <typename Function, typename... Args>
std::unique_ptr<mbind> Bind::bind(Function&& function, Args&&... args)
{
    return std::unique_ptr<mbind>(new fbind<Function, Args...>(std::forward<Function>(function), std::forward<Args>(args)...));
}
