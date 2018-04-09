#include "cancellation_source.h"

CancellationSource::CancellationSource()
    : _canceled{false}
{
}

void CancellationSource::cancel() noexcept
{
    _canceled = true;
}

CancellationToken CancellationSource::get_token() const
{
    return CancellationToken(_canceled);
}
