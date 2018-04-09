#pragma once

#include <exception>

class CanceledException final : public std::exception
{
};