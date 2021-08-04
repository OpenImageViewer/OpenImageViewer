#include "System.h"
#include <LLUtils/Exception.h>
namespace OIV
{
	uint32_t System::GetIdealNumThreadsForMemoryOperations()
	{
		auto cpuCoresInfo = LLUtils::PlatformUtility::GetCPUCoresInfo();

		if (cpuCoresInfo.logicalCores > cpuCoresInfo.physicalCores)
		{
			if (cpuCoresInfo.physicalCores * 2 == cpuCoresInfo.logicalCores)
			{
				// with hyper threading it seems like resampling works best on 75% of the virtual/ logical cores.
				return static_cast<uint32_t>(cpuCoresInfo.physicalCores * 1.5);
			}
			else
			{
				LL_EXCEPTION_NOT_IMPLEMENT("Unknown CPU cores layout");
			}
		}
		else
		{
			return cpuCoresInfo.physicalCores; // no hyper threading.
		}
	}
}