#include <gtest\gtest.h>
#include "mbind.h"

TEST(mbindtest, general)
{
    const int expectedNumber{10};
    // Some non-copyable type is needed
    auto pNumber = std::make_unique<int>(expectedNumber);
    int actualNumber{0};

    auto method = [](std::unique_ptr<int>&& input, int& output) { output = *input; };

    // Note that std::ref for actualNumber is not needed, the parameters are captured by l-value
    // or r-value references
    auto bindedMethod = Bind::bind(std::move(method), std::move(pNumber), std::ref(actualNumber));
    (*bindedMethod)();

    ASSERT_EQ(expectedNumber, actualNumber);
}