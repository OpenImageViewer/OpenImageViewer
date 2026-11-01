#include "System.h"
#include "SystemThreadPolicy.h"

#include <LLUtils/PlatformUtility.h>

#include <algorithm>

namespace OIV::internal
{
    uint32_t CalculateIdealNumThreadsForMemoryOperations(uint32_t physicalCores, uint32_t logicalCores) noexcept
    {
        const uint32_t availableCores = logicalCores != 0 ? logicalCores : physicalCores;
        uint32_t idealThreads         = 1;
        if (availableCores != 0)
        {
            const uint32_t usablePhysicalCores    = physicalCores != 0 ? std::min(physicalCores, availableCores)
                                                                       : availableCores;
            const uint32_t threeQuartersOfLogical = static_cast<uint32_t>(static_cast<uint64_t>(availableCores) * 3 /
                                                                          4);
            idealThreads                          = availableCores == usablePhysicalCores
                                                        ? availableCores
                                                        : std::max(usablePhysicalCores, threeQuartersOfLogical);
        }
        return idealThreads;
    }
}  // namespace OIV::internal

namespace OIV
{
    uint32_t System::GetIdealNumThreadsForMemoryOperations()
    {
        const auto cpuCoresInfo = LLUtils::PlatformUtility::GetCPUCoresInfo();
        return internal::CalculateIdealNumThreadsForMemoryOperations(cpuCoresInfo.physicalCores,
                                                                     cpuCoresInfo.logicalCores);
    }
}  // namespace OIV
