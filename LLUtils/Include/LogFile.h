#pragma once
#include <mutex>
#include "FileSystemHelper.h"

#include "FileHelper.h"
#include "PlatformUtility.h"

namespace LLUtils
{
	class LogFile
	{
	public:
		LogFile(std::wstring logPath, bool clear)
		{
			mLogPath = logPath;
			FileSystemHelper::EnsureDirectory(mLogPath);
			if (clear == true)
				std::filesystem::remove(logPath);
		}
		 void Log(std::wstring message)
		{
			if (mLogPath.empty() == false)
			{
				std::lock_guard<std::mutex> lock(mWriteMutex);
				File::WriteAllText(mLogPath, message, true);
			}
		}
	private:
		std::wstring mLogPath;
		std::mutex mWriteMutex;
	};
}
