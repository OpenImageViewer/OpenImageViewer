#pragma once

#include <cstdint>

namespace OIV::internal
{
    [[nodiscard]] uint32_t CalculateIdealNumThreadsForMemoryOperations(uint32_t physicalCores,
                                                                       uint32_t logicalCores) noexcept;
}
