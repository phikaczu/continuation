#include "cancellation_token.h"

bool CancellationToken::is_canceled() const noexcept
{
    return _canceled;
}

CancellationToken::CancellationToken(const std::atomic_bool& canceled)
    : _canceled(canceled)
{
}