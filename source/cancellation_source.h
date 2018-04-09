#pragma once

#include <atomic>
#include "cancellation_token.h"

class CancellationSource final
{
public:
    CancellationSource();

    void cancel() noexcept;
    CancellationToken get_token() const;

private:
    std::atomic_bool _canceled;
};