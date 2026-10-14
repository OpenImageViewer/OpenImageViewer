#pragma once
#include <expected>
namespace OIV
{
    enum class NavigationError
    {
        EmptyData,
        OutOfRange,
        FileNotFound
    };
    using NavigationResult = std::expected<void, NavigationError>;
}  // namespace OIV
