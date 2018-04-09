#pragma once

#include <atomic>

class CancellationSource;

class CancellationToken final
{
    friend CancellationSource;

public:
    bool is_canceled() const noexcept;

private:
    explicit CancellationToken(const std::atomic_bool& canceled);

    const std::atomic_bool& _canceled;
};