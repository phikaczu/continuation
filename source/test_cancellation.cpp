#include <gtest\gtest.h>
#include "cancellation_source.h"

#include <vector>

TEST(cancelationSource, cancelationSignalization)
{
    const std::size_t tokensCount{5};
    CancellationSource cancelSource;

    std::vector<CancellationToken> tokens;
    for (std::size_t idx = 0; idx < tokensCount; ++idx)
    {
        tokens.push_back(cancelSource.get_token());
    }

    for (auto& token : tokens)
    {
        ASSERT_FALSE(token.is_canceled());
    }

    cancelSource.cancel();

    for (auto& token : tokens)
    {
        ASSERT_TRUE(token.is_canceled());
    }
}