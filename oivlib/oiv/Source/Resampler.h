#pragma once
#include <cstdint>
#include <vector>
#include <thread>
namespace std
{
	class thread;
}


struct ResamplerBox
{
	int32_t top;
	int32_t left;
	int32_t bottom;
	int32_t right;
};

struct ResamplerParams
{
	uint32_t* targetBuffer;
	uint32_t targetWidth;
	uint32_t targetHeight;
	const uint32_t* sourceBuffer;
	uint32_t sourceWidth;
	uint32_t sourceHeight;
};

struct AverageParams
{
	const uint32_t* imageBuffer;
	size_t ImageWidth;
	size_t ImageHeight;
	size_t ImageX;
	size_t ImageY;
	ResamplerBox box;
};


struct ResampleTask
{
	size_t TaskID;
	ResamplerParams resampleParams;
	ResamplerBox box;
	size_t totalTargetTexels;
	double ratioX;
	double ratioY;
	size_t totalThreads;


#if RESAMPLE_THREAD_POOL
	std::condition_variable* cv;
	std::mutex* mtx;
	std::function<void()>* Callback;
#endif
};


class Resampler
{

public:
	void Resample(const ResamplerParams& params);
private: // memeber functions
	void Init();
	uint32_t GetAverageAt(const AverageParams& params);
	void ResampleThreadEntryPoint(ResampleTask* task);
	uint32_t GetIdealNumThreadsForResampling();


private: // memeber fields
	std::vector<std::thread> fThreads;
	std::vector<ResampleTask> fTasks;
	uint32_t fNumOfIdealThreadsForResampling = 1;
	bool fInitialized = false;
};